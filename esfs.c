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

#include "params_c.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
// fcntl -- AT_FDCWD
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>
// sys/stat.h -- utimens
#include <sys/stat.h>

#include "log_c.h"

#include "util_c.c"
#include "snapshot_c.c"

// Report errors to logfile and give -errno to caller
// TODO Turn into inline function
static int $error(char *str)
{
   int ret = -errno;
   
   log_msg("   ERROR %s: %s\n", str, strerror(errno));
   
   return ret;
}



///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come from /usr/include/fuse.h


   /** Get file attributes.
   *
   * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
   * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
   * mount option is given.
   */
//   int (*getattr) (const char *, struct stat *);
int $getattr(const char *path, struct stat *statbuf)
{
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO

   log_msg("\ngetattr(path=\"%s\", statbuf=0x%08x)\n", path, statbuf);
   $fullpath(fpath, path);

   retstat = lstat(fpath, statbuf);
   if (retstat != 0) retstat = $error("getattr lstat");
   
   log_stat(statbuf);
   
   return retstat;
}

   /** Read the target of a symbolic link
   *
   * The buffer should be filled with a null terminated string.  The
   * buffer size argument includes the space for the terminating
   * null character.   If the linkname is too long to fit in the
   * buffer, it should be truncated.  The return value should be 0
   * for success.
   */
//   int (*readlink) (const char *, char *, size_t);
// Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to bb_readlink()
// bb_readlink() code by Bernardo F Costa (thanks!)
int $readlink(const char *path, char *link, size_t size)
{
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO

   log_msg("readlink(path=\"%s\", link=\"%s\", size=%d)\n", path, link, size);
   $fullpath(fpath, path);

   retstat = readlink(fpath, link, size - 1);
   if (retstat < 0){
     retstat = $error("readlink readlink");
   }else{
     link[retstat] = '\0';
     retstat = 0;
   }

   return retstat;
}

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
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO
   
   log_msg("mknod(path=\"%s\", mode=0%3o, dev=%lld)\n", path, mode, dev);
   $fullpath(fpath, path);
   
   retstat = mknod(fpath, mode, dev);
   if (retstat < 0) retstat = $error("mknod mknod");

   
   // On Linux this could just be 'mknod(path, mode, rdev)' but this
   //  is more portable -- Done; see above
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

   return retstat;
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
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO
   
   log_msg("\nmkdir(path=\"%s\", mode=0%3o)\n", path, mode);
   $fullpath(fpath, path);
   
   retstat = mkdir(fpath, mode);
   if (retstat < 0) retstat = $error("mkdir mkdir");
   
   return retstat;
}

   /** Remove a file */
//   int (*unlink) (const char *);
int $unlink(const char *path)
{
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO
   
   log_msg("unlink(path=\"%s\")\n", path);
   $fullpath(fpath, path);
   
   retstat = unlink(fpath);
   if (retstat < 0) retstat = $error("unlink unlink");
   
   return retstat;
}

   /** Remove a directory */
//   int (*rmdir) (const char *);
int $rmdir(const char *path)
{
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO
   
   log_msg("rmdir(path=\"%s\")\n", path);
   $fullpath(fpath, path);
   
   retstat = rmdir(fpath);
   if (retstat < 0) retstat = $error("rmdir rmdir");
   
   return retstat;
}

   /** Create a symbolic link */
//   int (*symlink) (const char *, const char *);
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int $symlink(const char *path, const char *link)
{
   int retstat = 0;
   char flink[PATH_MAX]; // TODO
   
   log_msg("\nsymlink(path=\"%s\", link=\"%s\")\n", path, link);
   $fullpath(flink, link);
   
   retstat = symlink(path, flink);
   if (retstat < 0) retstat = $error("symlink symlink");
   
   return retstat;
}

   /** Rename a file */
