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

/* USEFUL
 * strerror(errno)
 */


// Use this when the command writes - we don't allow that in the snapshot dir, only in the main space.
#define $$IF_PATH_MAIN_ONLY \
   char fpath[$$PATH_MAX]; \
   struct $fsdata_t *fsdata; \
   fsdata = ((struct $fsdata_t *) fuse_get_context()->private_data ); \
   switch($cmpath(fpath, path, fsdata)){ \
      case -ENAMETOOLONG : return -ENAMETOOLONG; \
      case -EACCES : return -EACCES; \
   }

// Use this when there are two paths, path and newpath, and the command writes.
#define $$IF_MULTI_PATHS_MAIN_ONLY \
   char fpath[$$PATH_MAX]; \
   char fnewpath[$$PATH_MAX]; \
   struct $fsdata_t *fsdata; \
   fsdata = ((struct $fsdata_t *) fuse_get_context()->private_data ); \
   switch($cmpath(fpath, path, fsdata)){ \
      case -ENAMETOOLONG : return -ENAMETOOLONG; \
      case -EACCES : return -EACCES; \
   } \
   switch($cmpath(fnewpath, newpath, fsdata)){ \
      case -ENAMETOOLONG : return -ENAMETOOLONG; \
      case -EACCES : return -EACCES; \
   }


// Checks and map path (path into fpath)
// Puts the mapped path in fpath and returns
// 0 - if the path is in the main space
// -EACCES - if the path is in the snapshot space
// -ENAMETOOLONG - if the mapped path is too long
static int $cmpath(char *fpath, const char *path, struct $fsdata_t *fsdata)
{
   
   // Add prefix of root folder to map to underlying file
   strcpy(fpath, fsdata->rootdir);
   strncat(fpath, path, $$PATH_MAX - fsdata->rootdir_len);
   if(likely(strlen(fpath) < $$PATH_MAX)){ // success - fpath is not too long
      // If path starts with the snapshot folder
      if(unlikely(strncmp(path, $$SNDIR, $$SNDIR_LEN) == 0 && (path[$$SNDIR_LEN] == '/' || path[$$SNDIR_LEN] == '\0'))){
         return -EACCES;
      }
      return 0;
   }
   return -ENAMETOOLONG;

}


// Adds data suffix to a path
// Returns:
// 0 - success
// -ENAMETOOLONG - if the new path is too long
static int $get_data_path(char *newpath, const char *oldpath)
{
   if(unlikely(strlen(oldpath) >= $$PATH_MAX - $$EXT_LEN)){
      return -ENAMETOOLONG;
   }
   strcpy(newpath, oldpath);
   strncat(newpath, $$EXT_DATA, $$EXT_LEN);
   return 0;
}


// Adds "hidden" suffix to a path
// Returns:
// 0 - success
// -ENAMETOOLONG - if the new path is too long
static int $get_hid_path(char *newpath, const char *oldpath)
{
   if(unlikely(strlen(oldpath) >= $$PATH_MAX - $$EXT_LEN)){
      return -ENAMETOOLONG;
   }
   strcpy(newpath, oldpath);
   strncat(newpath, $$EXT_HID, $$EXT_LEN);
   return 0;
}
