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


/** Returns if a filename in a snapshot should be presented, and if yes, how.
 *
 * Returns:
 * 0 - if the file is internal, and should not be shown
 * 1 - if the file should be shown (directories)
 * 2 - if the file should be shown, but without the last $$EXT_LEN characters
 */
static inline int $mfd_filter_name(char *name)
{
   $$PATH_LEN_T plen;

   plen = strlen(name);
   if(plen <= $$EXT_LEN) { return 1; }
   name = name + plen - $$EXT_LEN;
   if(strcmp(name, $$EXT_DAT) == 0) { return 0; }
   if(strcmp(name, $$EXT_MAP) == 0) { return 2; }
   return 1;
}


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
      $dlogi("ERROR mfd_save_mapheader: Failed to write .map header, error %d = %s\n", ret, strerror(ret));
      return -ret;
   }

   if(unlikely(ret != sizeof(struct $mapheader_t))) {
      $dlogi("ERROR mfd_save_mapheader: Failed: only written %d bytes into .map header\n", ret);
      return -EIO;
   }

   return 0;
}


/** Load the map header from the map file
 *
 * Returns:
 * * 0 on success
 * * -errno on error
 */
static inline int $mfd_load_mapheader(struct $mapheader_t *maphead, int fd, const struct $fsdata_t *fsdata)
{
   int ret;

   ret = pread(fd, maphead, sizeof(struct $mapheader_t), 0);
   if(unlikely(ret == -1)) {
      ret = errno;
      $dlogi("ERROR Failed to read .map, error %d = %s\n", ret, strerror(ret));
      return -ret;
   }
   if(unlikely(ret != sizeof(struct $mapheader_t))) {
      $dlogi("ERROR Only read %d bytes instead of %ld from .map. Broken FS?\n", ret, sizeof(struct $mapheader_t));
      return -EIO;
   }

   // Check the version and the signature
   if(maphead->$version != 10000 || strncmp(maphead->signature, "ESFS", 4) != 0) {
      $dlogi("ERROR version or signature bad in map file. Broken FS?\n");
      return -EFAULT;
   }

   return 0;
}


// breaks below are not errors, but we want to skip opening/creating the dat file
// if the file was empty or nonexistent when the snapshot was taken
#define $$MFD_OPEN_DAT_FILE \
            if(maphead->exists == 0) { \
               mfd->datfd = $$MFD_FD_ENOENT; \
               break; \
            } \
            if(unlikely(maphead->fstat.st_size == 0)) { \
               mfd->datfd = $$MFD_FD_ZLEN; \
               break; \
            } \
            if($get_dat_prefix_path(fdat, vpath, fsdata->sn_lat_dir, fsdata->sn_lat_dir_len) != 0) { \
               waserror = ENAMETOOLONG; \
               break; \
            } \
            fd_dat = open(fdat, O_WRONLY | O_CREAT | O_APPEND | O_NOATIME, S_IRWXU); \
            if(fd_dat == -1){ \
               fd_dat = errno; \
               $dlogi("ERROR mfd_open_sn: Failed to open .dat at '%s', error %d = %s (1)\n", fdat, fd_dat, strerror(fd_dat)); \
               waserror = fd_dat; \
               break; \
            } \
            $dlogdbg("mfd_open_sn: Opened dat file at '%s' FD '%d' (vpath='%s')\n", fdat, fd_dat, vpath); \
            mfd->datfd = fd_dat;


// MFD open flags
#define $$MFD_DEFAULTS 0
#define $$MFD_NOFOLLOW 1
#define $$MFD_KEEPLOCK 2
#define $$MFD_RENAMED  4

