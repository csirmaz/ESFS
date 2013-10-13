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

/* This file contains FS operations that are not implemented
 */



/** A logging function from BBFS, for reference.
 */

//  macro to log fields in structs.
#define log_struct(st, field, format, typecast) \
  log_msg("    " #field " = " #format "\n", typecast st->field)

// struct fuse_file_info keeps information about files (surprise!).
// This dumps all the information in a struct fuse_file_info.  The struct
// definition, and comments, come from /usr/include/fuse/fuse_common.h
// Duplicated here for convenience.
void log_fi (struct fuse_file_info *fi)
{
    /** Open flags.  Available in open() and release() */
    //   int flags;
   log_struct(fi, flags, 0x%08x, );

    /** Old file handle, don't use */
    //   unsigned long fh_old;
   log_struct(fi, fh_old, 0x%08lx,  );

    /** In case of a write operation indicates if this was caused by a
        writepage */
    //   int writepage;
   log_struct(fi, writepage, %d, );

    /** Can be filled in by open, to use direct I/O on this file.
        Introduced in version 2.4 */
    //   unsigned int keep_cache : 1;
   log_struct(fi, direct_io, %d, );

    /** Can be filled in by open, to indicate, that cached file data
        need not be invalidated.  Introduced in version 2.4 */
    //   unsigned int flush : 1;
   log_struct(fi, keep_cache, %d, ); // TODO Check if this would be useful

    /** Padding.  Do not use*/
    //   unsigned int padding : 29;

    /** File handle.  May be filled in by filesystem in open().
        Available in all other file operations */
    //   uint64_t fh;
   log_struct(fi, fh, 0x%016llx,  );

    /** Lock owner id.  Available in locking operations and flush */
    //  uint64_t lock_owner;
   log_struct(fi, lock_owner, 0x%016llx, );
};





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
