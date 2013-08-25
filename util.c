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
// Uses path; Defines fpath, fsdata
#define $$IF_PATH_MAIN_ONLY \
   char fpath[$$PATH_MAX]; \
   struct $fsdata_t *fsdata; \
   fsdata = ((struct $fsdata_t *) fuse_get_context()->private_data ); \
   switch($cmpath(fpath, path, fsdata)){ \
      case -ENAMETOOLONG : return -ENAMETOOLONG; \
      case -EACCES : return -EACCES; \
   }


// Use this when there are two paths, path and newpath, and the command writes.
// Uses path; Defines fpath, fnewpath, fsdata
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


// Use these when there are different things to do in the two spaces
// Uses path; Defines fpath, fsdata, snpath, ret
// ! For performance, we only allocate memory for snpath when needed, but because of this,
// one cannot return in the SN branch.
#define $$IF_PATH_MAIN \
   char fpath[$$PATH_MAX]; \
   struct $fsdata_t *fsdata; \
   struct $snpath_t *snpath; \
   int ret = -EBADE; \
   fsdata = ((struct $fsdata_t *) fuse_get_context()->private_data ); \
   switch($cmpath(fpath, path, fsdata)){ \
      case 0 :

// Uses snpath, path
// DO NOT RETURN FROM THE FOLLOWING BLOCK! SET 'ret' INSTEAD!
#define $$ELIF_PATH_SN \
      case -EACCES : \
         snpath = malloc(sizeof(struct $snpath_t)); \
         if(snpath == NULL){ return -ENOMEM; } \
         $decompose_sn_path(snpath, path);

// Uses ret
#define $$FI_PATH \
         free(snpath); \
         return ret; \
      case -ENAMETOOLONG : return -ENAMETOOLONG; \
   } \
   return -EFAULT;



// Use this to allow the operation everywhere.
// TODO Later, replace this with something that hides .hid files
// and deals with .dat extensions on non-directories.
// Uses path; Defines fpath, fsdata
#define $$ALL_PATHS \
   char fpath[$$PATH_MAX]; \
   struct $fsdata_t *fsdata; \
   fsdata = ((struct $fsdata_t *) fuse_get_context()->private_data ); \
   switch($map_path(fpath, path, fsdata)){ \
      case -ENAMETOOLONG : return -ENAMETOOLONG; \
   }


// Adds a suffix to a path
// Returns:
// 0 - success
// -ENAMETOOLONG - if the new path is too long
#define $$ADDNSUFFIX(newpath, olpdath, fix, fixlen) \
   if(likely(strlen(oldpath) < $$PATH_MAX - fixlen)){ \
      strcpy(newpath, oldpath); \
      strncat(newpath, fix, fixlen); \
      return 0; \
   } \
   return -ENAMETOOLONG;


// Adds a prefix to a path
// Returns:
// 0 - success
// -ENAMETOOLONG - if the new path is too long
#define $$ADDNPREFIX(newpath, oldpath, fix, fixlen) \
   $$PATH_LEN_T len; \
   len = $$PATH_MAX - fixlen; \
   if(likely(strlen(oldpath) < len)){ \
      strcpy(newpath, fix); \
      strncat(newpath, oldpath, len); \
      return 0; \
   } \
   return -ENAMETOOLONG;


// Adds a prefix and a suffix to a path
// Returns:
// 0 - success
// -ENAMETOOLONG - if the new path is too long
#define $$ADDNPSFIX(newpath, oldpath, prefix, prefixlen, suffix, suffixlen) \
   $$PATH_LEN_T len; \
   len = $$PATH_MAX - prefixlen - suffixlen; \
   if(likely(strlen(oldpath) < len)){ \
      strcpy(newpath, prefix); \
      strncat(newpath, oldpath, len); \
      strncat(newpath, suffix, suffixlen); \
      return 0; \
   } \
   return -ENAMETOOLONG;


// Adds data suffix to a path
// Returns:
// 0 - success
// -ENAMETOOLONG - if the new path is too long
static int $get_data_path(char *newpath, const char *oldpath)
{
   $$ADDNSUFFIX(newpath, oldpath, $$EXT_DATA, $$EXT_LEN)
}


