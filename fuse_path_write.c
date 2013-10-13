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
   $$IF_PATH_SN

   // Create a new snapshot

   if(snpath->is_there != $$snpath_id) {
      snret = -EFAULT; // "Bad address" if wrong path was used
   } else if($sn_create(fsdata, fpath) != 0) {
      snret = -EIO; // If error occurred while trying to create snapshot; check logs.
   } else {
      snret = 0;
   }

   $$ELIF_PATH_MAIN

   $dlogdbg("  mkdir(path=\"%s\", mode=0%3o)\n", path, mode);

   // TODO mark dir as nonexistent in snapshot

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
      $dlogdbg("Bad path\n");
      snret = -EFAULT;
   } else {
      snret = $sn_destroy(fsdata);
   }

   $$ELIF_PATH_MAIN

   $dlogdbg("rmdir(path=\"%s\")\n", path);
   if(rmdir(fpath) == 0) { return 0; }
   return -errno;

   $$FI_PATH
}



/** Rename a file
 *
 * See mfd.c for a detailed description of read and write directives.
 */
// TODO Do we need locking for the rename operations?
int $rename(const char *path, const char *newpath)
{
   struct $mfd_t mymfd; // The mfd of path, with follow
   struct $mfd_t mydirectmfd; // The mfd of path, without follow
   struct $mfd_t mynewmfd; // The mfd of newpath, with follow
   int ret;
   int waserror = 0; // negative on error
   $$IF_MULTI_PATHS_MAIN_ONLY

   $dlogdbg("  rename(fpath=\"%s\", newpath=\"%s\")\n", path, newpath);

   if(unlikely((ret = $mfd_open_sn(&mymfd, path, fpath, fsdata, $$MFD_DEFAULTS)) != 0)) {
      $dlogdbg("Rename: mfd_open_sn failed on %s with %d = %s\n", path, ret, strerror(-ret));
      // We do not allow moving/renaming directories,
      // to save the complexity of saving the file change of everything under them.
      if(ret == -EISDIR) {
         return -EOPNOTSUPP;
      }
      return ret;
   }

   do {

      // If path is a file, newpath is a file as well (even if the command was to move a file under a directory).
      // Get the mfd for newpath.
      if(unlikely((ret = $mfd_open_sn(&mynewmfd, newpath, fnewpath, fsdata, $$MFD_DEFAULTS)) != 0)) {
         $dlogdbg("Rename: mfd_open_sn failed on %s with %d = %s\n", newpath, ret, strerror(-ret));
         waserror = ret;
         break;
      }

      do {

         // If newpath exists, it will be replaced, so we need to save it.
         if(mynewmfd.mapheader.exists != 0) {
            if(unlikely((ret = $b_truncate(fsdata, &mynewmfd, 0)) != 0)) {
               $dlogi("Rename: b_truncate failed on %s with %d = %s\n", newpath, ret, strerror(-ret));
               waserror = ret;
               break;
            }
         }

         // Before we modify the read and write directives, we do the actual rename.
         /* rename: If newpath already exists it will be atomically replaced (subject to a few conditions; see ERRORS below),
          * so that there  is  no  point  at  which another process attempting to access newpath will find it missing.
          */
         if(unlikely(rename(fpath, fnewpath) != 0)) {
            waserror = -errno;
            break;
         }

         // Add a write directive to the new path so that subsequently, data would be written
         // to the dat file of the original path.
         // This may replace an existing write directive, which is the correct behaviour.
         strcpy(mynewmfd.mapheader.write_v, path);
         // TODO 2 Skip saving the mapheader wile opening mfd if the map file has just been created here
         if(unlikely((ret = $mfd_save_mapheader(&mynewmfd, fsdata)) != 0)) {
            $dlogi("Rename: mfd_save_mapheader(mynewmfd) failed with %d = %s\n", ret, strerror(-ret));
            // Not sure if we can clean up this error, as why would we be able to successfully save the restored header?
            waserror = ret;
            break;
         }

         // If the old file has already been removed, we have followed a write directive
         // when opening mymfd.
         // That write directive needs to be removed, as writing oldpath no longer functions
         // like writing the old file.
         if(mymfd.is_renamed != 0) {

            if(unlikely((ret = $mfd_open_sn(&mydirectmfd, path, fpath, fsdata, $$MFD_NOFOLLOW)) != 0)) {
               $dlogi("Rename: mfd_open_sn(nofollow) failed on %s with %d = %s\n", path, ret, strerror(-ret));
               waserror = ret;
               break;
            }

            mydirectmfd.mapheader.write_v[0] = '\0';

            if(unlikely((ret = $mfd_save_mapheader(&mydirectmfd, fsdata)) != 0)) {
               $dlogi("Rename: mfd_save_mapheader(mydirectmfd) failed with %d = %s\n", ret, strerror(-ret));
               waserror = ret;
               // DO NOT break here as we want to close mydirectmfd anyway
            }

            if(unlikely((ret = $mfd_close_sn(&mydirectmfd)) != 0)) {
               $dlogi("Rename: mfd_close_sn(mydirectmfd) failed with %d = %s\n", ret, strerror(-ret));
               waserror = ret;
            }

            if(waserror != 0) {
               break;
            }

         }

         // mymfd.is_renamed = 1;
         // There's no need to set the is_renamed flag as it's in-memory only, and we'll close
         // the mfd's anyway.

         // Add a read directive to the old path so that when we step to the next snapshot,
         // we know to search the new path.
         strcpy(mymfd.mapheader.read_v, newpath);
         if(unlikely((ret = $mfd_save_mapheader(&mymfd, fsdata)) != 0)) {
            $dlogi("Rename: mfd_save_mapheader(mymfd) failed with %d = %s\n", ret, strerror(-ret));
            // We could restore mynewmfd.mapheader & mydirectmfd.mapheader here.
            waserror = ret;
            break;
         }

      } while(0);

      if((ret = $mfd_close_sn(&mynewmfd)) != 0) {
         $dlogi("Rename: mfd_close_sn(mynewmfd) failed with %d = %s\n", ret, strerror(-ret));
         waserror = ret;
         break;
      }

   } while(0);

   if((ret = $mfd_close_sn(&mymfd)) != 0) {
      $dlogi("Rename: mfd_close_sn(mymfd) failed with %d = %s\n", ret, strerror(-ret));
      waserror = ret;
   }

   return waserror;
}


