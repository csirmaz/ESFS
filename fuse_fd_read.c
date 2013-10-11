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
// TODO Implement snapshots
//   int (*read) (const char *, char *, size_t, off_t,
//           struct fuse_file_info *);
// BBFS: I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.
int $read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
   int ret;

   log_msg("read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x, main fd=%d)\n", path, buf, size, offset, fi, $$MFD->mainfd);
   // TODO Check if path is null, as expected

   ret = pread($$MFD->mainfd, buf, size, offset);
   if(ret >= 0) { return ret; }
   return -errno;
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
// TODO Implement snapshots:
/* Read the directory in all snapshots and the main space, collecting all files.
 * If a file is marked as nonexistent, ignore it in later snapshots.
 */
int $readdir(
   const char *path,
   void *buf,
   fuse_fill_dir_t filler,
   off_t offset,
   struct fuse_file_info *fi
)
{
   struct $mfd_t *mfd;
   struct dirent *de;
   $$DFSDATA

   $dlogdbg("readdir(path=\"%s\", offset=%lld)\n", path, (long long int)offset);

   mfd = $$MFD;

   switch(mfd->is_main) {
      case $$MFD_MAIN:

         /* readdir: If the  end  of  the  directory stream is reached, NULL is returned
            and errno is not changed.
            If an error occurs, NULL is returned and errno is set
            appropriately.
         */

         while(1) {
            errno = 0;
            de = readdir(mfd->maindir);
            if(de == NULL) {
               if(errno != 0) { return -errno; }
               break;
            }
            if(filler(buf, de->d_name, NULL, 0) != 0) {
               return -ENOMEM;
            }
         }

         return 0;

      case $$MFD_SNROOT:

         // A normal directory read, but skip the .hid file

         while(1) {
            errno = 0;
            de = readdir(mfd->maindir);
            if(de == NULL) {
               if(errno != 0) { return -errno; }
               break;
            }

            if(strcmp(de->d_name, $$EXT_HID) == 0) {
               continue;
            }

            if(filler(buf, de->d_name, NULL, 0) != 0) {
               return -ENOMEM;
            }
         }

         return 0;

      case $$MFD_SN:

         // TODO Implement reading dirs from sn_steps
         return -EBADE;

      default:

         $dlogdbg("Unknown mfd->is_main %d\n", mfd->is_main);
         return -EBADE;
   }
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
// TODO Implement snapshots
//   int (*fgetattr) (const char *, struct stat *, struct fuse_file_info *);
// Since it's currently only called after bb_create(), and bb_create()
// opens the file, I ought to be able to just use the fd and ignore
// the path...
int $fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{

   // log_msg("fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n", path, statbuf, fi);

   if(fstat($$MFD->mainfd, statbuf) == 0) { return 0; }
   return -errno;
}


