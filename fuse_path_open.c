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


/* Helper function: Opens a file, saves the part to be truncated, closes it.
 * Returns 0 or -errno.
 */
static inline int $_open_truncate_close(struct $fsdata_t *fsdata, const char *path, const char *fpath, off_t newsize)
{
   struct $mfd_t myfd;
   struct $mfd_t *mfd;
   int ret;
   int waserror = 0;

   mfd = &myfd;

   if(unlikely((ret = $mfd_open_sn(mfd, path, fpath, fsdata, $$MFD_DEFAULTS)) != 0)) {
      return ret;
   }

   do {

      // Return here if there are no snapshots
      if(mfd->mapfd == $$MFD_FD_NOSN) {
         break;
      }

      // Return here if the file did not exist, as we don't need to save the data
      if(mfd->mapheader.exists == 0) {
         break;
      }

      ret = open(fpath, O_RDONLY);
      if(unlikely(ret == -1)) {
         waserror = errno;
         $dlogi("ERROR truncate(%s): failed to open main file err %d = %s\n", fpath, waserror, strerror(waserror));
         break;
      }
      mfd->mainfd = ret;

      ret = $b_truncate(fsdata, mfd, newsize);
      if(unlikely(ret != 0)) {
         waserror = -ret;
         $dlogi("ERROR truncate(%s): b_truncate failed err %d = %s\n", fpath, waserror, strerror(waserror));
         // Don't break here, we want to close mainfd anyway
      }

      close(mfd->mainfd);

   } while(0);

   // TODO CLEAN UP MAP/DAT FILES unnecessarily created?
   if(unlikely((ret = $mfd_close_sn(mfd, fsdata)) != 0)) {
      $dlogi("ERROR _open_truncate_close(%s): mfd_close_sn failed err %d = %s\n", fpath, -ret, strerror(-ret));
      return ret;
   }

   return -waserror;

   // TODO add optimisation: set whole_saved if newsize==0
}


/** File open operation
 *
 * No creation (O_CREAT, O_EXCL) and by default also no
 * truncation (O_TRUNC) flags will be passed to open(). If an
 * application specifies O_TRUNC, fuse first calls truncate()
 * and then open(). Only if 'atomic_o_trunc' has been
 * specified and kernel version is 2.6.24 or later, O_TRUNC is
 * passed on to open.
 *
 * Unless the 'default_permissions' mount option is given,
 * open should check if the operation is permitted for the
 * given flags. Optionally open may also return an arbitrary
 * filehandle in the fuse_file_info structure, which will be
 * passed to all file operations.
 *
 * Changed in version 2.2
 */
// TODO 2 Use (-o atomic_o_trunc) and provide support for atomic_o_trunc (see above) for greater efficiency
int $open(const char *path, struct fuse_file_info *fi)
{
   int fd;
   int flags;
   int waserror = 0; // positive on error
   struct $mfd_t *mfd;

   $$IF_PATH_SN

   flags = fi->flags;

   $dlogdbg("* open.sn path='%s' flags= '%d'\n", path, flags);

   do {

      if((flags & O_ACCMODE) != O_RDONLY) {
         snret = -EACCES;
         break;
      }

      if(snpath->is_there != $$snpath_full) { // We cannot open the snapshot roots
         snret = -EACCES;
         break;
      }

      // TODO 2 Implement permission checking here. See $access

      mfd = malloc(sizeof(struct $mfd_t));
      if(mfd == NULL) {
         snret =  -ENOMEM;
         break;
      }

      if((snret = $mfd_get_sn_steps(mfd, snpath, fsdata, $$SN_STEPS_F_FILE)) != 0) {
         free(mfd);
         break;
      }

      mfd->is_main = $$mfd_sn_full;

      fi->fh = (intptr_t) mfd;
      fi->keep_cache = 1;

      snret = 0;

   } while(0);

   $$ELIF_PATH_MAIN

   $dlogdbg("* open.main path='%s'\n", path);

   mfd = malloc(sizeof(struct $mfd_t));
   if(mfd == NULL) { return -ENOMEM; }

   do {
      flags = fi->flags;

      $dlogdbg("  open.main path='%s' flags='%d'\n", fpath, flags);

      if((flags & O_ACCMODE) == O_RDONLY) { // opening for read-only

         $mfd_open_sn_rdonly(mfd);

      } else { // opening for writing

         // Save the current status of the file by initialising the map file.
         // We don't delete this even if the subsequent operation fails.
         fd = $mfd_open_sn(mfd, path, fpath, fsdata, $$MFD_DEFAULTS); // fd only stores a success flag here
         if(fd < 0) {
            waserror = -fd; // converting -errno to +errno
            break;
         }

         // If the client wants to open O_WRONLY, we still need to read from the file to save
         // the overwritten blocks.
         // TODO 2 Should we get a new filehandle to do this?
         if((flags & O_ACCMODE) == O_WRONLY) {
            flags |= O_ACCMODE;
            flags ^= O_ACCMODE;
            flags |= O_RDWR;
         }

      }

      do {

         fd = open(fpath, flags);
         if(fd == -1) {
            waserror = errno;
            break;
         }

         mfd->mainfd = fd;

      } while(0);

      if(waserror != 0) {
         // TODO 2 CLEAN UP MAP/DAT FILES if they have just been created?
         $mfd_close_sn(mfd, fsdata);
         break;
      }

   } while(0);

   if(waserror != 0) {
      free(mfd);
      return -waserror;
   }

   $dlogdbg("  open success main fd=%d\n", fd);

   mfd->is_main = $$mfd_main;

   fi->fh = (intptr_t) mfd;
   fi->keep_cache = 1;

   return 0;

   $$FI_PATH
}


