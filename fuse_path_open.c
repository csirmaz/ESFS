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

      fd = open(fpath, flags);
      if(fd == -1){
         waserror = errno;
         // TODO CLEAN UP MAP/DAT FILES (if opening for writing)
         break;
      }

      // We need to store the inode no of the file.
      // We can only do that here after the open as it's possibly new
      // TODO but if it's not, we could get the inode from $n_open!
      if(fstat(fd, &mystat) == -1){
         waserror = errno;
         break;
      }

   }while(0);

   if(waserror != 0){
      $n_close(mfd);
      free(mfd);
      return -waserror;
   }

   log_msg("  open success main fd=%d\n", fd);

   mfd->mainfd = fd;
   mfd->main_inode = mystat.st_ino;
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

      fd = creat(fpath, mode);
      if(fd < 0){
         waserror = errno;
         // TODO CLEAN UP MAP/DAT FILES
         break;
      }

      // We need to store the inode no of the file. We can only do that here as it's new
      if(fstat(fd, &mystat) == -1){
         waserror = errno;
         // TODO CLEAN UP MAP/DAT FILES
         break;
      }
   }while(0);

   if(waserror != 0){
      $n_close(mfd);
      free(mfd);
      return -waserror;
   }

   mfd->mainfd = fd;
   mfd->main_inode = mystat.st_ino;
   fi->fh = (intptr_t) mfd;

   return 0;
}



   /** Change the size of a file */
//   int (*truncate) (const char *, off_t);
int $truncate(const char *path, off_t newsize)
{
   struct $fd_t mfd;
   int ret;
   int waserror = 0;

   $$IF_PATH_MAIN_ONLY

   log_msg("  trunc(path=\"%s\" size=%zu)\n", path, newsize);

   // TODO should there be a lock for the whole of truncate?

   if(unlikely((ret = $n_open(&(mfd), path, fpath, fsdata)) != 0)){ return ret; }

   /////////////////////////////////////////////
   // TODO                                    //
   // Return here if the file does not exist. //
   // Put the inode into mfd.                 //
   /////////////////////////////////////////////

   do{
      ret = open(fpath, O_RDONLY);
      if(ret == -1){
         waserror = errno;
         // TODO CLEAN UP MAP/DAT FILES
         break;
      }

      mfd.mainfd = ret;

      do{

         // If the file existed and was larger than newsize, save the blocks
         // NB Any blocks outside the current main file should have already been saved
         if(mfd.mapheader.exists == 1 && newsize < mfd.mapheader.fstat.st_size){
            ret = $b_write(fsdata, &(mfd), mfd.mapheader.fstat.st_size - newsize, newsize);
            if(ret != 0){
               waserror = -ret;
               break;
            }
         }

         if(truncate(fpath, newsize) != 0){
            waserror = errno;
            break;
         }

      }while(0);

      close(mfd.mainfd);

   }while(0);

   if(unlikely((ret = $n_close(&(mfd))) != 0)){
      return ret;
   }

   return -waserror;

   // TODO add optimisation: copy file to snapshot if that's faster?
   // TODO add optimisation: set whole_saved if newsize==0

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


