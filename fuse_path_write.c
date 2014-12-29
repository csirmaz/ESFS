/*
  This file is part of ESFS, a FUSE-based filesystem that supports snapshots.
  ESFS is Copyright (C) 2013, 2014 Elod Csirmaz
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


/* This file contains FS operations that modify dentries based on a path
 *
 * SPECIAL COMMANDS
 * ================
 *
 * Issuing `mkdir /snapshots/[ID]` creates a new snapshot.
 * Issuing `rmdir /snapshots` removes the earliest snapshot.
 */


/** Create a directory or a snapshot
 *
 * Issuing `mkdir /snapshots/[ID]` creates a new snapshot.
 *
 * Note that the mode argument may not have the type specification
 * bits set, i.e. S_ISDIR(mode) can be false.  To obtain the
 * correct directory type bits use  mode|S_IFDIR
 */
int $mkdir(const char *path, mode_t mode)
{
   int ret;
   $$IF_PATH_SN

   // Create a new snapshot

   if(snpath->is_there != $$snpath_id) {
      $dlogi("ERROR mkdir: bad path given\n");
      snret = -EFAULT; // "Bad address" if wrong path was used
   } else if($sn_create(fsdata, fpath) != 0) {
      $dlogi("ERROR Could not create snapshot\n");
      snret = -EIO; // If error occurred while trying to create snapshot; check logs.
   } else {
      snret = 0;
   }

   $$ELIF_PATH_MAIN

   $dlogdbg("* mkdir(path=\"%s\", mode=0%3o)\n", path, mode);

   // Mark dir as nonexistent in snapshot.
   // This will create a DIRNAME.map file.
   if(unlikely((ret = $mfd_init_sn(path, fpath, fsdata)) != 0)) {
      $dlogi("ERROR mfd_init_sn failed with %d = %s\n", -ret, strerror(-ret));
      return ret;
   }

   if(mkdir(fpath, mode) == 0) { return 0; }
   return -errno;

   $$FI_PATH
}


/** Remove a directory or a snapshot
 *
 * Issuing `rmdir /snapshots` removes the earliest snapshot.
 */
int $rmdir(const char *path)
{
   $$IF_PATH_SN

   // Remove the latest snapshot
   $dlogdbg("About to remove snapshot path: %s fpath: %s is_there: %d\n", path, fpath, snpath->is_there);

   if(snpath->is_there != $$snpath_root) {
      $dlogi("ERROR rmdir: Bad path given\n");
      snret = -EFAULT;
   } else {
      snret = $sn_destroy(fsdata);
   }

   $$ELIF_PATH_MAIN

   $dlogdbg("* rmdir(path=\"%s\")\n", path);
   if(rmdir(fpath) == 0) { return 0; }
   return -errno;

   $$FI_PATH
}


/** Change the permission bits of a file */
int $chmod(const char *path, mode_t mode)
{
   int ret;
   $$IF_PATH_MAIN_ONLY

   $dlogdbg("   chmod(path=%s fpath=\"%s\", mode=0%03o)\n", path, fpath, mode);

   if((ret = $mfd_init_sn(path, fpath, fsdata)) != 0) {
      $dlogi("ERROR mfd_init_sn failed with '%s'\n", strerror(-ret));
      return ret;
   }

   if(chmod(fpath, mode) == 0) { return 0; }
   return -errno;
}


/** Change the owner and group of a file */
int $chown(const char *path, uid_t uid, gid_t gid)
{
   int ret;
   $$IF_PATH_MAIN_ONLY

   $dlogdbg("  chown(path=\"%s\", uid=%d, gid=%d)\n", path, uid, gid);

   if((ret = $mfd_init_sn(path, fpath, fsdata)) != 0) {
      $dlogi("ERROR mfd_init_sn failed with '%s'\n", strerror(-ret));
      return ret;
   }

   if(chown(fpath, uid, gid) == 0) { return 0; }
   return -errno;
}


/** Remove a file */
int $unlink(const char *path)
{
   int ret;

   $$IF_PATH_MAIN_ONLY

   $dlogdbg("* unlink(path=\"%s\")\n", path);

   if(unlikely((ret = $_open_truncate_close(fsdata, path, fpath, 0)) != 0)) {
      $dlogi("ERROR _open_truncate_close failed with '%s'\n", strerror(-ret));
      return ret;
   }

   // Actually do the unlink
   if(unlink(fpath) == 0) { return 0; }
   ret = errno;
   $dlogdbg("WARNING unlink(%s): unlink failed err %d = %s\n", fpath, ret, strerror(ret));
   return -ret;

   // TODO 2 Add optimisation: move file to snapshot if feasible
}