//   int (*rename) (const char *, const char *);
int $rename(const char *path, const char *newpath)
{
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO
   char fnewpath[PATH_MAX]; // TODO
   
   log_msg("\nrename(fpath=\"%s\", newpath=\"%s\")\n", path, newpath);
   $fullpath(fpath, path);
   $fullpath(fnewpath, newpath);
   
   retstat = rename(fpath, fnewpath);
   if (retstat < 0) retstat = $error("rename rename");
   
   return retstat;
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
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO
   
   log_msg("\nchmod(fpath=\"%s\", mode=0%03o)\n", path, mode);
   $fullpath(fpath, path);
   
   retstat = chmod(fpath, mode);
   if (retstat < 0) retstat = $error("chmod chmod");
   
   return retstat;
}

   /** Change the owner and group of a file */
//   int (*chown) (const char *, uid_t, gid_t);
int $chown(const char *path, uid_t uid, gid_t gid)
{
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO

   log_msg("\nchown(path=\"%s\", uid=%d, gid=%d)\n", path, uid, gid);
   $fullpath(fpath, path);
   
   retstat = chown(fpath, uid, gid);
   if (retstat < 0) retstat = $error("chown chown");
   
   return retstat;
}

   /** Change the size of a file */
//   int (*truncate) (const char *, off_t);
int $truncate(const char *path, off_t newsize)
{
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO
   
   log_msg("\ntruncate(path=\"%s\", newsize=%lld)\n", path, newsize);
   $fullpath(fpath, path);
   
   retstat = truncate(fpath, newsize);
   if (retstat < 0) retstat = $error("truncate truncate");
   
   return retstat;
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
   int retstat = 0;
   int fd;
   char fpath[PATH_MAX]; // TODO
   
   log_msg("\nopen(path\"%s\", fi=0x%08x)\n", path, fi);
   $fullpath(fpath, path);
   
   fd = open(fpath, fi->flags);
   if (fd < 0) retstat = $error("open open");
   
   fi->fh = fd;
   
   return retstat;
}


   /** Read data from an open file
    *
    * Read should return exactly the number of bytes requested except
    * on EOF or error, otherwise the rest of the data will be
    * substituted with zeroes.    An exception to this is when the
    * 'direct_io' mount option is specified, in which case the return
    * value of the read system call will reflect the return value of
    * this operation.
    *
    * Changed in version 2.2
    */
//   int (*read) (const char *, char *, size_t, off_t,
//           struct fuse_file_info *);
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.
int $read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
   int retstat = 0;
   
   log_msg("\nread(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);
   
   retstat = pread(fi->fh, buf, size, offset);
   if (retstat < 0) retstat = $error("read read");
   
   return retstat;
}


   /** Write data to an open file
    *
    * Write should return exactly the number of bytes requested
    * except on error.   An exception to this is when the 'direct_io'
    * mount option is specified (see read operation).
    *
    * Changed in version 2.2
    */
//   int (*write) (const char *, const char *, size_t, off_t,
//            struct fuse_file_info *);
// As  with read(), the documentation above is inconsistent with the
// documentation for the write() system call.
int $write(const char *path, const char *buf, size_t size, off_t offset,
          struct fuse_file_info *fi)
{
   int retstat = 0;

   log_msg("\nwrite(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);

   retstat = pwrite(fi->fh, buf, size, offset);
   if (retstat < 0) retstat = $error("write pwrite");

   return retstat;
}

   /** Get file system statistics
    *
    * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
    *
    * Replaced 'struct statfs' parameter with 'struct statvfs' in
    * version 2.5
    */
//   int (*statfs) (const char *, struct statvfs *);
int $statfs(const char *path, struct statvfs *statv)
{
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO
   
   log_msg("\nstatfs(path=\"%s\", statv=0x%08x)\n", path, statv);
   $fullpath(fpath, path);
   
   // get stats for underlying filesystem
   retstat = statvfs(fpath, statv);
   if (retstat < 0) retstat = $error("statfs statvfs");
   
   log_statvfs(statv);
   
   return retstat;
}


