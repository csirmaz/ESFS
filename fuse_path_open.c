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
   int waserror = 0;
   struct $mfd_t *mfd;
   struct stat mystat;

   $$IF_PATH_SN

   do {

      if(snpath->is_there != $$SNPATH_FULL) {
         snret = -EACCES;
         break;
      }

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
      snret = 0;

   } while(0);

   $$ELIF_PATH_MAIN

   mfd = malloc(sizeof(struct $mfd_t));
   if(mfd == NULL) { return -ENOMEM; }

   do {
      flags = fi->flags;

      $dlogdbg("  open.main(%s, %s %s %s %s)\n", fpath,
               ((flags & O_TRUNC) > 0) ? "TRUNC" : "",
               ((flags & O_RDWR) > 0) ? "RDWR" : "",
               ((flags & O_RDONLY) > 0) ? "RD" : "",
               ((flags & O_WRONLY) > 0) ? "WR" : "");

      if((flags & O_WRONLY) == 0 && (flags & O_RDWR) == 0) { // opening for read-only

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
         // TODO Should we get a new filehandle to do this?
         if(flags & O_WRONLY) { flags ^= O_WRONLY; flags |= O_RDWR; }

      }

      do {

         fd = open(fpath, flags);
         if(fd == -1) {
            waserror = errno;
            break;
         }
         mfd->mainfd = fd;

         do {

            // We need to store the inode no of the file.
            // We can only do that here after the open as it's possibly new
            // TODO but if it's not (see mapheader.exists), we could get the inode from $mfd_open_sn!
            if(fstat(fd, &mystat) == -1) {
               waserror = errno;
               break;
            }
            mfd->main_inode = mystat.st_ino;

         } while(0);

         if(waserror != 0) {
            close(fd);
         }

      } while(0);

      if(waserror != 0) {
         // TODO CLEAN UP MAP/DAT FILES if they have just been created?
         $mfd_close_sn(mfd);
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

   return 0;

   $$FI_PATH
}


/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int $create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
   int fd;
   int waserror = 0;
   struct $mfd_t *mfd;
   struct stat mystat;

   $$IF_PATH_MAIN_ONLY

   $dlogdbg("  create(path=\"%s\", mode=0%03o)\n", path, mode);

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

         fd = creat(fpath, mode);
         if(fd < 0) {
            waserror = errno;
            break;
         }
         mfd->mainfd = fd;

         do {

            // We need to store the inode no of the file. We can only do that here as it's new
            if(fstat(fd, &mystat) == -1) {
               waserror = errno;
               break;
            }
            mfd->main_inode = mystat.st_ino;

         } while(0);

         if(waserror != 0) {
            close(fd);
         }

      } while(0);

      if(waserror != 0) {
         // TODO CLEAN UP MAP/DAT FILES
         $mfd_close_sn(mfd);
         break;
      }

   } while(0);

   if(waserror != 0) {
      free(mfd);
      return -waserror;
   }

   mfd->is_main = $$mfd_main;
   fi->fh = (intptr_t) mfd;

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

      $dlogdbg("opendir:sn(path=\"%s\")\n", path);

      mfd = malloc(sizeof(struct $mfd_t));
      if(unlikely(mfd == NULL)) {
         waserror = -ENOMEM;
         break;
      }

      do {

         // If we're opening the main /snapshots/ directory
         if(snpath->is_there == $$SNPATH_ROOT) {

            dp = opendir(fpath);
            if(unlikely(dp == NULL)) {
               waserror = -ENOMEM;
               break;
            }

            mfd->maindir = dp;
            mfd->is_main = $$mfd_sn_root;
            fi->fh = (intptr_t) mfd;
            break;

         }

         if(unlikely((waserror = $mfd_get_sn_steps(mfd, snpath, fsdata, $$SN_STEPS_F_DIR)) != 0)) {
            $dlogdbg("get sn steps failed with %d = %s\n", -waserror, strerror(-waserror));
            break;
         }

         // No directories found in any snapshot or the main part
         if(mfd->sn_first_file == -1) {
            if(unlikely((waserror = $mfd_destroy_sn_steps(mfd, fsdata)) != 0)) {
               $dlogdbg("destroy sn steps failed with %d = %s\n", -waserror, strerror(-waserror));
               break;
            }
            waserror = -ENOENT;
            break;
         }

         mfd->is_main = (snpath->is_there == $$SNPATH_FULL ? $$mfd_sn_full : $$mfd_sn_id);
         fi->fh = (intptr_t) mfd;

      } while(0);

      if(waserror < 0) {
         free(mfd);
      }

   } while(0);

   snret = waserror;

   $$ELIF_PATH_MAIN

   $dlogdbg("opendir:main(path=\"%s\")\n", path);

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

   return 0;

   $$FI_PATH
}


/* =================== Helper functions ===================== */

/* Opens a file, saves the part to be truncated, closes it.
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

      // Return here if the file did not exist.
      if(mfd->mapheader.exists == 0) {
         waserror = ENOENT;
         break;
      }

      ret = open(fpath, O_RDONLY);
      if(unlikely(ret == -1)) {
         waserror = errno;
         $dlogdbg("truncate(%s): failed to open main file err %d = %s\n", fpath, waserror, strerror(waserror));
         break;
      }
      mfd->mainfd = ret;

      // Put the inode into mfd
      // We have already stat'd the main file.
      // mfd->mapheader.fstat has been initialised as mapheader.exists == 1.
      mfd->main_inode = mfd->mapheader.fstat.st_ino;

      ret = $b_truncate(fsdata, mfd, newsize);
      if(unlikely(ret != 0)) {
         waserror = -ret;
         // Don't break here, we want to close mainfd anyway
      }

      close(mfd->mainfd);

   } while(0);

   // TODO CLEAN UP MAP/DAT FILES unnecessarily created?
   if(unlikely((ret = $mfd_close_sn(mfd)) != 0)) {
      $dlogdbg("_open_truncate_close(%s): mfd_close_sn failed err %d = %s\n", fpath, -ret, strerror(-ret));
      return ret;
   }

   return -waserror;

   // TODO add optimisation: set whole_saved if newsize==0
}