/** Opens (and initialises) the snapshot-related parts of a main MFD
 *
 * This is done by
 * opening (and creating and initialising, if necessary) the .map and .dat files.
 * Call this before modifying any file, including renaming it.
 *
 * Flags:
 * * $$MFD_DEFAULTS
 * * $$MFD_NOFOLLOW -- do not follow write directives
 * * $$MFD_KEEPLOCK -- keep the file lock on success, until $mfd_close_sn is called
 * * $$MFD_RENAMED  -- only used internally when a write directive is followed
 *
 * Sets:
 * * mfd->mapfd, the map file opened for RDWR or a negative value if unused -- see types.h
 * * mfd->datfd, the dat file opened for WR|APPEND or a negative value if unused -- see types.h
 * * mfd->is_renamed, 0 or 1
 * * mfd->mapheader
 * * mfd->locklabel, optionally mfd->lock
 * * mfd->sn_number, mfd->flags, mfd->write_vpath
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
   const char *fpath_in, /**< the real path of the file in the main space, or NULL  */
   struct $fsdata_t *fsdata, // cannot be const because of the locking
   int flags /**< $$MFD_DEFAULTS or $$MFD_NOFOLLOW | $$MFD_KEEPLOCK */
)
{
   char fmap[$$PATH_MAX];
   char fdat[$$PATH_MAX];
   char fpath_redo[$$PATH_MAX];
   const char *fpath_use;
   int fd; // map file FD
   int fd_dat; // dat file FD
   int ret;
   int waserror = 0; // positive on error
   struct $mapheader_t *maphead;

   // Calculate fpath if needed
   if(fpath_in == NULL) { // We need to re-calculate fpath if we're re-initialising as it is not cached
      if($map_path(fpath_redo, vpath, fsdata) != 0) { return -ENAMETOOLONG; }
      fpath_use = fpath_redo;
   } else {
      fpath_use = fpath_in;
   }

   // Pointers
   maphead = &(mfd->mapheader);

   // Default values
   mfd->sn_number = fsdata->sn_number; /* to see if a new snapshot was created */
   mfd->flags = flags; /* to be able to reinitialise the mfd */
   mfd->is_main = $$mfd_main; /* for safety's sake */
   mfd->lock = -1; // the lock number
   mfd->locklabel = $string2locklabel(fpath_use); // We use fpath and not vpath here as the same label needs to be generated from sn_steps
   mfd->latest_written_block_cache = 0;
   if(flags & $$MFD_RENAMED) {
      strcpy(mfd->write_vpath, vpath);
   } else {
      mfd->write_vpath[0] = '\0';
      strcpy(mfd->vpath, vpath); /* to be able to reinitialise the mfd in case there is a new snapshot */
   }

   // No snapshots?
   if(fsdata->sn_is_any == 0) {
      $dlogdbg("mfd_open_sn: no snapshots found, so returning\n");
      mfd->mapfd = $$MFD_FD_NOSN;
      mfd->datfd = $$MFD_FD_NOSN;
      return 0;
   }

   // Get the paths of the map file
   if($get_map_prefix_path(fmap, vpath, fsdata->sn_lat_dir, fsdata->sn_lat_dir_len) != 0) {
      $dlogi("ERROR file name too long\n");
      return -ENAMETOOLONG;
   }

   // Create path
   // $mkpath is resilient to parallel runs, so we don't lock before calling it.
   ret = $mkpath(fmap, NULL, S_IRWXU);
   if(ret < 0) {  // error
      $dlogi("ERROR mfd_open_sn: mkpath failed with '%d' = '%s'\n", -ret, strerror(-ret));
      return ret;
   }

   // We need to clok here. Even if O_CREAT | O_EXCL is thread-safe, if we lock afterwards,
   // and two threads race to create a new map file, it might not be sure that the one creating it
   // will be the one getting the lock, and so we'd need additional checks to decide who
   // needs to initialise the mapheader.
   $dlogdbg("mfd_open_sn: getting lock for label '%lu'... (vpath='%s', fpath='%s')\n", mfd->locklabel, vpath, fpath_use);
   if(unlikely((mfd->lock = $mflock_lock(fsdata, mfd->locklabel)) < 0)) {
      $dlogi("ERROR mfd_open_sn: mflock_lock(%lu) failed with '%d'='%s'\n", mfd->locklabel, -mfd->lock, strerror(-mfd->lock));
      return mfd->lock;
   }

   do { // [A]

      // Open or create the map file
      fd = open(fmap, O_RDWR | O_CREAT | O_EXCL | O_NOATIME, S_IRWXU);

      if(fd == -1) {

         fd = errno;
         if(unlikely(fd != EEXIST)) { // Other error
            $dlogi("ERROR mfd_open_sn: Failed to open .map at %s, error %d = %s\n", fmap, fd, strerror(fd));
            waserror = fd;
            break; // [A]
         }

         // The .map file already exists, which means that the main file is already dirty.
         // TODO 2 If we aren't actually to set up an mfd, just ensure that the mapheader is written, simply return here

         // Otherwise, we open the .map file again.
         fd = open(fmap, O_RDWR);
         if(unlikely(fd == -1)) {
            waserror = errno;
            $dlogi("ERROR mfd_open_sn: Failed to open .map again at %s, error %d = %s\n", fmap, waserror, strerror(waserror));
            break; // [A]
         }
         $dlogdbg("mfd_open_sn: Managed to open .map file again at '%s', FD '%d'\n", fmap, fd);

         mfd->mapfd = fd;

         do { // [B] From here we either return with a positive errno, or -1 if we need to try again

            if(unlikely((ret = $mfd_load_mapheader(maphead, fd, fsdata)) != 0)) {
               waserror = -ret;
               $dlogi("ERROR mfd_open_sn: Failed to read .map at '%s', error '%d' = '%s'\n", fmap, waserror, strerror(waserror));
               break; // [B]
            }

            // We need to check if there is a "write" directive in here.
            if(maphead->write_v[0] != '\0' && (!(flags & $$MFD_NOFOLLOW))) {
               // Found a write directive, which we need to follow. This is a virtual path.
               $dlogdbg("mfd_open_sn: Found a write directive from '%s' (map: '%s') to '%s'\n", vpath, fmap, maphead->write_v);
               // mfd->is_renamed = 1; // This will be set when we call ourselves again
               waserror = -1;
               // We don't break yet...
            }

            // Release lock; mark as released.
            // Release the lock if we're following a redirect, even if $$MFD_KEEPLOCK
            if((waserror == -1) || (!(flags & $$MFD_KEEPLOCK))) {
               if(unlikely((ret = $mflock_unlock(fsdata, mfd->lock)) != 0)) {
                  waserror = -ret;
                  $dlogi("ERROR during unlock; err %d = %s\n", waserror, strerror(waserror));
                  break; // [B]
               }
               mfd->lock = -1;
            }

            // Skip opening the dat file if we're about to follow a redirect
            if(waserror == -1) {
               break; // [B]
            }

            // There's no write directive

            // Read information about the file as it was at the time of the snapshot
            // and open or create the dat file if necessary
            $dlogdbg("about to open the dat file, vpath='%s'\n", vpath);
            $$MFD_OPEN_DAT_FILE

         } while(0); // [B]

         if(waserror != 0) {  // Cleanup & follow

            close(fd);

            if(waserror == -1) {  // Follow the write directive
               // The lock is already released at this point
               // Start again.
               // We cannot use maphead->write_v directly, so we copy
               // it into a local variable.
               // fdat is a good candidate as it hasn't been used if we are here.
#define $$RECURSION_PATH fdat
               strcpy($$RECURSION_PATH, maphead->write_v);
               if(unlikely((ret = $mfd_open_sn(mfd, $$RECURSION_PATH, NULL, fsdata, flags | $$MFD_RENAMED)) != 0)) {
                  waserror = -ret;
                  $dlogi("ERROR during recursion; err %d = %s\n", waserror, strerror(waserror));
                  break; // [A]
               }
               waserror = 0; // waserror == -1 was not an error condition
#undef $$RECURSION_PATH

               break; // [A]
            }

            break; // [A]
         }

         // Continue below

      } else { // ----- We've created a new .map file -----

         do { // [C]
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
            $dlogdbg("mfd_open_sn: stating '%s'\n", fpath_use);
            if(lstat(fpath_use, &(maphead->fstat)) != 0) {
               ret = errno;
               if(ret == ENOENT) {  // main file does not exist (yet?)
                  // WARNING In this case, mfd.mapheader.fstat remains uninitialised!
                  maphead->exists = 0;
               } else { // some other error
                  $dlogi("ERROR mfd_open_sn: Failed to stat main file at %s, error %d = %s\n", fpath_use, ret, strerror(ret));
                  waserror = ret;
                  break; // [C]
               }
            }

            // mfd's are not currently allowed for directories.
            // We rely on this return value to disallow moving/renaming directories
            if(unlikely(S_ISDIR(maphead->fstat.st_mode))) {
               waserror = EISDIR;
               $dlogi("mfds are not allowed for directories\n");
               break; // [C]
            }

            // write into the map file
            if(unlikely((ret = $mfd_save_mapheader(mfd, fsdata)) != 0)) {
               waserror = -ret;
               $dlogi("ERROR during saving mapheader; err %d = %s\n", waserror, strerror(waserror));
               break; // [C]
            }

            // Release lock; mark as released.
            if(!(flags & $$MFD_KEEPLOCK)) {
               if(unlikely((ret = $mflock_unlock(fsdata, mfd->lock)) != 0)) {
                  waserror = -ret;
                  $dlogi("ERROR during unlock; err %d = %s\n", waserror, strerror(waserror));
                  break; // [C]
               }
               mfd->lock = -1;
            }

            // Read information about the file as it was at the time of the snapshot
            // and open or create the dat file if necessary
            $$MFD_OPEN_DAT_FILE

         } while(0); // [C]

         if(waserror != 0) {  // Cleanup
            close(fd);
            // TODO 2: Clean up directories created by $mkpath based on the 'firstcreated' it can return.
            // However, be aware that other files being opened might already be using the directories
            // created, so using $recursive_remove here is not a safe option.
            // TODO 2: Clean up the dat file?

            // If the lock has already been released, the map file has been initialised successfully,
            // and other threads might be reading it. So don't delete it.
            if(mfd->lock >= 0) { unlink(fmap); }
            break; // [A]
         }

      } // end else

   } while(0); // [A]

   // Release lock if it hasn't been released AND ( there was an error OR we don't need to keep it )
   if(mfd->lock >= 0 && (waserror != 0 || (!(flags & $$MFD_KEEPLOCK)))) {
      $dlogdbg("Releasing leftover lock\n");
      if(unlikely((ret = $mflock_unlock(fsdata, mfd->lock)) != 0)) {
         $dlogi("ERROR mflock_unlock failed with '%d'='%s'\n", -ret, strerror(-ret));
         waserror = -ret;
      }
   }

   return -waserror;
}