   /** Possibly flush cached data
    *
    * BIG NOTE: This is not equivalent to fsync().  It's not a
    * request to sync dirty data.
    *
    * Flush is called on each close() of a file descriptor.  So if a
    * filesystem wants to return write errors in close() and the file
    * has cached dirty data, this is a good place to write back data
    * and return any errors.  Since many applications ignore close()
    * errors this is not always useful.
    *
    * NOTE: The flush() method may be called more than once for each
    * open().  This happens if more than one file descriptor refers
    * to an opened file due to dup(), dup2() or fork() calls.  It is
    * not possible to determine if a flush is final, so each flush
    * should be treated equally.  Multiple write-flush sequences are
    * relatively rare, so this shouldn't be a problem.
    *
    * Filesystems shouldn't assume that flush will always be called
    * after some writes, or that if will be called at all.
    *
    * Changed in version 2.2
    */
//   int (*flush) (const char *, struct fuse_file_info *);
int $flush(const char *path, struct fuse_file_info *fi)
{
   int retstat = 0;
   
   log_msg("\nflush(path=\"%s\", fi=0x%08x)\n", path, fi);

   // TODO THIS IS IGNORED
   
   return retstat;
}


   /** Release an open file
    *
    * Release is called when there are no more references to an open
    * file: all file descriptors are closed and all memory mappings
    * are unmapped.
    *
    * For every open() call there will be exactly one release() call
    * with the same flags and file descriptor.   It is possible to
    * have a file opened more than once, in which case only the last
    * release will mean, that no more reads/writes will happen on the
    * file.  The return value of release is ignored.
    *
    * Changed in version 2.2
    */
//   int (*release) (const char *, struct fuse_file_info *);
int $release(const char *path, struct fuse_file_info *fi)
{
   int retstat = 0;
   
   log_msg("\nrelease(path=\"%s\", fi=0x%08x)\n", path, fi);

   // We need to close the file.  Had we allocated any resources
   // (buffers etc) we'd need to free them here as well.
   retstat = close(fi->fh);
   // TODO no $error?
   
   return retstat;
}

   /** Synchronize file contents
    *
    * If the datasync parameter is non-zero, then only the user data
    * should be flushed, not the meta data.
    *
    * Changed in version 2.2
    */
//   int (*fsync) (const char *, int, struct fuse_file_info *);
int $fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
   int retstat = 0;
   
   log_msg("\nfsync(path=\"%s\", datasync=%d, fi=0x%08x)\n", path, datasync, fi);
   
   if (datasync){
      retstat = fdatasync(fi->fh);
      // fdatasync()  is  similar  to  fsync(),  but  does  not flush modified metadata unless that metadata is needed
   }else{
      retstat = fsync(fi->fh);
      // fsync() transfers ("flushes") all modified in-core data of (i.e., modified buffer cache pages for) 
      // the file referred to by the file descriptor fd to the disk device
   }
   
   if (retstat < 0) $error("fsync");
   
   return retstat;
}


   /** Set extended attributes */
//   int (*setxattr) (const char *, const char *, const char *, size_t, int);
int $setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO
   
   log_msg("\nsetxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n", path, name, value, size, flags);
   $fullpath(fpath, path);
   
   retstat = lsetxattr(fpath, name, value, size, flags);
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
   if (retstat < 0) retstat = $error("setxattr lsetxattr");
   
   return retstat;
}

   /** Get extended attributes */
//   int (*getxattr) (const char *, const char *, char *, size_t);
int $getxattr(const char *path, const char *name, char *value, size_t size)
{
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO
   
   log_msg("\ngetxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = %d)\n", path, name, value, size);
   $fullpath(fpath, path);
   
   retstat = lgetxattr(fpath, name, value, size);
   if (retstat < 0) retstat = $error("getxattr lgetxattr");
   
   return retstat;
}

   /** List extended attributes */
//   int (*listxattr) (const char *, char *, size_t);
int $listxattr(const char *path, char *list, size_t size)
{
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO
   
   log_msg("listxattr(path=\"%s\", list=0x%08x, size=%d)\n", path, list, size);
   $fullpath(fpath, path);
   
   retstat = llistxattr(fpath, list, size);
   if (retstat < 0) retstat = $error("listxattr llistxattr");
   
   return retstat;
}


   /** Remove extended attributes */
