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

// This file contains functions related to saving things in a snapshot
// and retrieving things from snapshots.

// Create the snapshot dir if necessary.
// Returns
// 0 - on success
// -errno - if lstat or mkdir fails
// 1 - if the node exists but is not a directory
int $sn_check_dir(struct $fsdata_t *fsdata) // $dlogi needs this to locate the log file
{
   struct stat mystat;
   int ret;

   $dlogi("Init: checking the snapshots dir\n");
   
   if(lstat(fsdata->snapshotdir, &mystat) == 0){
      // Check if it is a directory
      if(S_ISDIR(mystat.st_mode)){ return 0; }
      $dlogi("Init error: found something else than a directory at %s\n", fsdata->snapshotdir);
      return 1;
   } else {
      ret = errno;
      if(ret == ENOENT){
         $dlogi("Init: creating the snapshots dir\n");
         if(mkdir(fsdata->snapshotdir, 0700) == 0){ return 0; }
         ret = errno;
         $dlog("Init error: mkdir on %s failed with %s\n", fsdata->snapshotdir, strerror(ret));
         return -ret;
      }
      $dlogi("Init error: lstat on %s failed with %s\n", fsdata->snapshotdir, strerror(ret));
      return -ret;
   }

   return 0;
}

// Check if xattrs are suppored by the underlying filesystem
// They are not supported by ext4
// Returns
// 0 on success
// -errno on any failure
// 1 on unexpected value
int $sn_check_xattr(struct $fsdata_t *fsdata) // $dlogi needs this to locate the log file
{
   int ret;
   char value[2];

   $dlogi("Init: checking xattr support\n");

   ret = lremovexattr(fsdata->snapshotdir, $$XA_TEST);
   if(ret != 0){
      ret = errno;
      if(ret != ENODATA){
         $dlogi("Init: removing xattr on %s failed with %d = %s\n", fsdata->snapshotdir, ret, strerror(ret));
         return -ret;
      }
   }
   ret = lsetxattr(fsdata->snapshotdir, $$XA_TEST, "A", 2, 0);
   if(ret != 0){
      ret = errno;
      $dlogi("Init: setting xattr on %s failed with %s\n", fsdata->snapshotdir, strerror(ret));
      return -ret;
   }
   ret = lgetxattr(fsdata->snapshotdir, $$XA_TEST, value, 2);
   if(ret != 0){
      $dlogi("Init: getting xattr on %s failed with %s\n", fsdata->snapshotdir, strerror(ret));
      return -ret;
   }
   if(strcmp(value, "A") != 0){
      $dlogi("Init: got unexpected value from xattr on %s\n", fsdata->snapshotdir);
      return 1;
   }
   ret = lremovexattr(fsdata->snapshotdir, $$XA_TEST);
   if(ret != 0){
      ret = errno;
      $dlogi("Init: removing xattr on %s failed with %s\n", fsdata->snapshotdir, strerror(ret));
      return -ret;
   }
   return 0;
}

// TODO Get the path prefix of the latest snapshot, and possibly a list of snapshots

/* Save information about a file that usually goes into the directory
 * entry, like flags, permissions, and, most importantly, size.
 */
// $push_dentry

// $push_whole_file

// $pull_directory

// $push_blocks

// $pull_blocks
