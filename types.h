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

#ifndef $$TYPES_H_
#define $$TYPES_H_

// Logging
// -------
#define $log(...) log_msg(__VA_ARGS__)
#define $dlogdbg(...) fprintf(fsdata->logfile, __VA_ARGS__) // debug messages
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
#define $$PATH_LEN_T size_t // type to store path lengths in
#define $$FILESIZE_T unsigned long // type to store file size in

// The snapshots directory
//                  0123456789
#define $$SNDIR    "/snapshots"
#define $$SNDIR_LEN 10 // Without null character

#define $$EXT_DAT ".dat"
#define $$EXT_MAP ".map"
#define $$EXT_HID ".hid"
#define $$EXT_LEN 4 // Without null character

#define $$DIRSEP "/"
#define $$DIRSEPCH '/'

// Check consistency of constants
// Returns 0 on success, -1 on failure
int $check_params(void){
   if(strlen($$SNDIR) != $$SNDIR_LEN){ return -1; }
   if(strlen($$EXT_DAT) != $$EXT_LEN){ return -1; }
   if(strlen($$EXT_HID) != $$EXT_LEN){ return -1; }
   if(strlen($$DIRSEP) != 1){ return -1; }
   return 0;
}


// Global FS private data
// ----------------------

struct $fsdata_t {
    FILE *logfile;
    char *rootdir; // the underlying root acting as the source of the FS
    $$PATH_LEN_T rootdir_len; // length of rootdir string, without terminating NULL
    char sn_dir[$$PATH_MAX]; // the real path to the snapshot directory
    char sn_lat_dir[$$PATH_MAX]; // the real path to the root of the latest snapshot
    $$PATH_LEN_T sn_lat_dir_len; // length of the latest snapshot dir string
    int sn_is_any; // whether there are any snapshots, 1 or 0
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


// MAP file header
// ---------------

// TODO To make FS files portable, the types used here should be reviewed. and proper (de)serialisation implemented.
struct $mapheader_t {
   int exists; // whether the file exists
   struct stat fstat; // saved parameters of the file
   char read_v[$$PATH_MAX]; // the read directive: going forward, read the dest instead. Contains a virtual path or an empty string.
   char write_v[$$PATH_MAX]; // the write directive: in this snapshot, write the dest instead. Contains a virtual path or an empty string.
   char padding[200];
};


// Filehandle struct
// -----------------

struct $fd_t {
   int is_main; // 1 if this is a main file; 0 otherwise
   // MAIN FILE FD:
   int mainfd; // filehandle to the main file
   int mapfd; // filehandle to the map file; -1 if there are no snapshots; -2 if the main file is opened for read only
   int datfd; // filehandle to the dat file; -1 if there are no snapshots; -2 if the main file is opened for read only
};

// CAST
#define $$MFD ((struct $fd_t *) fi->fh)

#endif
