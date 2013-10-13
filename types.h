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

/* Formats on my system
 * size_t %zu
 * off_t %td
 */

// Logging
// -------
#define $dlogdbg(...) fprintf(fsdata->logfile, __VA_ARGS__);fflush(fsdata->logfile) // debug messages
#define $dlogi(...) fprintf(fsdata->logfile, __VA_ARGS__);fflush(fsdata->logfile) // important log lines


// Constants
// ---------
// IMPORTANT: When changing these, update $check_params.

/* About $$PATH_MAX
 * http://sysdocs.stu.qmul.ac.uk/sysdocs/Comment/FuseUserFileSystems/FuseBase.html
 * suggests that operations need to be thread-safe, although pkg-config does
 * not return -D_REENTRANT on my system. // TODO
 * Using a constant-length string to store the mapped path appears to be the
 * simplest solution under these circumstances, even though incoming paths
 * can be of any length.
 */
#define $$PATH_MAX PATH_MAX // we regularly use strings of this length
#define $$PATH_LEN_T size_t // type to store path lengths in
#define $$FILESIZE_T unsigned long // type to store file size in

#define $$BL_S 131072 // blocksize in bytes. 128K = 2^17 // TODO Determine this based on stat.st_blksize
#define $$BL_SLOG 17 // log2(blocksize)
#define $$BLP_T size_t // block pointer type. Note: filesizes are stored in off_t
#define $$BLP_S (sizeof($$BLP_T)) // block pointer size in bytes

#define $$MAX_SNAPSHOTS 1024*1024 // this is currently only used to detect infinite loops // TODO Review this

// The snapshots directory
//                  0123456789
#define $$SNDIR    "/snapshots"
#define $$SNDIR_LEN 10 // Without null character

// Extensions for snapshot files
#define $$EXT_DAT ".dat"
#define $$EXT_MAP ".map"
#define $$EXT_HID ".hid"
#define $$EXT_LEN 4 // Without null character

#define $$DIRSEP "/"
#define $$DIRSEPCH '/'

// Locking
#define $$LOCK_NUM 64 // Number of locks; this determines the number of concurrent files that can be written
#define $$LOCKLABEL_T ino_t // ==, & used on it. 0 is a special value

#define $$RECURSIVE_RM_FDS 3 // number of filehandles to use when traversing directories

// MFD open flags
#define $$MFD_DEFAULTS 0
#define $$MFD_NOFOLLOW 1


/** An element in the table used for file-based locking
 */
struct $mflock_t {
   $$LOCKLABEL_T label;
   pthread_mutex_t mutex;
   pthread_mutex_t mod_mutex;
   unsigned int want;
};


/** Global filesystem private data
 */
struct $fsdata_t {
   FILE *logfile;
   char *rootdir; /**< the underlying root acting as the source of the FS */
   $$PATH_LEN_T rootdir_len; /**< the length of rootdir string, without the terminating NULL */
   char sn_dir[$$PATH_MAX]; /**< the real path to the snapshot directory */
   char sn_lat_dir[$$PATH_MAX]; /**< the real path to the root of the latest snapshot */
   $$PATH_LEN_T sn_lat_dir_len; /**< the length of the latest snapshot dir string */
   int sn_is_any; /**< whether there are any snapshots, 1 or 0 */
   struct $mflock_t *mflocks; /**< file-based locks */
};


#define $$SNPATH_ROOT 0
#define $$SNPATH_IDONLY 1
#define $$SNPATH_FULL 2

/** A path inside the snapshots space
 */
struct $snpath_t {
   int is_there; /**< $$SNPATH_ROOT=nothing, $$SNPATH_IDONLY=ID, $$SNPATH_FULL=ID&inpath -- always check this before using the strings */
   char id[$$PATH_MAX]; /**< e.g. "/ID" */
   char inpath[$$PATH_MAX]; /**< e.g. "/dir/dir/file" */
};


/** Map file header
 *
 * This is the data saved in the .map file
 */