//   int (*removexattr) (const char *, const char *);
int $removexattr(const char *path, const char *name)
{
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO
   
   log_msg("\nremovexattr(path=\"%s\", name=\"%s\")\n", path, name);
   $fullpath(fpath, path);
   
   retstat = lremovexattr(fpath, name);
   if (retstat < 0) retstat = $error("removexattr lrmovexattr");
   
   return retstat;
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
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO
   
   log_msg("\nopendir(path=\"%s\", fi=0x%08x)\n", path, fi);
   $fullpath(fpath, path);
   
   dp = opendir(fpath);
   if (dp == NULL) retstat = $error("opendir opendir");
   
   fi->fh = (intptr_t) dp;
   
   return retstat;
}

   /** Read directory
    *
    * This supersedes the old getdir() interface.  New applications
    * should use this.
    *
    * The filesystem may choose between two modes of operation:
    *
    * 1) The readdir implementation ignores the offset parameter, and
    * passes zero to the filler function's offset.  The filler
    * function will not return '1' (unless an error happens), so the
    * whole directory is read in a single readdir operation.  This
    * works just like the old getdir() method.
    *
    * 2) The readdir implementation keeps track of the offsets of the
    * directory entries.  It uses the offset parameter and always
    * passes non-zero offset to the filler function.  When the buffer
    * is full (or an error happens) the filler function will return
    * '1'.
    *
    * Introduced in version 2.3
    */
//   int (*readdir) (const char *, void *, fuse_fill_dir_t, off_t,
//         struct fuse_file_info *);
int $readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
            struct fuse_file_info *fi)
{
   int retstat = 0;
   DIR *dp;
   struct dirent *de;
   
   log_msg("\nreaddir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n", path, buf, filler, offset, fi);
   // note that I need to cast fi->fh
   dp = (DIR *) (uintptr_t) fi->fh;

   // Every directory contains at least two entries: . and ..  If my
   // first call to the system readdir() returns NULL I've got an
   // error; near as I can tell, that's the only condition under
   // which I can get an error from readdir()
   de = readdir(dp);
   if (de == 0) {
      retstat = $error("readdir readdir");
      return retstat;
   }

   // This will copy the entire directory into the buffer.  The loop exits
   // when either the system readdir() returns NULL, or filler()
   // returns something non-zero.  The first case just means I've
   // read the whole directory; the second means the buffer is full.
   do {
      if (filler(buf, de->d_name, NULL, 0) != 0) {
         return -ENOMEM;
      }
   } while ((de = readdir(dp)) != NULL);
   
   return retstat;
}

   /** Release directory
    *
    * Introduced in version 2.3
    */
//   int (*releasedir) (const char *, struct fuse_file_info *);
int $releasedir(const char *path, struct fuse_file_info *fi)
{
   int retstat = 0;
   
   log_msg("\nreleasedir(path=\"%s\", fi=0x%08x)\n", path, fi);
   
   closedir((DIR *) (uintptr_t) fi->fh);
   
   return retstat;
}

   /** Synchronize directory contents
    *
    * If the datasync parameter is non-zero, then only the user data
    * should be flushed, not the meta data
    *
    * Introduced in version 2.3
    */
//   int (*fsyncdir) (const char *, int, struct fuse_file_info *);
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ???
int $fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
   int retstat = 0;
   
   log_msg("\nfsyncdir(path=\"%s\", datasync=%d, fi=0x%08x)\n", path, datasync, fi);
   
   // TODO This is ignored
   
   return retstat;
}

   /**
    * Initialize filesystem
    *
    * The return value will passed in the private_data field of
    * fuse_context to all file operations and as a parameter to the
    * destroy() method.
    *
    * Introduced in version 2.3
    * Changed in version 2.6
    */
