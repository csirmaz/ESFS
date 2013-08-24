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


   /** Create a file node
    *
    * This is called for creation of all non-directory, non-symlink
    * nodes.  If the filesystem defines a create() method, then for
    * regular files that will be called instead.
    */
   // Regular files - for example, not a FIFO
//   int (*mknod) (const char *, mode_t, dev_t);
int $mknod(const char *path, mode_t mode, dev_t dev)
{
   // Special files are not allowed in ESFS.
   // As create() is defined, we can return here.
   return -EPERM;

   // retstat = mknod(fpath, mode, dev);

   // On Linux this could just be 'mknod(path, mode, rdev)' but this
   //  is more portable
   /*
   if (S_ISREG(mode)) {
      retstat = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
      if (retstat < 0){
         retstat = $error("mknod open");
      }else{
         retstat = close(retstat);
         if (retstat < 0) retstat = $error("mknod close");
      }
   }else{
      if (S_ISFIFO(mode)) {
         retstat = mkfifo(fpath, mode);
         if (retstat < 0) retstat = $error("mknod mkfifo");
      } else {
         retstat = mknod(fpath, mode, dev);
         if (retstat < 0) retstat = $error("mknod mknod");
      }
   }
   */
}


   /** Create a directory
    *
    * Note that the mode argument may not have the type specification
    * bits set, i.e. S_ISDIR(mode) can be false.  To obtain the
    * correct directory type bits use  mode|S_IFDIR
    * */
//   int (*mkdir) (const char *, mode_t);
int $mkdir(const char *path, mode_t mode)
{
   $$IF_PATH_MAIN_ONLY

   log_msg("mkdir(path=\"%s\", mode=0%3o)\n", path, mode);

   if(mkdir(fpath, mode) == 0){ return 0; }
   return -errno;
}


   /** Remove a file */
//   int (*unlink) (const char *);
int $unlink(const char *path)
{
   // log_msg("unlink(path=\"%s\")\n", path);
   $$IF_PATH_MAIN_ONLY

   if(unlink(fpath) == 0){ return 0; }
   return -errno;
}


   /** Remove a directory */
//   int (*rmdir) (const char *);
int $rmdir(const char *path)
{
   // log_msg("rmdir(path=\"%s\")\n", path);
   $$IF_PATH_MAIN_ONLY

   if(rmdir(fpath) == 0){ return 0; }
   return -errno;
}


   /** Create a symbolic link */
//   int (*symlink) (const char *, const char *);
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  (1) is where the link points,
// while (2) is the link itself.  So we need to leave (1)
// unaltered, but insert the link into the mounted directory.
int $symlink(const char *dest, const char *path)
{
   $$IF_PATH_MAIN_ONLY

   log_msg("\nsymlink(dest=\"%s\", path=\"%s\")\n", dest, path);

   if(symlink(dest, fpath) == 0){ return 0; }
   return -errno;
}


   /** Rename a file */
//   int (*rename) (const char *, const char *);
int $rename(const char *path, const char *newpath)
{
   $$IF_MULTI_PATHS_MAIN_ONLY

   log_msg("\nrename(fpath=\"%s\", newpath=\"%s\")\n", path, newpath);

   if(rename(fpath, fnewpath) == 0){ return 0; }
   return -errno;
}


   /** Create a hard link to a file */
//   int (*link) (const char *, const char *);
int $link(const char *path, const char *newpath)
{
   // This is not allowed in ESFS.
   return -EPERM;

   // To pass the request on, call link(fpath, fnewpath);
}


   /** Change the permission bits of a file */
//   int (*chmod) (const char *, mode_t);
int $chmod(const char *path, mode_t mode)
{
   // log_msg("\nchmod(fpath=\"%s\", mode=0%03o)\n", path, mode);
   $$IF_PATH_MAIN_ONLY

   if(chmod(fpath, mode) == 0){ return 0; }
   return -errno;
}


   /** Change the owner and group of a file */
