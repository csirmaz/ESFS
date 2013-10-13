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
// TODO Implement snapshots
//   int (*flush) (const char *, struct fuse_file_info *);
int $flush(const char *path, struct fuse_file_info *fi)
{
   $$DFSDATA_MFD

   $dlogdbg("flush(path=\"%s\")\n", path);

   // TODO THIS IS IGNORED

   return 0;
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
int $release(const char *path, struct fuse_file_info *fi)
{
   int ret = 0;
   $$DFSDATA_MFD

   mfd = $$MFD;

   $dlogdbg("release(path=\"%s\")\n", path);

   if(mfd->is_main == $$mfd_main) {

      ret = $mfd_close_sn(mfd);
      if(unlikely(close(mfd->mainfd) != 0)) { ret = errno; }

   } else if(mfd->is_main == $$mfd_sn_full) {

      ret = $mfd_destroy_sn_steps(mfd, fsdata);

   } else {

      $dlogi("Wrong is_main!\n");
      ret = -EBADE;

   }

   free(mfd);
   return ret;
}


/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
// TODO implement
//   int (*fsync) (const char *, int, struct fuse_file_info *);
int $fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
   $$DFSDATA_MFD

   $dlogdbg("fsync(path=\"%s\", datasync=%d)\n", path, datasync);

   if(datasync) {
      if(fdatasync($$MFD->mainfd) == 0) { return 0; } // TODO sync other files
      return -errno;
      // fdatasync()  is  similar  to  fsync(),  but  does  not flush modified metadata unless that metadata is needed
   }

   if(fsync($$MFD->mainfd) == 0) { return 0; } // TODO sync other files
   return -errno;
   // fsync() transfers ("flushes") all modified in-core data of (i.e., modified buffer cache pages for)
   // the file referred to by the file descriptor fd to the disk device

}


/** Release directory
 *
 * Introduced in version 2.3
 */
int $releasedir(const char *path, struct fuse_file_info *fi)
{
   struct $mfd_t *mfd;
   int waserror = 0;
   $$DFSDATA
   mfd = $$MFD;

   if($$MFD_MAINLIKE(mfd->is_main)) {

      $dlogdbg("releasedir.main(path=\"%s\")\n", path);
      if(unlikely(closedir(mfd->maindir) != 0)) { waserror = -errno; }
      free(mfd);
      $dlogdbg("  releasedir.main ends with %d\n", waserror);
      return waserror;

   }

   $dlogdbg("releasedir.sn(path=\"%s\")\n", path);
   waserror = $mfd_destroy_sn_steps(mfd, fsdata);
   free(mfd);
   $dlogdbg("  releasedir.sn ends with %d\n", waserror);
   return waserror;
}


/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
// TODO Implement snapshots
//   int (*fsyncdir) (const char *, int, struct fuse_file_info *);
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ???
int $fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
   $$DFSDATA_MFD

   $dlogdbg("fsyncdir(path=\"%s\", datasync=%d)\n", path, datasync);

   // TODO This is ignored

   return 0;
}