/** Change the size of a file */
int $truncate(const char *path, off_t newsize)
{
   int ret;

   $$IF_PATH_MAIN_ONLY

   $dlogdbg("* trunc(path=\"%s\" size=%zu)\n", path, newsize);

   if(unlikely((ret = $_open_truncate_close(fsdata, path, fpath, newsize)) != 0)) {
      $dlogi("ERROR _open_truncate_close failed with '%s'\n", strerror(-ret));
      return ret;
   }

   // Actually do the truncate
   if(truncate(fpath, newsize) == 0) { return 0; }

   ret = errno;
   $dlogdbg("WARNING truncate(%s): truncate failed err %d = %s\n", fpath, ret, strerror(ret));
   return -ret;

   // TODO 2 Add optimisation: copy file to snapshot if that's faster?
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
int $utimens(const char *path, const struct timespec tv[2])
{
   int ret;
   $$IF_PATH_MAIN_ONLY

#if $$SAVE_ON_UTIMENSAT > 0
   if((ret = $mfd_init_sn(path, fpath, fsdata)) != 0) {
      $dlogi("ERROR mfd_init_sn failed with '%s'\n", strerror(-ret));
      return ret;
   }
#endif

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
   */

   if(utimensat(AT_FDCWD, fpath, tv, AT_SYMLINK_NOFOLLOW) == 0) {
      return 0;
   }
   return -errno;
}


/* Unsupported operations
 ***********************************************/


/** Renaming
 *
 * This is currently not supported.
 *
 * Path map
 * ========
 *
 * A possible solution to supporting renaming efficiently, that is,
 * to avoid having to copy all data in the file being renamed,
 * is to store a path map in each snapshot that maps paths
 * in the main part of the filesystem (or later, in the next snapshot)
 * to paths in the current snapshot.
 *
 * Initially, paths of existing files are mapped to themselves (A to A),
 * but when B is renamed to C, C will be mapped to B.
 * This means that when modifying C, old data needs to be saved at B
 * in the snapshot, and when reading from B in the snapshot, we may need
 * to access unmodified data one level up, at C.
 *
 *   main part or    A      B     C     D         |     ^
 *   snapshot[i+1]   |      :   / :     :       write   |
 *                   |      : /   :     :         |    read
 *   snapshot[i]     A      B     C     D         v     |
 *
 *    MAP(A) = A
 *    MAP(B) = -
 *    MAP(C) = B
 *    MAP(D) = -
 *
 * When truncating or deleting a file, this link could also be severed
 * as no more writing is necessary, and all data should be accessible
 * in the snapshot, so reading blocks one level up will not be needed
 * (see D).
 * There should also be no link for files created after the snapshot
 * was taken.
 *
 * Renaming A to B updates the map in the following way:
 * MAP_new(B) := MAP_old(A)
 * MAP_new(A) := -
 *
 * Please note that any of the threads / filehandles could modify the
 * mapping at multiple points.
 *
 * The destination file, if it exists, needs to be saved, which
 * could be done by calling $b_truncate().
 *
 * When renaming a directory, it would need to be traversed
 * and the map updated for each file.
 *
 */
int $rename(const char *path, const char *newpath)
{
   return -ENOTSUP;
}


/** Create a symbolic link
 *
 * This is not supported yet.
 *
 * The parameters here are a little bit confusing, but do correspond
 * to the symlink() system call.  (1) is where the link points,
 * while (2) is the link itself.  So we need to leave (1)
 * unaltered, but insert the link into the mounted directory.
 */
int $symlink(const char *dest, const char *path)
{
   return -ENOTSUP;

   /* The implementation could be:

   int ret;
   $$IF_PATH_MAIN_ONLY

   $dlogdbg("  symlink(dest=\"%s\", path=\"%s\")\n", dest, path);

   if((ret = $mfd_init_sn(path, fpath, fsdata)) != 0) { return ret; }

   if(symlink(dest, fpath) == 0) { return 0; }
   return -errno;
   */
}


/** Create a hard link to a file
 *
 * This is not allowed in ESFS.
 */
int $link(const char *path, const char *newpath)
{
   return -ENOTSUP;

   // To pass the request on, call link(fpath, fnewpath);
}


/** Create a file node
 *
 * This is called for creation of all non-directory, non-symlink
 * nodes.  If the filesystem defines a create() method, then for
 * regular files that will be called instead.
 *
 * Special files are not allowed in ESFS.
 * As create() is defined, we can return here with an error.
 */
int $mknod(const char *path, mode_t mode, dev_t dev)
{
   return -ENOTSUP;

   // Original code from BBFS:
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


/** Set extended attributes
 *
 * Xattr is not supported.
 */
int $setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
   return -ENOTSUP;

   /*
   $$IF_PATH_MAIN_ONLY
   logmsg("\nsetxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n", path, name, value, size, flags);
   if(lsetxattr(fpath, name, value, size, flags) == 0){ return 0; }
   return -errno;
   */
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


/** Remove extended attributes
 *
 * Xattr is not supported.
 */
int $removexattr(const char *path, const char *name)
{
   return -ENOTSUP;

   /*
   $$IF_PATH_MAIN_ONLY
   logmsg("\nremovexattr(path=\"%s\", name=\"%s\")\n", path, name);
   if(lremovexattr(fpath, name) == 0){ return 0; }
   return -errno;
   */
}

