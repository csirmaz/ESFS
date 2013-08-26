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

/* This file contains functions related to storing and retriecing
 * files and data in snapshots.
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
 * the latest snapshot:
 *
 * A.dat
 * A.map           A(A)
 *
 * B.dat
 * B.map:read C
 *
 * C.map:write B   C(B)
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


/* Opens (and initialises, if necessary) the .map and .dat files.
 * Call this before modifying any file, including renaming it.
 *   vpath is the virtual path to the file in the main space.
 * Sets
 *   mfd->mapfd, the map file opened for RDWR or -1 if there are no snapshots
 *   mfd->datfd, the dat file opened for WR or -1 if there are no snapshots
 * Saves
 *   stats of the file into the map file, unless the map file already exists.
 * Returns
 *   0 - on success
 *  -errno - on failure
 */
int $n_open(
   struct $fd_t *mfd,
   const char *vpath,
   const char *fpath,
   const struct $fsdata_t *fsdata
)
{
   char fmap[$$PATH_MAX];
   char fdat[$$PATH_MAX];
   int fd;
   int ret;
   int waserror = 0;
   struct $mapheader_t mapheader;
   $$PATH_LEN_T plen;

   // No snapshots?
   if(fsdata->sn_is_any == 0){
      $dlogdbg("n_open: no snapshots found, so returning\n");
      mfd->mapfd = -1;
      mfd->datfd = -1;
      return 0;
   }

   // Get the paths of the map & dat files
   $$ADDNPSFIX_CONT(fmap, fdat, vpath, fsdata->sn_lat_dir, fsdata->sn_lat_dir_len, $$EXT_MAP, $$EXT_DAT, $$EXT_LEN)

   //////////////////////////////////
   // TODO implement mkdir -p here // TODO which creates directories mirroring them in the main space
   ////////////////////////////////// (create them when THEY are modified)

   // Open or create the map file
   fd = open(fmap, O_RDWR | O_CREAT | O_EXCL, S_IRWXU);

   if(fd == -1){

      fd = errno;
      if(fd == EEXIST){
         // The .map file already exists, which means that the main file is already dirty.
         // We open the .map file again.
         fd = open(fmap, O_RDWR);
         if(unlikely(fd == -1)){
            fd = errno;
            $dlogi("n_open: Failed to open .map again at %s, error %d = %s\n", fmap, fd, strerror(fd));
            return -fd;
         }

         do{ // From here we either return with a positive errno, or -1 if we need to try again

            // We need to check if there is a "write" directive in here.
            ret = pread(fd, &mapheader, sizeof(struct $mapheader_t), 0);
            if(unlikely(ret == -1)){
               waserror = errno;
               $dlogi("n_open: Failed to read .map at %s, error %d = %s\n", fmap, waserror, strerror(waserror));
               break;
            }
            if(unlikely(ret != sizeof(struct $mapheader_t))){
               $dlogi("n_open: Only read %d bytes instead of %ld from .map at %s. Broken FS?\n", ret, sizeof(struct $mapheader_t), fmap);
               waserror = EIO;
               break;
            }
            if(mapheader.write_v[0] != '\0'){
               // Found a write directive, which we need to follow. This is a virtual path.
               $dlogdbg("n_open: Found a write directive from %s (map: %s) to %s\n", vpath, fmap, mapheader.write_v);
               waserror = -1;
               break;
            }

            // There's no write directive
            // Open or create the dat file
            fd = open(fdat, O_WRONLY | O_CREAT, S_IRWXU);
            if(fd == -1){
               fd = errno;
               $dlogi("n_open: Failed to open .dat at %s, error %d = %s (1)\n", fdat, fd, strerror(fd));
               waserror = fd;
               break;
            }
            mfd->datfd = fd;

         }while(0);

         if(waserror != 0){ // Cleanup & follow
            close(fd);
            if(waserror == -1){ // Follow the write directive
               // Get the full path in fmap
               $$ADDNPREFIX_CONT(fmap, mapheader.write_v, fsdata->sn_lat_dir, fsdata->sn_lat_dir_len)
               return $n_open(mfd, mapheader.write_v, fmap, fsdata);
            }
            return -waserror;
         }

         mfd->mapfd = fd;
         // Continue below

      } else { // Other error
         $dlogi("n_open: Failed to open .map at %s, error %d = %s\n", fmap, fd, strerror(fd));
         return -fd;
      }

   } else { // We've created a new .map file

      do{
         // We've created the .map file; let's save data about the main file.

         mfd->mapfd = fd;

         mapheader.exists = 1;
         mapheader.read_v[0] = '\0';
         mapheader.write_v[0] = '\0';

         // stat the main file
         if(lstat(fpath, &(mapheader.fstat)) != 0){
            ret = errno;
            if(ret == ENOENT){ // main file does not exist (yet)
               mapheader.exists = 0;
            }else{ // some other error
               $dlogi("n_open: Failed to stat main file at %s, error %d = %s\n", fpath, ret, strerror(ret));
               waserror = ret;
               break;
            }
         }

         // write into the map file
         ret = pwrite(fd, &mapheader, sizeof(struct $mapheader_t), 0);
         if(unlikely(ret == -1)){
            $dlogi("n_open: Failed to write .map header at %s, error %d = %s\n", fmap, ret, strerror(ret));
            waserror = errno;
            break;
         }
         if(unlikely(ret != sizeof(struct $mapheader_t))){
            $dlogi("n_open: Failed: only written %d bytes into .map header at %s\n", ret, fmap);
            waserror = EIO;
            break;
         }

         // Open or create the dat file
         fd = open(fdat, O_WRONLY | O_CREAT, S_IRWXU);
         if(fd == -1){
            fd = errno;
            $dlogi("n_open: Failed to open .dat at %s, error %d = %s (2)\n", fdat, fd, strerror(fd));
            waserror = fd;
            break;
         }
         mfd->datfd = fd;

      }while(0);

      if(waserror != 0){ // Cleanup
         close(fd);
         unlink(fmap);
         return -waserror;
      }

   }

   return 0;
}


// Marks main FD as read-only
static void $n_open_rdonly(struct $fd_t *mfd)
{
   mfd->mapfd = -2;
   mfd->datfd = -2;
}


// $push_whole_file

// $pull_directory

// $push_blocks

// $pull_blocks


