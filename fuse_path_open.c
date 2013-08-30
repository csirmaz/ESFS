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
// TODO Use (-o atomic_o_trunc) and provide support for atomic_o_trunc (see above) for greater efficiency
//   int (*open) (const char *, struct fuse_file_info *);
int $open(const char *path, struct fuse_file_info *fi)
{
   int fd;
   int flags;
   int waserror = 0;
   struct $fd_t *mfd;
   struct stat mystat;

   $$IF_PATH_MAIN_ONLY // TODO need to open files in snapshots (for reading)

   mfd = malloc(sizeof(struct $fd_t));
   if(mfd == NULL){ return -ENOMEM; }

   do{
      flags = fi->flags;

      $dlogdbg("  open(%s, %s %s %s %s)\n", fpath,
               ((flags&O_TRUNC)>0)?"TRUNC":"", 
               ((flags&O_RDWR)>0)?"RDWR":"",
               ((flags&O_RDONLY)>0)?"RD":"",
               ((flags&O_WRONLY)>0)?"WR":"");

      if((flags & O_WRONLY) == 0 && (flags & O_RDWR) == 0){ // opening for read-only

         $n_open_rdonly(mfd);

      }else{ // opening for writing

         // Save the current status of the file by initialising the map file.
         // We don't delete this even if the subsequent operation fails.
         fd = $n_open(mfd, path, fpath, fsdata); // fd only stores a success flag here
         if(fd < 0){
            waserror = -fd; // converting -errno to +errno
            break;
         }

         // If the client wants to open O_WRONLY, we still need to read from the file to save
         // the overwritten blocks.
         // TODO Should we get a new filehandle to do this?
         if(flags & O_WRONLY){ flags ^= O_WRONLY; flags |= O_RDWR; }

      }

      do{

         fd = open(fpath, flags);
         if(fd == -1){
            waserror = errno;
            break;
         }
         mfd->mainfd = fd;

         do{

            // We need to store the inode no of the file.
            // We can only do that here after the open as it's possibly new
            // TODO but if it's not (see mapheader.exists), we could get the inode from $n_open!
            if(fstat(fd, &mystat) == -1){
               waserror = errno;
               break;
            }
            mfd->main_inode = mystat.st_ino;

         }while(0);

         if(waserror != 0){
            close(fd);
         }

      }while(0);

      if(waserror != 0){
         // TODO CLEAN UP MAP/DAT FILES if they have just been created?
         $n_close(mfd);
         break;
      }

   }while(0);

   if(waserror != 0){
      free(mfd);
      return -waserror;
   }

   log_msg("  open success main fd=%d\n", fd);

   fi->fh = (intptr_t) mfd;

   return 0;
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
//   int (*create) (const char *, mode_t, struct fuse_file_info *);
int $create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
   int fd;
   int waserror = 0;
   struct $fd_t *mfd;
   struct stat mystat;

   $$IF_PATH_MAIN_ONLY

   log_msg("  create(path=\"%s\", mode=0%03o, fi=0x%08x)\n", path, mode, fi);

   mfd = malloc(sizeof(struct $fd_t));
   if(mfd == NULL){ return -ENOMEM; }

   do{
      // Save the current status of the file by initialising the map file.
      // We don't delete this even if the subsequent operation fails.
      fd = $n_open(mfd, path, fpath, fsdata); // fd only stores a success flag here
      if(fd < 0){
         waserror = -fd; // Converting -errno to +errno;
         break;
      }

      do{

         fd = creat(fpath, mode);
         if(fd < 0){
            waserror = errno;
            break;
         }
         mfd->mainfd = fd;

         do{

            // We need to store the inode no of the file. We can only do that here as it's new
            if(fstat(fd, &mystat) == -1){
               waserror = errno;
               break;
            }
            mfd->main_inode = mystat.st_ino;

         }while(0);

         if(waserror != 0){
            close(fd);
         }

      }while(0);

      if(waserror != 0){
         // TODO CLEAN UP MAP/DAT FILES
         $n_close(mfd);
         break;
      }

   }while(0);

   if(waserror != 0){
      free(mfd);
      return -waserror;
   }

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
//   int (*opendir) (const char *, struct fuse_file_info *);
int $opendir(const char *path, struct fuse_file_info *fi)
{
   DIR *dp;
   $$IF_PATH_MAIN_ONLY // TODO need to open dirs in snapshots

   log_msg("  opendir(path=\"%s\", fi=0x%08x)\n", path, fi);

   dp = opendir(fpath);
   if(unlikely(dp == NULL)){ return -errno; }

   fi->fh = (intptr_t) dp;

   return 0;
}


/* =================== Helper functions ===================== */

/* Opens a file, saves the part to be truncated, closes it.
 * Returns 0 or -errno.
 */
static inline int $_open_truncate_close(struct $fsdata_t *fsdata, const char *path, const char *fpath, off_t newsize)
{
   struct $fd_t myfd;
   struct $fd_t *mfd;
   int ret;
   int waserror = 0;

   mfd = &myfd;

   if(unlikely((ret = $n_open(mfd, path, fpath, fsdata)) != 0)){
      return ret;
   }

   do{

      // Return here if the file did not exist.
      if(mfd->mapheader.exists == 0){
         waserror = ENOENT;
         break;
      }

      ret = open(fpath, O_RDONLY);
      if(unlikely(ret == -1)){
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
      if(unlikely(ret != 0)){
         waserror = -ret;
         // Don't break here, we want to close mainfd anyway
      }

      close(mfd->mainfd);

   }while(0);

   // TODO CLEAN UP MAP/DAT FILES unnecessarily created?
   if(unlikely((ret = $n_close(mfd)) != 0)){
      $dlogdbg("_open_truncate_close(%s): n_close failed err %d = %s\n", fpath, -ret, strerror(-ret));
      return ret;
   }

   return -waserror;

   // TODO add optimisation: set whole_saved if newsize==0
}
