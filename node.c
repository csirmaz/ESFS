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
 * that have been modified since the creation of the snapshot.
 *
 * All original dentry information about a file is saved in the header
 * of the .map file, including its size.
 *
 * If a file did not exist when the snapshot was taken, this fact
 * is saved in the header of the .map file.
 *
 * The presence of the .map file means that the file in the main space is
 * already dirty, and dentry changes need not be saved again in the snapshot.
 */


/* Opens (and initialises, if necessary) the .map file,
 * saving the information about a file that usually goes into the directory
 * entry, like flags, permissions, and, most importantly, size.
 *   vpath is the virtual path to the file in the main space.
 * Sets
 *   mfd->mapfd, the map file opened for RDWR or -1
 * Returns
 *   0 - on success
 *  -errno - on failure
 */
int $open_init_map(struct $fd_t *mfd, const char *vpath, const char *fpath, const struct $fsdata_t *fsdata)
{
   char fmap[$$PATH_MAX];
   int fd;
   int ret;
   int waserror = 0;
   struct $mapheader_t *mapheader;

   // No snapshots?
   if(fsdata->sn_is_any == 0){
      mfd->mapfd = -1;
      return 0;
   }

   $$ADDNPSFIX(fmap, vpath, fsdata->sn_lat_dir, fsdata->sn_lat_dir_len, $$EXT_MAP, $$EXT_LEN)

   fd = open(fmap, O_RDWR | O_CREAT | O_EXCL, S_IRWXU);

   if(fd == -1){
      fd = errno;
      if(fd == EEXIST){
         // The .map file already exists, which means that the main file is already dirty.
         // We open the .map file again.
         fd = open(fmap, O_RDWR);
         if(unlikely(fd == -1)){
            $dlogi("Failed to open .map again at %s, error %d = %s\n", fmap, errno, strerror(errno));
            return -errno;
         }
         mfd->mapfd = fd;
         return 0;
      }
      $dlogi("Failed to open .map at %s, error %d = %s\n", fmap, fd, strerror(fd));
      return -fd;
   }

   do{
      // We created the .map file; let's save data about the main file.
      mapheader = malloc(sizeof(struct $mapheader_t));
      if(mapheader == NULL){
         waserror = ENOMEM;
         break;
      }

      do{
         mapheader->exists = 1;

         // stat the file
         if(lstat(fpath, &(mapheader->fstat)) != 0){
            ret = errno;
            if(ret == ENOENT){ // main file does not exist (yet)
               mapheader->exists = 0;
            }else{ // some other error
               $dlogi("Failed to stat main file at %s, error %d = %s\n", fpath, ret, strerror(ret));
               waserror = ret;
               break;
            }
         }

         // write into the map file
         ret = pwrite(fd, mapheader, sizeof(struct $mapheader_t), 0);
         if(unlikely(ret == -1)){
            $dlogi("Failed to write .map header at %s, error %d = %s\n", fmap, ret, strerror(ret));
            waserror = errno;
            break;
         }
         if(unlikely(ret != sizeof(struct $mapheader_t))){
            $dlogi("Failed: only written %d bytes into .map header at %s\n", ret, fmap);
            waserror = EIO;
            break;
         }

      }while(0);

      free(mapheader);

   }while(0);

   if(waserror != 0){
      close(fd);
      unlink(fmap);
      return -waserror;
   }

   mfd->mapfd = fd;
   return 0;
}

// $push_whole_file

// $pull_directory

// $push_blocks

// $pull_blocks


