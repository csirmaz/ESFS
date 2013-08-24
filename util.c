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

// This file contains low-level tools.

/* About $$PATH_MAX
 * http://sysdocs.stu.qmul.ac.uk/sysdocs/Comment/FuseUserFileSystems/FuseBase.html
 * suggests that operations need to be thread-safe, although pkg-config does
 * not return -D_REENTRANT on my system. // TODO
 * Using a constant-length string to store the mapped path appears to be the
 * simplest solution under these circumstances, even though incoming paths
 * can be of any length.
 */

// Define, Check, Map Path in variable path
// Use this when the command writes - we don't allow that in the snapshot dir, only in the main space.
#define $$IF_PATH_MAIN_ONLY \
   char fpath[$$PATH_MAX]; \
   switch($cmpath(fpath, path)){ \
      case -EINVAL : return -EINVAL; \
      case -EACCES : return -EACCES; \
   }

// Use this when there are two paths, path and newpath, and the command writes.
#define $$IF_MULTI_PATHS_MAIN_ONLY \
   char fpath[$$PATH_MAX]; \
   char fnewpath[$$PATH_MAX]; \
   switch($cmpath(fpath, path)){ \
      case -EINVAL : return -EINVAL; \
      case -EACCES : return -EACCES; \
   } \
   switch($cmpath(fnewpath, newpath)){ \
      case -EINVAL : return -EINVAL; \
      case -EACCES : return -EACCES; \
   }

// Check and map path (path into fpath)
// Returns
// 0 - if the path is in the main space, and is mapped
// -EACCES - if the path is in the snapshot space
// -EINVAL - if the mapped path is too long
static int $cmpath(char *fpath, const char *path)
{
   $$DFSDATA
   
   // If path starts with the snapshot folder
   if(unlikely(strncmp(path, $$SNDIR, $$SNDIR_LEN) == 0 && (path[$$SNDIR_LEN] == '/' || path[$$SNDIR_LEN] == '\0'))){
      return -EACCES;
   }else{
      // Add prefix of root folder to map to underlying file
      strcpy(fpath, $fsdata->rootdir);
      strncat(fpath, path, $$PATH_MAX - $fsdata->rootdir_len);
      if(likely(strlen(fpath) < $$PATH_MAX)){ return 0; } // success
      return -EINVAL;
   }
}
   