//   void *(*init) (struct fuse_conn_info *conn);
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
void *$init(struct fuse_conn_info *conn)
{
   return $$FSDATA; // TODO -- We put user_data into fuse_context
}

   /**
    * Clean up filesystem
    *
    * Called on filesystem exit.
    *
    * Introduced in version 2.3
    */
//   void (*destroy) (void *);
void $destroy(void *userdata)
{
   log_msg("\ndestroy(userdata=0x%08x)\n", userdata);
}

   /**
    * Check file access permissions
    *
    * This will be called for the access() system call.  If the
    * 'default_permissions' mount option is given, this method is not
    * called.
    *
    * This method is not called under Linux kernel versions 2.4.x
    *
    * Introduced in version 2.5
    */
//   int (*access) (const char *, int);
int $access(const char *path, int mask)
{
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO
   
   log_msg("\naccess(path=\"%s\", mask=0%o)\n", path, mask);
   $fullpath(fpath, path);
   
   retstat = access(fpath, mask);
   
   if (retstat < 0) retstat = $error("access access");
   
   return retstat;
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
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO
   int fd;
   
   log_msg("\ncreate(path=\"%s\", mode=0%03o, fi=0x%08x)\n", path, mode, fi);
   $fullpath(fpath, path);
   
   fd = creat(fpath, mode);
   if (fd < 0) retstat = $error("create creat");
   
   fi->fh = fd;
   
   return retstat;
}

   /**
    * Change the size of an open file
    *
    * This method is called instead of the truncate() method if the
    * truncation was invoked from an ftruncate() system call.
    *
    * If this method is not implemented or under Linux kernel
    * versions earlier than 2.6.15, the truncate() method will be
    * called instead.
    *
    * Introduced in version 2.5
    */
//   int (*ftruncate) (const char *, off_t, struct fuse_file_info *);
int $ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
   int retstat = 0;
   
   log_msg("\nftruncate(path=\"%s\", offset=%lld, fi=0x%08x)\n", path, offset, fi);
   
   retstat = ftruncate(fi->fh, offset);
   if (retstat < 0) retstat = $error("ftruncate ftruncate");
   
   return retstat;
}

   /**
    * Get attributes from an open file
    *
    * This method is called instead of the getattr() method if the
    * file information is available.
    *
    * Currently this is only called after the create() method if that
    * is implemented (see above).  Later it may be called for
    * invocations of fstat() too.
    *
    * Introduced in version 2.5
    */
//   int (*fgetattr) (const char *, struct stat *, struct fuse_file_info *);
// Since it's currently only called after bb_create(), and bb_create()
// opens the file, I ought to be able to just use the fd and ignore
// the path...
int $fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
   int retstat = 0;
   
   log_msg("\nfgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n", path, statbuf, fi);
   
   retstat = fstat(fi->fh, statbuf);
   if (retstat < 0) retstat = $error("fgetattr fstat");
   
   return retstat;
}

   /**
    * Perform POSIX file locking operation
    *
    * The cmd argument will be either F_GETLK, F_SETLK or F_SETLKW.
    *
    * For the meaning of fields in 'struct flock' see the man page
    * for fcntl(2).  The l_whence field will always be set to
    * SEEK_SET.
    *
    * For checking lock ownership, the 'fuse_file_info->owner'
    * argument must be used.
    *
    * For F_GETLK operation, the library will first check currently
    * held locks, and if a conflicting lock is found it will return
    * information without calling this method.   This ensures, that
    * for local locks the l_pid field is correctly filled in.  The
    * results may not be accurate in case of race conditions and in
    * the presence of hard links, but it's unlikely that an
    * application would rely on accurate GETLK results in these
    * cases.  If a conflicting lock is not found, this method will be
    * called, and the filesystem may fill out l_pid by a meaningful
    * value, or it may leave this field zero.
    *
    * For F_SETLK and F_SETLKW the l_pid field will be set to the pid
    * of the process performing the locking operation.
    *
    ** Note: if this method is not implemented, the kernel will still
    ** allow file locking to work locally.  Hence it is only
    ** interesting for network filesystems and similar.
    *
    * Introduced in version 2.6
    */
