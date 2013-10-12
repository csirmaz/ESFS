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


/** Get file attributes.
*
* Similar to stat().  The 'st_dev' and 'st_blksize' fields are
* ignored.  The 'st_ino' field is ignored except if the 'use_ino'
* mount option is given.
*/
int $getattr(const char *path, struct stat *statbuf)
{
   struct $mfd_t mfd;
   $$IF_PATH_SN

   if(snpath->is_there != $$SNPATH_FULL) {

      $dlogdbg("  getattr.sn.root/id(path=\"%s\")\n", path);
      if(lstat(fpath, statbuf) == 0) {
         snret = 0;
      } else {
         snret =  -errno;
      }

   } else {

      $dlogdbg("  getattr.sn.full(path=\"%s\")\n", path);
      if(unlikely((snret = $mfd_get_sn_steps(&mfd, snpath, fsdata, $$SN_STEPS_F_OPENFILE | $$SN_STEPS_F_FIRSTONLY | $$SN_STEPS_F_SKIPOPENDAT)) != 0)) {
         $dlogdbg("get sn steps failed with %d = %s\n", -snret, strerror(-snret));
      } else {
         memcpy(statbuf, &(mfd.mapheader.fstat), sizeof(struct stat));
         $dlogdbg("getattr.sn.full successful\n");
         snret = 0;
      }

   }

   $$ELIF_PATH_MAIN

   $dlogdbg("  getattr.main(path=\"%s\")\n", path);

   if(lstat(fpath, statbuf) == 0) { return 0; }
   return -errno;

   $$FI_PATH
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
// TODO Implement snapshots
//   int (*access) (const char *, int);
int $access(const char *path, int mask)
{
   $$ALL_PATHS // TODO

   $dlogdbg("  access(path=\"%s\", mask=0%o)\n", path, mask);

   if(access(fpath, mask) == 0) { return 0; }
   return -errno;
}


/* Unsupported operations
 ***********************************************/


/** Read the target of a symbolic link
 *
 * This is not supported yet.
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.   If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
// BBFS: Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to bb_readlink()
// bb_readlink() code by Bernardo F Costa (thanks!)
int $readlink(const char *path, char *link, size_t size)
{
   return -ENOTSUP;

   /* Implementation without reading snapshots:

   int ret;
   $$IF_PATH_MAIN_ONLY

   $dlogdbg("  readlink(path=\"%s\", link=\"%s\", size=%d)\n", path, link, size);

   ret = readlink(fpath, link, size - 1);
   if(unlikely(ret == -1)) {
      return -errno;
   }
   link[ret] = '\0';
   return 0;
   */
}


/** Get extended attributes (unsupported) */
int $getxattr(const char *path, const char *name, char *value, size_t size)
{
   // Xattr is not supported by ext4; for now, we disable it.
   return -ENOTSUP;

   /*
   $$IF_PATH_MAIN_ONLY
   log_msg("\ngetxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = %d)\n", path, name, value, size);
   if(lgetxattr(fpath, name, value, size) == 0){ return 0; }
   return -errno;
   */
}


/** List extended attributes (unsupported) */
int $listxattr(const char *path, char *list, size_t size)
{
   // Xattr is not supported by ext4; for now, we disable it.
   return -ENOTSUP;

   /*
   $$IF_PATH_MAIN_ONLY
   log_msg("listxattr(path=\"%s\", list=0x%08x, size=%d)\n", path, list, size);
   if( llistxattr(fpath, list, size) == 0){ return 0; }
   return -errno;
   */
}

