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


// Constants
// ---------
// IMPORTANT: When changing these, make sure $check_params is updated.


/** Mutex type
 * * PTHREAD_MUTEX_FAST_NP = the default, portable
 * * PTHREAD_MUTEX_ERRORCHECK_NP = returns with EDEADLK if the thread attempts to lock a mutex it already owns
 * instead of locking the thread forever
 */
#define $$MUTEXT_TYPE PTHREAD_MUTEX_FAST_NP


/** Whether to save the stat of a file in a snapshot on
 * a utimensat call: 0 (no) or 1 (yes)
 */
#define $$SAVE_ON_UTIMENSAT 0


/* About $$PATH_MAX
 * Operations need to be thread-safe (although pkg-config does
 * not return -D_REENTRANT on my system - please see
 * http://sysdocs.stu.qmul.ac.uk/sysdocs/Comment/FuseUserFileSystems/FuseBase.html )
 * Using a constant-length string to store the mapped path appears to be the
 * simplest solution under these circumstances, even though incoming paths
 * can be of any length.
 */
#define $$PATH_MAX PATH_MAX /**< we store paths in strings of this length */
#define $$PATH_LEN_T size_t // type to store path lengths in
#define $$FILESIZE_T unsigned long // type to store file size in

#define $$BL_S 131072 // blocksize in bytes. 128K = 2^17 // TODO Determine this based on stat.st_blksize
#define $$BL_SLOG 17 // log2(blocksize)
#define $$BLP_T off_t // block pointer type. Note: filesizes are stored in off_t
#define $$BLP_S (sizeof($$BLP_T)) // block pointer size in bytes

#define $$MAX_SNAPSHOTS 1024*1024 // this is currently only used to detect infinite loops // TODO 2 Review this

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

#define $$RECURSIVE_RM_FDS 3 // number of filehandles to use when traversing directories


// Locking
#define $$LOCK_NUM 64 // Number of locks; this determines the number of concurrent files that can be written
#define $$LOCKLABEL_T unsigned long // ==, & used on it. 0 is a special value

#define $$LOCKLABEL_RMDIR "*rmdir"

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
   int sn_number; /**< increases when a snapshot is made; compared to mfd->sn_number */
   char sn_lat_dir[$$PATH_MAX]; /**< caches the real path to the root of the latest snapshot */
   $$PATH_LEN_T sn_lat_dir_len; /**< the length of the latest snapshot dir string */
   int sn_is_any; /**< whether there are any snapshots, 1 or 0 */
   struct $mflock_t *mflocks; /**< file-based locks */
   // CACHE
   char *block_buffer; /**< A buffer that can be used for copy on write without having to allocate/free memory. Useful especially if ESFS handles a single file */
};


/** Values to track what kind of path is stored in snpath.
 * Note the $$ prefix to distinguish these from function names and structs.
 */
enum $$snpath_types {
   $$snpath_root, /**< /snapshots */
   $$snpath_id, /**< /snapshots/ID */
   $$snpath_full /**< /snapshots/ID/... */
};

/** A path inside the snapshots space
 */
struct $snpath_t {
   enum $$snpath_types is_there; /**< always check this before using the strings */
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


#define $$SN_STEPS_UNUSED -8
#define $$SN_STEPS_NOTOPEN -9
#define $$SN_STEPS_MAIN -7

/** Filehandle struct extension for files in snapshots.
 * Each step represents a snapshot or the main space with potential information about the resource.
 *
 * [C] = can also be < 0:
 * * $$SN_STEPS_NOTOPEN = if the file has not been opened yet
 * * $$SN_STEPS_UNUSED = if the snapshot has no information about the file
 *
 * [D] = can also be < 0:
 * * $$SN_STEPS_MAIN = if the step represents a main file (index==0)
 */
struct $sn_steps_t {
   char path[$$PATH_MAX]; /**< after $sn_get_paths_to, the real path of the snapshot ID; after $mfd_get_sn_steps, the file (or the main file) */
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


#define $$MFD_FD_NOSN   -1
#define $$MFD_FD_RDONLY -2
#define $$MFD_FD_ENOENT -3
#define $$MFD_FD_ZLEN   -4

/** Filehandle struct (mfd)
 *
 * This is the data associated with an open node (file or directory).
 *
 * [A] = can also be < 0:
 * * $$MFD_FD_NOSN - if there are no snapshots
 * * $$MFD_FD_RDONLY - if the main file is opened for read-only
 *
 * [B] = can also be < 0:
 * * $$MFD_FD_ENOENT - if the file didn't exist when the snapshot was taken
 * * $$MFD_FD_ZLEN - if the file was 0 length when the snapshot was taken
 */
struct $mfd_t {
   enum $$mfd_types is_main; /**< what this node is */
   struct $mapheader_t mapheader; /**< The whole mapheader loaded into memory for main files, and from the first map file for snapshot files */
   $$LOCKLABEL_T locklabel;
   int sn_number; /**< a number identifying the current snapshot; compared to fsdata->sn_number */

   // MAIN FILE PART: (used when dealing with a file in the main space)
   int mainfd; /**< filehandle for the main file */
   DIR *maindir; /**< dir handle for a directory in the main space, or /snapshots/ if is_main==$$MFD_SNROOT */
   int mapfd; /**< filehandle to the map file[A] in the latest snapshot. See $mfd_open_sn */
   int datfd; /**< filehandle to the dat file[A,B] in the latest snapshot. See $mfd_open_sn */
   // USED FOR RENAMES
   char write_vpath[$$PATH_MAX]; /**< the vpath the map/dat of which is actually open; or a zero-length string if we haven't followed a write directive */
   int lock; /**< used when the lock is kept; -1 means the lock is not set */
   // USED FOR REINITIALISATION
   char vpath[$$PATH_MAX]; /**< the in-FS path of the file opened; needed in case the map/dat files must be reinitalised due to a new snapshot. This is the original vpath even if we have followed a write directive */
   int flags; /**< the flags the mfd was opened with; needed in case the map/dat files must be reinitalised */
   // CACHE
   $$BLP_T latest_written_block_cache; /**< Used to cache the index+1 of the latest block written (not the position+1 in the dat file). 0 if there is nothing cached. */

   // SNAPSHOT FILE PART: (used when dealing with a file in the snapshot space)
   int sn_current; /**< the largest index in sn_steps, representing the snapshot being read */
   int sn_first_file; /**< the largest index where the node can actually be found, or -1 if not found anywhere */
   struct $sn_steps_t *sn_steps; /**< data about each snapshot step, from the main file [0], the first snapshot [1], to the snapshot being read [sn_current] */
};


/** A path for lists of paths */
struct $pathmark_t {
   char path[$$PATH_MAX];
};

#endif
