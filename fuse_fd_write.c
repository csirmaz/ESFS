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


/** Write data to an open file
 *
 * FUSE: Write should return exactly the number of bytes requested
 * except on error.   An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 *
 * BBFS: As  with read(), the documentation above is inconsistent
 * with the documentation for the write() system call.
 */
int $write(
   const char *path,
   const char *buf,
   size_t size,
   off_t offset,
   struct fuse_file_info *fi
)
{
   struct $mfd_t *mfd;
   int ret;
   $$DFSDATA

   mfd = $$MFD;

   // Only allow writes on main FDs
   if(mfd->is_main != 1) { return -EACCES; }

   $dlogdbg("  write(path=\"%s\", size=%d, offset=%lld, main df=%d)\n", path, (int)size, (long long int)offset, mfd->mainfd);

   // Save blocks into snapshot
   if(unlikely((ret = $b_write(fsdata, mfd, size, offset)) != 0)) { return ret; }

   ret = pwrite(mfd->mainfd, buf, size, offset);
   if(ret >= 0) { return ret; }
   return -errno;
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
int $ftruncate(const char *path, off_t newsize, struct fuse_file_info *fi)
{
   struct $mfd_t *mfd;
   int ret;
   $$DFSDATA

   mfd = $$MFD;

   $dlogdbg("  ftruncate(path=\"%s\", newsize=%zu, FD = %d)\n", path, newsize, mfd->mainfd);

   if(unlikely((ret = $b_truncate(fsdata, mfd, newsize)) != 0)) { return ret; }

   if(ftruncate(mfd->mainfd, newsize) == 0) { return 0; }
   return -errno;
}


