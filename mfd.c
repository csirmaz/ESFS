/*
  This file is part of ESFS, a FUSE-based filesystem that supports snapshots.
  ESFS is Copyright (C) 2013 Elod Csirmaz
  <http://www.epcsirmaz.com/> <https://github.com/csirmaz>.

  ESFS is based on Big Brother File System (fuse-tutorial)
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>,
  and was forked from it on 21 August 2013.
  Big Brother File System can be distributed under the terms of
  the GNU GPLv3. See the file COPYING.
  See also <http://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/>.

  Big Brother File System was derived from function prototypes found in
  /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  fuse.h is licensed under the LGPLv2.

  ESFS is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.

  ESFS is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along
  with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * NOTE: A Perl script is used to replace $ with esfs_ and $$ with ESFS_
 * in this file. To write $, use \$.
 */

/* This file contains functions related to snapshot files and
 * changes to the dentries of main files.
 *
 * Files in snapshots
 * ==================
 *
 * Each snapshot acts as an overlay over the files in the main space,
 * or that combined with later snapshots.
 * Each file has its own overlay, at a path that mirrors its original
 * location. It is stored in two files, in a .map and a .dat file.
 *
 * The .map file contains a list of pointers for each block in the file
 * pointing to blocks in the .dat file, which stores data in those blocks
 * that have been modified since the creation of the snapshot. If this
 * pointer is non-0, the block need not be saved again.
 *
 * All original dentry information about a file is saved in the header
 * of the .map file, including its size.
 *
 * If a file did not exist when the snapshot was taken, this fact
 * is saved in the header of the .map file.
 *
 * The presence of the .map file means that the file in the main space is
 * already dirty, and dentry changes need not be saved again in the snapshot.
 *
 * Renames
 * =======
 *
 * Suppose files A and B have already been modified since the last snapshot:
 *
 * A.dat    A(A)
 * A.map
 *
 * B.dat    B(B)
 * B.map
 *
 * Now B is renamed to C.
 * We don't want to handle it as delete(B) and create(C).
 * Then we need to know that when we write C, any old blocks need to be saved
 * in B.dat. This is saved in the header of C.map (in addition to anything
 * else that might have already been there!).
 *
 * A.dat            A(A)
 * A.map
 *
 * B.dat
 * B.map
 *
 * C.map:write B    C(B)
 *
 * When we want to read B from the snapshot, we also need to know that
 * some of the data might be in C (going forward, if this is not
 * the latest snapshot):
 *
 * A.dat
 * A.map           A(A)
 *
 * B.dat
 * B.map:read C
 *
 * C.map:write B   C(B)
 *
 * If C(B) is renamed again:
 *
 * A.dat
 * A.map           A(A)
 *
 * B.dat
 * B.map:read D
 *
 * C.map
 *
 * D.map:write B   D(B)
 *
 * When opening the map file, follow the "write" directive.
 * On subsequent renames, update the read directive in the map.
 * On rename/delete, remove the write directive.
 *
 * Reading a snapshot
 * ==================
 *
 * Although a snapshot can be thought of as an overlay, reading
 * starts at the snapshot.
 *
 * Get the map file.
 * If there is no map file, or it is too short, or it contains
 * 0 for the block, go to the next snapshot (forward in time).
 * But before doing so, if there is a "read" directive in the map,
 * switch the path.
 *
 * File stats come from the first map file, so "read" directives
 * play no role there.
 *
 * Ultimately, the main file can be used.
 */


/** Write the map header to the map file
 *
 * Returns:
 * * 0 on success
 * * -errno on error
 */
// TODO 2 If this fails, the filesystem may be broken
static inline int $mfd_save_mapheader(const struct $mfd_t *mfd, const struct $fsdata_t *fsdata)
{
   int ret;

   ret = pwrite(mfd->mapfd, &(mfd->mapheader), sizeof(struct $mapheader_t), 0);
   if(unlikely(ret == -1)) {
      ret = errno;
      $dlogi("mfd_open_sn: Failed to write .map header, error %d = %s\n", ret, strerror(ret));
      return -ret;
   }

   if(unlikely(ret != sizeof(struct $mapheader_t))) {
      $dlogi("mfd_open_sn: Failed: only written %d bytes into .map header\n", ret);
      return -EIO;
   }

   return 0;
}


