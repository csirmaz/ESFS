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
//   int (*open) (const char *, struct fuse_file_info *);
int $open(const char *path, struct fuse_file_info *fi)
{
   int fd;
   struct $fd_t *mfd;
   $$IF_PATH_MAIN_ONLY

   log_msg(" open(path\"%s\", fi=0x%08x)\n", path, fi);

   mfd = malloc(sizeof(struct $fd_t));
   if(mfd == NULL){ return -ENOMEM; }

   mfd->is_main = 1;

   if((fi->flags & O_WRONLY) == 0 && (fi->flags & O_RDWR) == 0){ // opening for read-only
      $dlogdbg("Opening %s for read-only, skipping opening map file\n", fpath);
      mfd->mapfd = -2;
   }else{ // opening for writing
      // Save the current status of the file by initialising the map file.
      // We don't delete this even if the subsequent operation fails.
      $dlogdbg("Opening map file for %s\n", fpath);
      fd = $open_init_map(mfd, path, fpath, fsdata); // fd only stores a success flag here
      if(fd < 0){
         free(mfd);
         return -fd;
      }
   }

   fd = open(fpath, fi->flags);
   if(fd == -1){
      free(mfd);
      return -errno;
   }

   log_msg(" open success fd=%d\n", fd);

   mfd->mainfd = fd;
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
   struct $fd_t *mfd;
   $$IF_PATH_MAIN_ONLY

   log_msg("\ncreate(path=\"%s\", mode=0%03o, fi=0x%08x)\n", path, mode, fi);

   mfd = malloc(sizeof(struct $fd_t));
   if(mfd == NULL){ return -ENOMEM; }

   mfd->is_main = 1;

   // Save the current status of the file by initialising the map file.
   // We don't delete this even if the subsequent operation fails.
   fd = $open_init_map(mfd, path, fpath, fsdata); // fd only stores a success flag here
   if(fd < 0){
      free(mfd);
      return -fd;
   }

   fd = creat(fpath, mode);
   if(fd < 0){
      free(mfd);
      return -errno;
   }

   mfd->mainfd = fd;
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
   // log_msg("\nopendir(path=\"%s\", fi=0x%08x)\n", path, fi);
   DIR *dp;
   $$IF_PATH_MAIN_ONLY

   dp = opendir(fpath);
   if(unlikely(dp == NULL)){ return -errno; }

   fi->fh = (intptr_t) dp;

   return 0;
}