/** Marks a main MFD as read-only */
static inline void $mfd_open_sn_rdonly(
   struct $mfd_t *mfd
)
{
   mfd->mapfd = $$MFD_FD_RDONLY;
   mfd->datfd = $$MFD_FD_RDONLY;
   mfd->lock = -1;
   mfd->is_main = $$mfd_main; /* for safety's sake */
}


/** Closes the snapshot-related parts of a main MFD
 *
 * Returns:
 * * 0 on success
 * * -errno on error (the last errno)
 */
static inline int $mfd_close_sn(struct $mfd_t *mfd, struct $fsdata_t *fsdata)
{
   int ret;
   int waserror = 0;

   if(mfd->datfd >= 0) {
      if(unlikely(close(mfd->datfd) != 0)) { waserror = errno; }
   }

   if(mfd->mapfd >= 0) {
      if(unlikely(close(mfd->mapfd) != 0)) { waserror = errno; }
   }

   if(mfd->lock >= 0) {
      if(unlikely((ret = $mflock_unlock(fsdata, mfd->lock)) != 0)) { waserror = errno; }
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
   struct $fsdata_t *fsdata
)
{
   struct $mfd_t mymfd;
   int ret;

   // It's somewhat wasteful to allocate a whole mfd here,
   // but we need the mapheader, which is included in it, anyway.

   if((ret = $mfd_open_sn(&mymfd, vpath, fpath, fsdata, $$MFD_DEFAULTS)) == 0) {
      ret = $mfd_close_sn(&mymfd, fsdata);
   }
   return ret;
}


/** Ensures that a snapshot has not been created since the mfd was initialised.
 * If yes, re-initialise the mfd.
 *
 * Returns:
 * * 0 - on success
 * * -errno - on error
 */
static inline int $mfd_validate(
   struct $mfd_t *mfd,
   struct $fsdata_t *fsdata
)
{
   int ret;

   if(fsdata->sn_number == mfd->sn_number) { return 0; }

   // We won't use the snapshot if the main file is opened for read only
   if(mfd->mapfd == $$MFD_FD_RDONLY) { return 0; }

   $dlogdbg("! Reinitialising the mfd\n");

   if((ret = $mfd_close_sn(mfd, fsdata)) != 0) {
      $dlogi("ERROR mfd_close_sn failed with err %d = %s\n", -ret, strerror(-ret));
      return ret;
   }
   if((ret = $mfd_open_sn(mfd, mfd->vpath, NULL, fsdata, mfd->flags)) != 0) {
      $dlogi("ERROR mfd_open_sn failed with err %d = %s\n", -ret, strerror(-ret));
      return ret;
   }

   mfd->sn_number = fsdata->sn_number;
   return 0;
}


/** Ensures that a snapshot has not been created since the in-snapshot mfd was initialised.
 * For now, only returns an error if there is a new snapshot.
 *
 * Returns:
 * * 0 - on success
 * * -errno - on error
 */
// TODO 2 Save the necessary data and reinitialise mfd
static inline int $mfd_in_sn_validate(
   struct $mfd_t *mfd,
   struct $fsdata_t *fsdata
)
{
   if(fsdata->sn_number == mfd->sn_number) { return 0; }
   $dlogi("ERROR: A snapshot has been created since this in-snapshot file was opened.\n");
   return -EREMCHG;
}


/** See $mfd_get_sn_steps */
#define $$SN_STEPS_F_FILE           00000001
#define $$SN_STEPS_F_DIR            00000002
#define $$SN_STEPS_F_TYPE_UNKNOWN   00000004
#define $$SN_STEPS_F_FIRSTONLY      00000010
#define $$SN_STEPS_F_SKIPOPENDAT    00000020
#define $$SN_STEPS_F_SKIPOPENDIR    00000040
#define $$SN_STEPS_F_STATDIR        00000100

/** Closes filehandles and frees the memory associated with sn_steps
 *
 * Returns:
 * * 0 - on success
 * * -errno - on failure
 */
static int $mfd_destroy_sn_steps(struct $mfd_t *mfd, const struct $fsdata_t *fsdata)
{
   int waserror = 0;
   int i;
   int j;

   if(mfd->sn_first_file >= 0) {
      for(i = mfd->sn_current; i >= 0; i--) {

         j = mfd->sn_steps[i].mapfd;
         if(j >= 0 && close(j) != 0) { waserror = -errno; }

         j = mfd->sn_steps[i].datfd;
         if(j >= 0 && close(j) != 0) { waserror = -errno; }

         if(mfd->sn_steps[i].dirfd != NULL && closedir(mfd->sn_steps[i].dirfd) != 0) { waserror = -errno; }
      }
   }

   free(mfd->sn_steps);

   return waserror;
}


/** Sets up mfd->sn_steps for a path inside a snapshot.
 * Can also open the map, dat, main files and directories.
 *
 * Flags:
 * * $$SN_STEPS_F_FILE | $$SN_STEPS_F_DIR | $$SN_STEPS_F_TYPE_UNKNOWN -- what type to expect
 * * $$SN_STEPS_F_FIRSTONLY -- stop after the first file found
 * * $$SN_STEPS_F_SKIPOPENDAT -- skip opening the dat files and the main file
 * * $$SN_STEPS_F_SKIPOPENDIR -- skip opening the directories (but stat them into mfd->mapheader.fstat)
 * * $$SN_STEPS_F_STATDIR -- stat directories into mfd->mapheader.fstat
 *
 * Sets:
 * * mfd->sn_current - index of the selected snapshot
 * * mfd->sn_first_file - index of the first snapshot with information about the file
 * * mfd->sn_steps
 * * mfd->mapheader - from the first map file found, or
 * * mfd->mapheader.fstat - from a main file or a directory
 * * mfd->locklabel
 * * mfd->sn_number
 *
 * Returns:
 * * 0 - on success
 * * -errno - on failure
 */
static int $mfd_get_sn_steps(
   struct $mfd_t *mfd,
   const struct $snpath_t *snpath,
   const struct $fsdata_t *fsdata,
   int flags /**< See $$SN_STEPS_F_OPENFILE, $$SN_STEPS_F_OPENDIR, $$SN_STEPS_F_FIRSTONLY */
)
{
   int waserror = 0; // negative on error
   int sni, ret, fd;
   char knowntype;
   char *s;
   char mypath[$$PATH_MAX];
   char mysnpath[$$PATH_MAX];
   DIR *dirfd;
   struct $mapheader_t maphead;
   struct stat mystat;

   // Default values
   mfd->sn_number = fsdata->sn_number;

   // Get the roots of the snapshots and the main space
   if(unlikely((ret = $sn_get_paths_to(mfd, snpath, fsdata)) != 0)) {
      $dlogi("ERROR sn_get_paths_to failed with %d = %s\n", -ret, strerror(-ret));
      return ret;
   }

   // All that remains is to add the path

   // Get the first path
   if(snpath->is_there == $$snpath_full) {
      strcpy(mypath, snpath->inpath);
   } else {
      mypath[0] = '\0';
   }

   // Initialise all FDs for the benefit of $mfd_destroy_sn_steps
   mfd->sn_first_file = -1;
   for(sni = mfd->sn_current; sni >= 0; sni--) {
      mfd->sn_steps[sni].mapfd = $$SN_STEPS_NOTOPEN;
      mfd->sn_steps[sni].datfd = $$SN_STEPS_NOTOPEN;
      mfd->sn_steps[sni].dirfd = NULL;
   }

   for(sni = mfd->sn_current; sni >= 0; sni--) {

      s = mfd->sn_steps[sni].path;
      if(unlikely(strlen(mypath) + strlen(s) >= $$PATH_MAX)) {
         waserror = -ENAMETOOLONG;
         $dlogi("ERROR filename too long\n");
         break;
      }
      strcat(s, mypath); // s is a pointer!

      $dlogdbg("sn_step %d: '%s'\n", sni, s);

      // IF WE DON'T KNOW WHETHER TO EXPECT A FILE OR A DIRECTORY
      // Used when we want to stat a path
      knowntype = '?';
      if(flags & $$SN_STEPS_F_TYPE_UNKNOWN) {
         // Test for a directory: try to stat the name as-is
         ret = 0;
         if(lstat(mfd->sn_steps[sni].path, &mystat) != 0) {
            ret = errno;
            if(ret == ENOENT) {
               // Try treating this as a file
               knowntype = 'f';
            } else {
               $dlogi("ERROR lstat on '%s' failed with %d = %s/n", mfd->sn_steps[sni].path, ret, strerror(ret));
               waserror = -ret;
               break;
            }
         } else {
            knowntype = (S_ISDIR(mystat.st_mode) ? 'd' : 'f'); // 'd' also means that the directory stat is available
         }
         $dlogdbg("unknown type at '%s' is recognised as '%c'\n", mfd->sn_steps[sni].path, knowntype);
      }

      // DIRECTORY
      if((knowntype == 'd') || (flags & $$SN_STEPS_F_DIR)) {

         // Try to open the directory
         if(!(flags & $$SN_STEPS_F_SKIPOPENDIR)) {
            dirfd = opendir(mfd->sn_steps[sni].path);
            if(dirfd == NULL) {
               ret = errno;
               if(ret == ENOENT) {
                  mfd->sn_steps[sni].mapfd = $$SN_STEPS_UNUSED;
                  mfd->sn_steps[sni].datfd = $$SN_STEPS_UNUSED;
                  continue;
               } else {
                  $dlogi("ERROR opendir on '%s' failed with err %d = %s/n", mfd->sn_steps[sni].path, ret, strerror(ret));
                  waserror = -ret;
                  break;
               }
            }

            // Successfully opened the directory
            mfd->sn_steps[sni].dirfd = dirfd;
         }

         if(mfd->sn_first_file == -1) {
            mfd->sn_first_file = sni;
            if((flags & $$SN_STEPS_F_STATDIR) || (flags & $$SN_STEPS_F_SKIPOPENDIR)) {
               if(knowntype == 'd') {
                  memcpy(&(mfd->mapheader.fstat), &mystat, sizeof(struct stat));
               } else {
                  if(lstat(mfd->sn_steps[sni].path, &(mfd->mapheader.fstat)) != 0) {
                     ret = errno;
                     if(ret == ENOENT) {
                        mfd->sn_steps[sni].mapfd = $$SN_STEPS_UNUSED;
                        mfd->sn_steps[sni].datfd = $$SN_STEPS_UNUSED;
                     } else {
                        $dlogi("ERROR lstat on '%s' failed with err %d = %s/n", mfd->sn_steps[sni].path, ret, strerror(ret));
                        waserror = -ret;
                        break;
                     }
                  }
               }
            }
            if(flags & $$SN_STEPS_F_FIRSTONLY) { break; }
         }

         // FILE
      } else if((knowntype == 'f') || (flags & $$SN_STEPS_F_FILE)) {

         if(sni > 0) { // in a snapshot

            // Read the map file for a read directive
            if(unlikely((ret = $get_map_path(mysnpath, mfd->sn_steps[sni].path)) != 0)) {
               waserror = ret;
               break;
            }

            $dlogdbg("opening the map file '%s'\n", mysnpath);
            fd = open(mysnpath, O_RDONLY);
            if(fd == -1) {
               ret = errno;
               if(ret == ENOENT) {
                  // This snapshot has no information about this file
                  mfd->sn_steps[sni].mapfd = $$SN_STEPS_UNUSED;
                  mfd->sn_steps[sni].datfd = $$SN_STEPS_UNUSED;
                  continue;
               } else {
                  $dlogi("ERROR open on '%s' failed with err %d = %s/n", mysnpath, ret, strerror(ret));
                  waserror = -ret;
                  break;
               }
            }

            // We save this here so that mfd_destroy_sn_steps would close it on error
            mfd->sn_steps[sni].mapfd = fd;

            do {

               if(unlikely((ret = $mfd_load_mapheader(&maphead, fd, fsdata)) != 0)) {
                  $dlogi("ERROR mfd_load_mapheader failed with err %d = %s\n", -ret, strerror(-ret));
                  waserror = ret;
                  break;
               }

               if(maphead.read_v[0] != '\0') { // there is a read directive
                  strcpy(mypath, maphead.read_v);
               }

            } while(0);

            // Open the dat file
            if(!(flags & $$SN_STEPS_F_SKIPOPENDAT)) {

               if(unlikely((ret = $get_dat_path(mysnpath, mfd->sn_steps[sni].path)) != 0)) {
                  waserror = ret;
                  break;
               }

               $dlogdbg("opening the dat file '%s'\n", mysnpath);
               fd = open(mysnpath, O_RDONLY);
               if(fd == -1) {
                  ret = errno;
                  if(ret == ENOENT) {
                     mfd->sn_steps[sni].datfd = $$SN_STEPS_UNUSED;
                     continue;
                  } else {
                     $dlogi("ERROR open on '%s' failed with %d = %s/n", mysnpath, ret, strerror(ret));
                     waserror = -ret;
                     break;
                  }
               }

               // We save this here so that mfd_destroy_sn_steps would close it on error
               mfd->sn_steps[sni].datfd = fd;

            }

         } else { // a main file

            mfd->sn_steps[sni].mapfd = $$SN_STEPS_MAIN;

            // Initialise the lock label
            mfd->locklabel = $string2locklabel(mfd->sn_steps[sni].path);

            if(flags & $$SN_STEPS_F_SKIPOPENDAT) {

               mfd->sn_steps[sni].datfd = $$SN_STEPS_NOTOPEN;

            } else {

               // Try to open the file
               $dlogdbg("opening the main file '%s'\n", mfd->sn_steps[sni].path);
               fd = open(mfd->sn_steps[sni].path, O_RDONLY);
               if(fd == -1) {
                  ret = errno;
                  if(ret == ENOENT) {
                     mfd->sn_steps[sni].datfd = $$SN_STEPS_UNUSED;
                     continue;
                  } else {
                     $dlogi("ERROR open on '%s' failed with %d = %s/n", mysnpath, ret, strerror(ret));
                     waserror = -ret;
                     break;
                  }
               }

               // We save this here so that mfd_destroy_sn_steps would close it on error
               mfd->sn_steps[sni].datfd = fd;

            }

         }

         if(mfd->sn_first_file == -1) { // first existing file found
            mfd->sn_first_file = sni;
            if(sni > 0) {
               // Save the mapheader from the first map file found
               $dlogdbg("stating: saving the map header\n");
               memcpy(&(mfd->mapheader), &maphead, sizeof(struct $mapheader_t));
            } else {
               // stat the main file as well
               $dlogdbg("stating the main file '%s'\n", mfd->sn_steps[sni].path);
               if(unlikely(lstat(mfd->sn_steps[sni].path, &(mfd->mapheader.fstat)) != 0)) {
                  waserror = -errno;
                  $dlogi("ERROR stating '%s' failed with err %d = %s\n", mfd->sn_steps[sni].path, -waserror, strerror(-waserror));
                  break;
               }
            }
            if(flags & $$SN_STEPS_F_FIRSTONLY) { break; }
         }

      } else {
         $dlogi("ERROR Wrong flag!");
         waserror = -EBADE;
      }

      if(waserror != 0) {
         break;
      }

   } // end for

   if(waserror != 0) {
      $mfd_destroy_sn_steps(mfd, fsdata);
      return waserror;
   }

   return 0;
}