/**
 * Create and open a file
 *
 * FUSE: If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * Here, we use a single open to create the file.
 *
 * FUSE: If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int $create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
   int fd;
   int waserror = 0; // positive on error
   struct $mfd_t *mfd;

   $$IF_PATH_MAIN_ONLY

   $dlogdbg("* create(path=\"%s\", flags=%d, mode=0%03o)\n", path, fi->flags, mode);

   mfd = malloc(sizeof(struct $mfd_t));
   if(mfd == NULL) { return -ENOMEM; }

   do {
      // Save the current status of the file by initialising the map file.
      // We don't delete this even if the subsequent operation fails.
      fd = $mfd_open_sn(mfd, path, fpath, fsdata, $$MFD_DEFAULTS); // fd only stores a success flag here
      if(fd < 0) {
         waserror = -fd; // Converting -errno to +errno;
         break;
      }

      do {

         // If there are snapshots and the file exists
         if(mfd->mapfd >= 0 && mfd->mapheader.exists == 1) {
            // This is somewhat wasteful as it sets up a new mfd
            if(unlikely((fd = $_open_truncate_close(fsdata, path, fpath, 0)) != 0)) { // fd only stores a success flag here
               waserror = -fd;
               $dlogi("ERROR create(%s): _open_truncate_close failed err %d = %s\n", fpath, waserror, strerror(waserror));
               break;
            }
         }

         fd = open(fpath, fi->flags | O_CREAT | O_TRUNC, mode);
         if(fd < 0) {
            waserror = errno;
            $dlogdbg("WARNING open[create](%s) failed with %d = %s\n", fpath, waserror, strerror(waserror));
            break;
         }

         $dlogdbg("opened[created] '%s' fd=%d\n", fpath, fd);
         mfd->mainfd = fd;

      } while(0);

      if(waserror != 0) {
         // TODO CLEAN UP MAP/DAT FILES
         $mfd_close_sn(mfd, fsdata);
         break;
      }

   } while(0);

   if(waserror != 0) {
      free(mfd);
      return -waserror;
   }

   mfd->is_main = $$mfd_main;

   fi->fh = (intptr_t) mfd;
   fi->keep_cache = 1;

   return 0;
}


/** Open directory
 *
 * Unless the 'default_permissions' mount option is given,
 * this method should check if opendir is permitted for this
 * directory. Optionally opendir may also return an arbitrary
 * filehandle in the fuse_file_info structure, which will be
 * passed to readdir, closedir and fsyncdir.
 *
 * Introduced in version 2.3
 */
int $opendir(const char *path, struct fuse_file_info *fi)
{
   DIR *dp;
   int waserror = 0; // negative on error
   struct $mfd_t *mfd;

   $$IF_PATH_SN

   do {

      $dlogdbg("* opendir.sn(path=\"%s\")\n", path);

      mfd = malloc(sizeof(struct $mfd_t));
      if(unlikely(mfd == NULL)) {
         waserror = -ENOMEM;
         break;
      }

      do {

         // If we're opening the main /snapshots/ directory
         if(snpath->is_there == $$snpath_root) {

            dp = opendir(fpath);
            if(unlikely(dp == NULL)) {
               waserror = -ENOMEM;
               break;
            }

            mfd->maindir = dp;
            mfd->is_main = $$mfd_sn_root;

            fi->fh = (intptr_t) mfd;
            fi->keep_cache = 1;

            break;

         }

         if(unlikely((waserror = $mfd_get_sn_steps(mfd, snpath, fsdata, $$SN_STEPS_F_DIR)) != 0)) {
            $dlogi("ERROR get sn steps failed with %d = %s\n", -waserror, strerror(-waserror));
            break;
         }

         // No directories found in any snapshot or the main part
         if(mfd->sn_first_file == -1) {
            if(unlikely((waserror = $mfd_destroy_sn_steps(mfd, fsdata)) != 0)) {
               $dlogi("ERROR destroy sn steps failed with %d = %s\n", -waserror, strerror(-waserror));
               break;
            }
            waserror = -ENOENT;
            break;
         }

         mfd->is_main = (snpath->is_there == $$snpath_full ? $$mfd_sn_full : $$mfd_sn_id);

         fi->fh = (intptr_t) mfd;
         fi->keep_cache = 1;

      } while(0);

      if(waserror < 0) {
         free(mfd);
      }

   } while(0);

   snret = waserror;

   $$ELIF_PATH_MAIN

   $dlogdbg("* opendir.main(path=\"%s\")\n", path);

   mfd = malloc(sizeof(struct $mfd_t));
   if(unlikely(mfd == NULL)) {
      return -ENOMEM;
   }

   dp = opendir(fpath);
   if(unlikely(dp == NULL)) {
      free(mfd);
      return -errno;
   }

   mfd->maindir = dp;
   mfd->is_main = $$mfd_main;

   fi->fh = (intptr_t) mfd;
   fi->keep_cache = 1;

   return 0;

   $$FI_PATH
}