// breaks below are not errors, but we want to skip opening/creating the dat file
// if the file was empty or nonexistent when the snapshot was taken
#define $$MFD_OPEN_DAT_FILE \
            if(maphead->exists == 0){ \
               mfd->datfd = -3; \
               break; \
            } \
            if(unlikely(maphead->fstat.st_size == 0)){ \
               mfd->datfd = -4; \
               break; \
            } \
            fd_dat = open(fdat, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU); \
            if(fd_dat == -1){ \
               fd_dat = errno; \
               $dlogi("mfd_open_sn: Failed to open .dat at %s, error %d = %s (1)\n", fdat, fd_dat, strerror(fd_dat)); \
               waserror = fd_dat; \
               break; \
            } \
            $dlogdbg("mfd_open_sn: Opened dat file at %s FD %d\n", fdat, fd_dat); \
            mfd->datfd = fd_dat;

/** Opens (and initialises) the snapshot part of an MFD
 *
 * This is done by
 * opening (and creating and initialising, if necessary) the .map and .dat files.
 * Call this before modifying any file, including renaming it.
 *
 * Sets:
 * * mfd->mapfd, the map file opened for RDWR or a negative value if unused -- see types.h
 * * mfd->datfd, the dat file opened for WR|APPEND or a negative value if unused -- see types.h
 * * mfd->is_renamed, 0 or 1
 * * mfd->mapheader
 *
 * Saves:
 * * stats of the file into the map file, unless the map file already exists.
 *
 * Returns:
 * * 0 - on success
 * * -errno - on failure
 */
static int $mfd_open_sn(
   struct $mfd_t *mfd,
   const char *vpath, /**< the virtual path in the main space */
   const char *fpath,
   const struct $fsdata_t *fsdata,
   int flags /**< $$MFD_DEFAULTS or $$MFD_NOFOLLOW */
)
{
   char fmap[$$PATH_MAX];
   char fdat[$$PATH_MAX];
   int fd; // map file FD
   int fd_dat; // dat file FD
   int ret;
   int waserror = 0; // positive on error
   struct $mapheader_t *maphead;
   $$PATH_LEN_T plen;

   // Pointers
   maphead = &(mfd->mapheader);

   // Default values
   mfd->is_renamed = 0;
   mfd->is_main = 1;

   // No snapshots?
   if(fsdata->sn_is_any == 0) {
      $dlogdbg("mfd_open_sn: no snapshots found, so returning\n");
      mfd->mapfd = -1;
      mfd->datfd = -1;
      return 0;
   }

   // Get the paths of the map & dat files
   $$ADDNPSFIX_CONT(fmap, fdat, vpath, fsdata->sn_lat_dir, fsdata->sn_lat_dir_len, $$EXT_MAP, $$EXT_DAT, $$EXT_LEN)

   // Create path
   ret = $mkpath(fmap, NULL, S_IRWXU); // TODO 1: this is almost thread safe, so decide where to lock
   if(ret < 0) {  // error
      $dlogdbg("mfd_open_sn: mkpath failed with %d = %s\n", -ret, strerror(-ret));
      return ret;
   }

   // Open or create the map file
   fd = open(fmap, O_RDWR | O_CREAT | O_EXCL, S_IRWXU); // TODO 2: add O_NOATIME

   if(fd == -1) {

      fd = errno;
      if(fd == EEXIST) {
         // The .map file already exists, which means that the main file is already dirty.
         // TODO 2 If we aren't actually to set up an mfd, just return here

         // Otherwise, we open the .map file again.
         fd = open(fmap, O_RDWR);
         if(unlikely(fd == -1)) {
            fd = errno;
            $dlogi("mfd_open_sn: Failed to open .map again at %s, error %d = %s\n", fmap, fd, strerror(fd));
            return -fd;
         }
         $dlogdbg("mfd_open_sn: Managed to open .map file again at %s, FD %d\n", fmap, fd);

         mfd->mapfd = fd;

         do { // From here we either return with a positive errno, or -1 if we need to try again

            // Read the mapheader
            ret = pread(fd, maphead, sizeof(struct $mapheader_t), 0);
            if(unlikely(ret == -1)) {
               waserror = errno;
               $dlogi("mfd_open_sn: Failed to read .map at %s, error %d = %s\n", fmap, waserror, strerror(waserror));
               break;
            }
            if(unlikely(ret != sizeof(struct $mapheader_t))) {
               $dlogi("mfd_open_sn: Only read %d bytes instead of %ld from .map at %s. Broken FS?\n", ret, sizeof(struct $mapheader_t), fmap);
               waserror = EIO;
               break;
            }

            // Check the version and the signature
            if(maphead->$version != 10000 || strncmp(maphead->signature, "ESFS", 4) != 0) {
               $dlogi("mfd_open_sn: version or signature bad in map file %s. Broken FS?\n", fmap);
               waserror = EFAULT;
               break;
            }

            // We need to check if there is a "write" directive in here.
            if(maphead->write_v[0] != '\0' && (!(flags & $$MFD_NOFOLLOW))) {
               // Found a write directive, which we need to follow. This is a virtual path.
               $dlogdbg("mfd_open_sn: Found a write directive from %s (map: %s) to %s\n", vpath, fmap, maphead->write_v);
               mfd->is_renamed = 1;
               waserror = -1;
               break;
            }

            // There's no write directive

            // Read information about the file as it was at the time of the snapshot
            // and open or create the dat file if necessary
            $$MFD_OPEN_DAT_FILE

         } while(0);

         if(waserror != 0) {  // Cleanup & follow
            close(fd);
            if(waserror == -1) {  // Follow the write directive
               // Get the full path in fmap
               $$ADDNPREFIX_CONT(fmap, maphead->write_v, fsdata->sn_lat_dir, fsdata->sn_lat_dir_len)
               return $mfd_open_sn(mfd, maphead->write_v, fmap, fsdata, flags);
            }
            return -waserror;
         }

         // Continue below

      } else { // Other error
         $dlogi("mfd_open_sn: Failed to open .map at %s, error %d = %s\n", fmap, fd, strerror(fd));
         return -fd;
      }

   } else { // ----- We've created a new .map file -----

      do {
         // We've created the .map file; let's save data about the main file.
         $dlogdbg("mfd_open_sn: created a new map file at %s FD %d\n", fmap, fd);

         mfd->mapfd = fd;

         // Default values for a new mapheader
         maphead->$version = 10000;
         strncpy(maphead->signature, "ESFS", 4);
         maphead->exists = 1;
         maphead->read_v[0] = '\0';
         maphead->write_v[0] = '\0';

         // stat the main file
         if(lstat(fpath, &(maphead->fstat)) != 0) {
            ret = errno;
            if(ret == ENOENT) {  // main file does not exist (yet?)
               // WARNING In this case, mfd.mapheader.fstat remains uninitialised!
               maphead->exists = 0;
            } else { // some other error
               $dlogi("mfd_open_sn: Failed to stat main file at %s, error %d = %s\n", fpath, ret, strerror(ret));
               waserror = ret;
               break;
            }
         }

         // mfd's are not currently allowed for directories.
         // We rely on this return value to disallow moving/renaming directories
         if(unlikely(S_ISDIR(maphead->fstat.st_mode))) {
            waserror = EISDIR;
            break;
         }

         // write into the map file
         if(unlikely((ret = $mfd_save_mapheader(mfd, fsdata)) != 0)) {
            waserror = -ret;
            break;
         }

         // Read information about the file as it was at the time of the snapshot
         // and open or create the dat file if necessary
         $$MFD_OPEN_DAT_FILE

      } while(0);

      if(waserror != 0) {  // Cleanup
         close(fd);
         // TODO 2: Clean up directories created by $mkpath based on the 'firstcreated' it can return.
         // However, be aware that other files being opened might already be using the directories
         // created, so using $recursive_remove here is not a safe option.
         // TODO 2: Clean up the dat file?
         unlink(fmap);
         return -waserror;
      }

   }

   return 0;
}