//   int (*lock) (const char *, struct fuse_file_info *, int cmd,
//           struct flock *);

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
   int retstat = 0;
   char fpath[PATH_MAX]; // TODO
   
   log_msg("\nutimens\n");
   $fullpath(fpath, path);
   
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
   
   retstat = utimensat(AT_FDCWD, fpath, tv, 0); // TODO For now, we fall back to AT_FDCWD
   if (retstat < 0) retstat = $error("utimens utimensat");
   
   return retstat;
}

   /**
    * Map block index within file to block index within device
    *
    * Note: This makes sense only for block device backed filesystems
    * mounted with the 'blkdev' option
    *
    * Introduced in version 2.6
    */
//   int (*bmap) (const char *, size_t blocksize, uint64_t *idx);

   /**
    * Flag indicating that the filesystem can accept a NULL path
    * as the first argument for the following operations:
    *
    * read, write, flush, release, fsync, readdir, releasedir,
    * fsyncdir, ftruncate, fgetattr, lock, ioctl and poll
    *
    * If this flag is set these operations continue to work on
    * unlinked files even if "-ohard_remove" option was specified.
    */
//   unsigned int flag_nullpath_ok:1;

   /**
    * Flag indicating that the path need not be calculated for
    * the following operations:
    *
    * read, write, flush, release, fsync, readdir, releasedir,
    * fsyncdir, ftruncate, fgetattr, lock, ioctl and poll
    *
    * Closely related to flag_nullpath_ok, but if this flag is
    * set then the path will not be calculaged even if the file
    * wasn't unlinked.  However the path can still be non-NULL if
    * it needs to be calculated for some other reason.
    */
//   unsigned int flag_nopath:1;

   /**
    * Flag indicating that the filesystem accepts special
    * UTIME_NOW and UTIME_OMIT values in its utimens operation.
    */
//   unsigned int flag_utime_omit_ok:1;

   /**
    * Reserved flags, don't set
    */
//   unsigned int flag_reserved:29;

   /**
    * Ioctl
    *
    * flags will have FUSE_IOCTL_COMPAT set for 32bit ioctls in
    * 64bit environment.  The size and direction of data is
    * determined by _IOC_*() decoding of cmd.  For _IOC_NONE,
    * data will be NULL, for _IOC_WRITE data is out area, for
    * _IOC_READ in area and if both are set in/out area.  In all
    * non-NULL cases, the area is of _IOC_SIZE(cmd) bytes.
    *
    * Introduced in version 2.8
    */
//   int (*ioctl) (const char *, int cmd, void *arg,
//            struct fuse_file_info *, unsigned int flags, void *data);

   /**
    * Poll for IO readiness events
    *
    * Note: If ph is non-NULL, the client should notify
    * when IO readiness events occur by calling
    * fuse_notify_poll() with the specified ph.
    *
    * Regardless of the number of times poll with a non-NULL ph
    * is received, single notification is enough to clear all.
    * Notifying more times incurs overhead but doesn't harm
    * correctness.
    *
    * The callee is responsible for destroying ph with
    * fuse_pollhandle_destroy() when no longer in use.
    *
    * Introduced in version 2.8
    */
//   int (*poll) (const char *, struct fuse_file_info *,
//           struct fuse_pollhandle *ph, unsigned *reventsp);

   /** Write contents of buffer to an open file
    *
    * Similar to the write() method, but data is supplied in a
    * generic buffer.  Use fuse_buf_copy() to transfer data to
    * the destination.
    *
    * Introduced in version 2.9
    */