// Adds "hidden" suffix to a path
// Returns:
// 0 - success
// -ENAMETOOLONG - if the new path is too long
static int $get_hid_path(char *newpath, const char *oldpath)
{
   $$ADDNSUFFIX(newpath, oldpath, $$EXT_HID, $$EXT_LEN)
}


// Maps virtual path into real path
// Puts the mapped path in fpath and returns
// 0 - if the path is in the main space
// -ENAMETOOLONG - if the mapped path is too long
static int $map_path(char *fpath, const char *path, struct $fsdata_t *fsdata)
{
   $$ADDNPREFIX(fpath, path, fsdata->rootdir, fsdata->rootdir_len)
}


// Checks and maps path (path into fpath)
// Puts the mapped path in fpath and returns
// 0 - if the path is in the main space
// -EACCES - if the path is in the snapshot space
// -ENAMETOOLONG - if the mapped path is too long
static int $cmpath(char *fpath, const char *path, struct $fsdata_t *fsdata)
{
   // If path starts with the snapshot folder
   if(unlikely(strncmp(path, $$SNDIR, $$SNDIR_LEN) == 0 && (path[$$SNDIR_LEN] == $$DIRSEPCH || path[$$SNDIR_LEN] == '\0'))){
      return -EACCES;
   }

   // Add prefix of root folder to map to underlying file
   return $map_path(fpath, path, fsdata); // TODO copy code here to ensure optimisation?
}


// Breaks up a VIRTUAL path in the snapshots space into a struct $snpath_t
// "/snapshots/ID/dir/dir/file"
// Returns snpath->is_there:
// 0 - if the string is "/snapshots" or "/snapshots?" (from $cmpath we'll know that ?='/') or "/snapshots//"
// 1 - if the string is "/snapshots/ID($|/)"
// 2 - if the string is "/snapshots/ID/..."
static int $decompose_sn_path(struct $snpath_t *snpath, const char *path)
{
   $$PATH_LEN_T len;
   $$PATH_LEN_T idlen;
   char *idstart;
   char *nextslash;

   len = strlen(path);
   // we know the string starts with "/snapshots"
   if(len <= $$SNDIR_LEN + 1){ // the string is "/snapshots" or "/snapshots?"
      snpath->is_there = 0;
      return 0;
   }

   idstart = $$SNDIR_LEN + path; // points to the slash at the end of "/snapshots/"
   nextslash = strchr(idstart + 1, $$DIRSEPCH);

   if(nextslash == NULL){ // there's no next '/', so the string must be "/snapshots/ID"
      snpath->is_there = 1;
      strcpy(snpath->id, idstart);
      return 1;
   }

   if(nextslash == idstart + 1){ // the string must be "/snapshots//"
      snpath->is_there = 0;
      return 0;
   }

   if(nextslash == path + len - 1){ // the next slash the last character, so "/snapshots/ID/"
      snpath->is_there = 1;
      idlen = len - $$SNDIR_LEN - 1;
      strncpy(snpath->id, idstart, idlen);
      snpath->id[idlen] = '\0';
      return 1;
   }

   // otherwise we've got "/snapshots/ID/..."
   snpath->is_there = 2;
   idlen = nextslash - idstart;
   strncpy(snpath->id, idstart, idlen);
   snpath->id[idlen] = '\0';
   strcpy(snpath->inpath, nextslash);
   return 0;
}


// Map snapshot ID ("/ID") into a real path
// Returns:
// 0 - success
// -ENAMETOOLONG - if the new path is too long
static int $sn_id_to_fpath(char *fpath, const char *id, struct $fsdata_t *fsdata)
{
   $$ADDNPREFIX(fpath, id, fsdata->sn_dir, strlen(fsdata->sn_dir)) // TODO cache length?
}


// Check consistency of constants
// Returns 0 on success, -1 on failure
int $check_params(void){
   if(strlen($$SNDIR) != $$SNDIR_LEN){ return -1; }
   if(strlen($$EXT_DATA) != $$EXT_LEN){ return -1; }
   if(strlen($$EXT_HID) != $$EXT_LEN){ return -1; }
   if(strlen($$DIRSEP) != 1){ return -1; }
   return 0;
}