/** Marks an MFD as read-only */
static inline void $mfd_open_sn_rdonly(struct $mfd_t *mfd)
{
   mfd->mapfd = -2;
   mfd->datfd = -2;
}


/** Closes the snapshot part of an MFD
 *
 * Returns:
 * * 0 on success
 * * -errno on error (the last errno)
 */
static inline int $mfd_close_sn(struct $mfd_t *mfd)
{
   int waserror = 0;

   if(mfd->datfd >= 0) {
      if(unlikely(close(mfd->datfd) != 0)) { waserror = errno; }
   }

   if(mfd->mapfd >= 0) {
      if(unlikely(close(mfd->mapfd) != 0)) { waserror = errno; }
   }

   return -waserror;
}


/** Initialises the map (and dat) files without leaving them open
 *
 * Returns:
 * * 0 on success
 * * -errno on error
 */
static inline int $mfd_init_sn(
   const char *vpath,
   const char *fpath,
   const struct $fsdata_t *fsdata
)
{
   struct $mfd_t mymfd;
   int ret;

   // It's somewhat wasteful to allocate a whole mfd here,
   // but we need the mapheader, which is included in it, anyway.

   if((ret = $mfd_open_sn(&mymfd, vpath, fpath, fsdata, $$MFD_DEFAULTS)) == 0) {
      ret = $mfd_close_sn(&mymfd);
   }
   return ret;
}