//   int (*write_buf) (const char *, struct fuse_bufvec *buf, off_t off,
//           struct fuse_file_info *);

   /** Store data from an open file in a buffer
    *
    * Similar to the read() method, but data is stored and
    * returned in a generic buffer.
    *
    * No actual copying of data has to take place, the source
    * file descriptor may simply be stored in the buffer for
    * later data transfer.
    *
    * The buffer must be allocated dynamically and stored at the
    * location pointed to by bufp.  If the buffer contains memory
    * regions, they too must be allocated using malloc().  The
    * allocated memory will be freed by the caller.
    *
    * Introduced in version 2.9
    */
//   int (*read_buf) (const char *, struct fuse_bufvec **bufp,
//          size_t size, off_t off, struct fuse_file_info *);

  /**
    * Perform BSD file locking operation
    *
    * The op argument will be either LOCK_SH, LOCK_EX or LOCK_UN
    *
    * Nonblocking requests will be indicated by ORing LOCK_NB to
    * the above operations
    *
    * For more information see the flock(2) manual page.
    *
    * Additionally fi->owner will be set to a value unique to
    * this open file.  This same value will be supplied to
    * ->release() when the file is released.
    *
    ** Note: if this method is not implemented, the kernel will still
    ** allow file locking to work locally.  Hence it is only
    ** interesting for network filesystems and similar.
    *
    * Introduced in version 2.9
    */
//   int (*flock) (const char *, struct fuse_file_info *, int op);


struct fuse_operations $oper = {
  .getattr = $getattr,
  .readlink = $readlink,
  .mknod   = $mknod,
  .mkdir   = $mkdir,
  .unlink  = $unlink,
  .rmdir   = $rmdir,
  .symlink = $symlink,
  .rename  = $rename,
  .link    = $link,
  .chmod   = $chmod,
  .chown   = $chown,
  .truncate = $truncate,
  .open    = $open,
  .read    = $read,
  .write   = $write,
  .statfs  = $statfs,
  .flush   = $flush,
  .release = $release,
  .fsync     = $fsync,
  .setxattr  = $setxattr,
  .getxattr  = $getxattr,
  .listxattr = $listxattr,
  .removexattr = $removexattr,
  .opendir    = $opendir,
  .readdir    = $readdir,
  .releasedir = $releasedir,
  .fsyncdir   = $fsyncdir,
  .init    = $init,
  .destroy = $destroy,
  .access  = $access,
  .create  = $create,
  .ftruncate = $ftruncate,
  .fgetattr  = $fgetattr,
  .utimens   = $utimens
};

void $usage()
{
   fprintf(stderr, "usage:  esfs [FUSE and mount options] rootDir mountPoint\n");
   abort();
}

int main(int argc, char *argv[])
{
   struct $fsdata *$myfsdata;

   // ESFS doesn't do any access checking on its own (the comment
   // blocks in fuse.h mention some of the functions that need
   // accesses checked -- but note there are other functions, like
   // chown(), that also need checking!).  Since running bbfs as root
   // will therefore open Metrodome-sized holes in the system
   // security, we'll check if root is trying to mount the filesystem
   // and refuse if it is.  The somewhat smaller hole of an ordinary
   // user doing it with the allow_other flag is still there because
   // I don't want to parse the options string.
   if ((getuid() == 0) || (geteuid() == 0)) {
      fprintf(stderr, "Running ESFS as root opens unnacceptable security holes\n");
      return 1;
   }

   // Perform some sanity checking on the command line:  make sure
   // there are enough arguments, and that neither of the last two
   // start with a hyphen (this will break if you actually have a
   // rootpoint or mountpoint whose name starts with a hyphen, but so
   // will a zillion other programs)
   if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
      $usage();

   $myfsdata = malloc(sizeof(struct $fsdata));
   if ($myfsdata == NULL) {
      perror("main calloc");
      abort();
   }

   // Pull the rootdir out of the argument list and save it in my
   // internal data
   $myfsdata->rootdir = realpath(argv[argc-2], NULL);
   argv[argc-2] = argv[argc-1];
   argv[argc-1] = NULL;
   argc--;
   
   $myfsdata->logfile = log_open();
   
   // turn over control to fuse
   return fuse_main(argc, argv, &$oper, $myfsdata);
}