struct $mapheader_t {
   int $version;
   int exists; /**< whether the file exists, 0 or 1 */
   struct stat fstat; /**< saved parameters of the file (only if exists==1) */
   char read_v[$$PATH_MAX]; // the read directive: going forward, read the dest instead. Contains a virtual path or an empty string.
   char write_v[$$PATH_MAX]; // the write directive: in this snapshot, write the dest instead. Contains a virtual path or an empty string.
   char signature[4];
};
// TODO 2 To make FS files portable, the types used here should be reviewed, and proper (de)serialisation implemented.


/** See $mfd_get_sn_steps */
#define $$SN_STEPS_F_FILE 1
#define $$SN_STEPS_F_DIR 2
#define $$SN_STEPS_F_TYPE_UNKNOWN 4
#define $$SN_STEPS_F_FIRSTONLY 8
#define $$SN_STEPS_F_SKIPOPENDAT 16
#define $$SN_STEPS_F_SKIPOPENDIR 32
#define $$SN_STEPS_F_STATDIR 64

#define $$SN_STEPS_UNUSED -8
#define $$SN_STEPS_NOTOPEN -9
#define $$SN_STEPS_MAIN -7

/** Filehandle struct extension for files in snapshots
 *
 * When dealing with a file in a snapshot:
 * * mfd->is_main is $$MFD_SN
 *
 * [C] = can also be:
 * * $$SN_STEPS_NOTOPEN = if the file has not been opened yet
 * * $$SN_STEPS_UNUSED = if the snapshot has no information about the file
 *
 * [D] = can also be:
 * * $$SN_STEPS_MAIN = if the step represents a main file (index==0)
 */
struct $sn_steps_t {
   char path[$$PATH_MAX]; /**< first the real path of the snapshot ID, then the file (or the main file) */
   int mapfd; /**< filehandle to the map file[C,D] */
   int datfd; /**< filehandle to the dat file[C] or the main file */
   DIR *dirfd; /**< handle to the open directory, or NULL */
};


/** Values to track what kind of node is stored in the mfd.
 * Note the $$ prefix to distinguish these from function names and structs.
 */
enum $$mfd_types {
   $$mfd_main,
   $$mfd_sn_root, /**< when it is /snapshots */
   $$mfd_sn_id, /**< when it is /snapshots/ID */
   $$mfd_sn_full
};

#define $$MFD_MAINLIKE(is_main) ((is_main == $$mfd_main) || (is_main == $$mfd_sn_root))

/** Filehandle struct (mfd)
 *
 * This is the data associated with an open file.
 *
 * [A] = can also be:
 * * -1 - if there are no snapshots
 * * -2 - if the main file is opened for read-only
 *
 * [B] = can also be:
 * * -3 - if the file didn't exist when the snapshot was taken
 * * -4 - if the file was 0 length when the snapshot was taken
 */
struct $mfd_t {
   enum $$mfd_types is_main;
   struct $mapheader_t mapheader; /**< The whole mapheader loaded into memory for main files, and from the first map file for snapshot files */

   // MAIN FILE PART: (used when dealing with a file in the main space)
   int mainfd; /**< filehandle for the main file */
   DIR *maindir; /**< dir handle for a directory in the main space, or /snapshots/ if is_main==$$MFD_SNROOT */
   ino_t main_inode; /**< the inode number of the main file (which is possibly new, so not in mapheader.fstat), used for locking */
   int mapfd; /**< filehandle to the map file[A] in the latest snapshot. See $mfd_open_sn */
   int datfd; /**< filehandle to the dat file[A,B] in the latest snapshot. See $mfd_open_sn */
   int is_renamed; /**< bool; 0 or 1 if we have followed a write directive. See $mfd_open_sn */

   // SNAPSHOT FILE PART: (used when dealing with a file in the snapshot space)
   int sn_current; /**< the largest index in sn_steps, representing the snapshot being read */
   int sn_first_file; /**< the largest index where the node can actually be found, or -1 if not found anywhere */
   struct $sn_steps_t *sn_steps; /**< data about each snapshot step, from the main file [0], the first snapshot [1], to the snapshot being read [sn_current] */
};


/** A path and a marker */
struct $pathmark_t {
   char path[$$PATH_MAX];
};

#endif