/** Change the permission bits of a file */
int $chmod(const char *path, mode_t mode)
{
   int ret;
   $$IF_PATH_MAIN_ONLY

   $dlogdbg("   chmod(path=%s fpath=\"%s\", mode=0%03o)\n", path, fpath, mode);

   if((ret = $mfd_init_sn(path, fpath, fsdata)) != 0) { return ret; }

   if(chmod(fpath, mode) == 0) { return 0; }
   return -errno;
}


/** Change the owner and group of a file */
int $chown(const char *path, uid_t uid, gid_t gid)
{
   int ret;
   $$IF_PATH_MAIN_ONLY

   $dlogdbg("  chown(path=\"%s\", uid=%d, gid=%d)\n", path, uid, gid);

   if((ret = $mfd_init_sn(path, fpath, fsdata)) != 0) { return ret; }

   if(chown(fpath, uid, gid) == 0) { return 0; }
   return -errno;
}


/** Remove a file */
int $unlink(const char *path)
{
   int ret;

   $$IF_PATH_MAIN_ONLY

   // $dlogdbg("  unlink(path=\"%s\")\n", path);

   if(unlikely((ret = $_open_truncate_close(fsdata, path, fpath, 0)) != 0)) {
      return ret;
   }

   // Actually do the unlink
   if(unlink(fpath) == 0) { return 0; }

   ret = errno;
   $dlogdbg("unlink(%s): unlink failed err %d = %s\n", fpath, ret, strerror(ret));
   return -ret;

   // TODO 2 Add optimisation: move file to snapshot if feasible
}


/** Change the size of a file */
int $truncate(const char *path, off_t newsize)
{
   int ret;

   $$IF_PATH_MAIN_ONLY

   $dlogdbg("  trunc(path=\"%s\" size=%zu)\n", path, newsize);

   if(unlikely((ret = $_open_truncate_close(fsdata, path, fpath, newsize)) != 0)) {
      return ret;
   }

   // Actually do the truncate
   if(truncate(fpath, newsize) == 0) { return 0; }

   ret = errno;
   $dlogdbg("truncate(%s): truncate failed err %d = %s\n", fpath, ret, strerror(ret));
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

   // TODO 2: for performance reasons, maybe we don't want to trigger a save here
   if((ret = $mfd_init_sn(path, fpath, fsdata)) != 0) { return ret; }

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
 * Xattr is not supported by ext4; for now, we disable it.
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
 * Xattr is not supported by ext4; for now, we disable it.
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

