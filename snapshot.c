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
// -ENAMETOOLONG - if fpath would be too long
// -errno - if lstat or mkdir fails
// 1 - if the node exists but is not a directory
int $sn_init(struct $fsdata_t *fsdata)
{
   struct stat mystat;
   char fpath[$$PATH_MAX];
   int ret;

   $dlogi("Init: checking the snapshots dir\n");

   if($cmpath(fpath, $$SNDIR, fsdata) == -ENAMETOOLONG){
      $dlogi("Init error: path too long\n");
      return -ENAMETOOLONG;
   }
   
   if(lstat(fpath, &mystat) == 0){
      // Check if it is a directory
      if(S_ISDIR(mystat.st_mode)){ return 0; }
      $dlogi("Init error: found something else than a directory at %s\n", fpath);
      return 1;
   } else {
      ret = errno;
      if(ret == ENOENT){
         $dlogi("Init: creating the snapshots dir\n");
         if(mkdir(fpath, 0700) == 0){ return 0; }
         ret = errno;
         $dlog("Init error: mkdir on %s failed with %s\n", fpath, strerror(ret));
         return -ret;
      }
      $dlogi("Init error: lstat on %s failed with %s\n", fpath, strerror(ret));
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