//   int (*chown) (const char *, uid_t, gid_t);
int $chown(const char *path, uid_t uid, gid_t gid)
{
   // log_msg("\nchown(path=\"%s\", uid=%d, gid=%d)\n", path, uid, gid);
   $$IF_PATH_MAIN_ONLY

   if(chown(fpath, uid, gid) == 0){ return 0; }
   return -errno;
}


   /** Change the size of a file */
//   int (*truncate) (const char *, off_t);
int $truncate(const char *path, off_t newsize)
{
   // log_msg("\ntruncate(path=\"%s\", newsize=%lld)\n", path, newsize);
   $$IF_PATH_MAIN_ONLY

   if(truncate(fpath, newsize) == 0){ return 0; }
   return -errno;
}


   /** Change the access and/or modification times of a file
    *
    * Deprecated, use utimens() instead.
    */
//   int (*utime) (const char *, struct utimbuf *);
/*
int $utime(const char *path, struct utimbuf *ubuf)
{
   ...
   retstat = utime(fpath, ubuf);
   ...
}
*/


   /** Set extended attributes */
//   int (*setxattr) (const char *, const char *, const char *, size_t, int);
int $setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
   // log_msg("\nsetxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n", path, name, value, size, flags);
   $$IF_PATH_MAIN_ONLY

   if(lsetxattr(fpath, name, value, size, flags) == 0){ return 0; }
   return -errno;
   /*
   setxattr() sets the value of the extended attribute identified by name and associated with
   the given path in the file system. The size of the value must be specified.
   lsetxattr() is identical to setxattr(), except in the case of a symbolic link,
   where the extended attribute is set on the link itself, not the file that it refers to.
   The flags argument can be used to refine the semantics of the operation.
   XATTR_CREATE specifies a pure create, which fails if the named attribute exists already.
   XATTR_REPLACE specifies a pure replace operation, which fails if the named attribute does not already exist.
   By default (no flags), the extended attribute will be created if need be,
   or will simply replace the value if the attribute exists.
   */
}


   /** Remove extended attributes */
//   int (*removexattr) (const char *, const char *);
int $removexattr(const char *path, const char *name)
{
   // log_msg("\nremovexattr(path=\"%s\", name=\"%s\")\n", path, name);
   $$IF_PATH_MAIN_ONLY

   if(lremovexattr(fpath, name) == 0){ return 0; }
   return -errno;
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
   $$IF_PATH_MAIN_ONLY

   log_msg("\ncreate(path=\"%s\", mode=0%03o, fi=0x%08x)\n", path, mode, fi);

   fd = creat(fpath, mode);
   if(fd < 0){ return -errno; }

   fi->fh = fd;

   return 0;
}


   /**
    * Change the access and modification times of a file with
    * nanosecond resolution
    *
    * This supersedes the old utime() interface.  New applications
    * should use this.
    *
    * See the utimensat(2) man page for details.
    * @param path is the path to the file
    * @param tv An array of two struct timespec.  The first member is atime, the second is mtime.
    *
    * Introduced in version 2.6
    */
//   int (*utimens) (const char *, const struct timespec tv[2]);
int $utimens(const char *path, const struct timespec tv[2])
{
   $$IF_PATH_MAIN_ONLY

   /*
    int utimensat(int dirfd, const char *pathname,
                     const struct timespec times[2], int flags);

      If pathname is relative, then by default it is interpreted relative
       to the directory referred to by the open file descriptor, dirfd
       (rather than relative to the current working directory of the calling
       process, as is done by utimes(2) for a relative pathname).  See
       openat(2) for an explanation of why this can be useful.

       If pathname is relative and dirfd is the special value AT_FDCWD, then
       pathname is interpreted relative to the current working directory of
       the calling process (like utimes(2)).

       If pathname is absolute, then dirfd is ignored.

       The flags field is a bit mask that may be 0, or include the following
       constant, defined in <fcntl.h>:

       AT_SYMLINK_NOFOLLOW
              If pathname specifies a symbolic link, then update the
              timestamps of the link, rather than the file to which it
              refers.
   */

   if(utimensat(AT_FDCWD, fpath, tv, 0) == 0){ // TODO For now, we fall back to AT_FDCWD
      return 0;
   }
   return -errno;
}
