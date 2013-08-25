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

/*
  There are a couple of symbols that need to be #defined before
  #including all the headers.
*/

#ifndef _PARAMS_H_
#define _PARAMS_H_

#include <limits.h> // for FILE
#include <stdio.h> // for FILE

// The FUSE API has been changed a number of times.  So, our code
// needs to define the version of the API that we assume.  As of this
// writing, the most current API version is 26
#define FUSE_USE_VERSION 26 // TODO

// For libraries
// -------------

// need this to get pwrite().  I have to use setvbuf() instead of
// setlinebuf() later in consequence.
#define _XOPEN_SOURCE 500

// Needed to get utimensat and AT_FDCWD
// Backtrace: fcntl.h, sys/stat.h <- features.h 
#define _ATFILE_SOURCE 1

// Parameters
// ----------
#define $log(...) log_msg(__VA_ARGS__)
#define $dlog(...) fprintf(fsdata->logfile, __VA_ARGS__)
#define $dlogi(...) fprintf(fsdata->logfile, __VA_ARGS__) // important log lines

// Constants
// ---------

/* About $$PATH_MAX
 * http://sysdocs.stu.qmul.ac.uk/sysdocs/Comment/FuseUserFileSystems/FuseBase.html
 * suggests that operations need to be thread-safe, although pkg-config does
 * not return -D_REENTRANT on my system. // TODO
 * Using a constant-length string to store the mapped path appears to be the
 * simplest solution under these circumstances, even though incoming paths
 * can be of any length.
 */
#define $$PATH_MAX PATH_MAX
#define $$PATH_LEN_T int

// The snapshots directory
//                  0123456789
#define $$SNDIR    "/snapshots"
#define $$SNDIR_LEN 10 // Without null character

#define $$EXT_DATA ".dat"
#define $$EXT_HID ".hid"
#define $$EXT_LEN 4 // Without null character

#define $$DIRSEP "/"
#define $$DIRSEPCH '/'

// Global FS private data
// ----------------------

struct $fsdata_t {
    FILE *logfile;
    char *rootdir; // the underlying root acting as the source of the FS
    size_t rootdir_len; // length of rootdir string, without terminating NULL
    char sn_dir[$$PATH_MAX]; // the real path to the snapshot directory
    char sn_lat_dir[$$PATH_MAX]; // the real path to the root of the latest snapshot
    int sn_is_any; // whether there are any snapshots
};

// Retrieve and cast
#define $$FSDATA ((struct $fsdata_t *) fuse_get_context()->private_data )

// A path inside the snapshots space
// ---------------------------------

struct $snpath_t {
   int is_there; // 0=nothing, 1=ID, 2=ID&inpath -- always check this before using the strings
   char id[$$PATH_MAX]; // e.g. "/ID"
   char inpath[$$PATH_MAX]; // e.g. "/dir/dir/file"
};

// In case it matters
// ------------------
#define likely(x) (__builtin_expect((x), 1))
#define unlikely(x) (__builtin_expect((x), 0))

#endif
