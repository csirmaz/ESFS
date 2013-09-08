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

/* This file contains functions related to the snapshots themselves.
 *
 * Organisation of snapshots
 * =========================
 *
 * Each snapshot is a directory in ROOT/snapshots (or $$SNDIR).
 *
 * There's a pointer to the latest snapshot in /snapshots/.hid
 * if there is at least one snapshot.
 * Also, there's a pointer from each snapshot to the earlier one in /snapshots/<ID>.hid
 * All these pointers contain the real paths to the snapshot roots: ROOT/snapshots/<ID>
 */

// Create the snapshot dir if necessary.
// Returns
// 0 - on success
// -errno - if lstat or mkdir fails
// 1 - if the node exists but is not a directory
static int $sn_check_dir(struct $fsdata_t *fsdata) // $dlogi needs this to locate the log file
{
   struct stat mystat;
   int ret;

   $dlogi("Init: checking the snapshots dir '%s'\n", fsdata->sn_dir);

   if(lstat(fsdata->sn_dir, &mystat) == 0){
      // Check if it is a directory
      if(S_ISDIR(mystat.st_mode)){ return 0; }
      $dlogi("Init error: found something else than a directory at '%s'\n", fsdata->sn_dir);
      return 1;
   } else {
      ret = errno;
      if(ret == ENOENT){
         $dlogi("Init: creating the snapshots dir\n");
         if(mkdir(fsdata->sn_dir, 0700) == 0){ return 0; }
         ret = errno;
         $dlogi("Init error: mkdir on '%s' failed with '%s'\n", fsdata->sn_dir, strerror(ret));
         return -ret;
      }
      $dlogi("Init error: lstat on '%s' failed with '%s'\n", fsdata->sn_dir, strerror(ret));
      return -ret;
   }

   return 0;
}


// Get the earliest snapshot
//   reads fsdata ->sn_is_any, sn_lat_dir
//   Sets snpath to the real path of the earliest snapshot (".../snapshots/ID")
//     and prevpointerpath to the real path of the pointer file of the second earliest snapshot.
// Returns
//   1 - if there are no snapshots
//   0 - on success
//   -errno - on internal error
static int $sn_get_earliest(const struct $fsdata_t *fsdata, char snpath[$$PATH_MAX], char prevpointerpath[$$PATH_MAX])
{
   int ret;
   char pointerpath[$$PATH_MAX];

   if(fsdata->sn_is_any == 0){ return 1; } // no snapshots at all

   strcpy(snpath, fsdata->sn_lat_dir);

   while(1){

      ret = $get_hid_path(pointerpath, snpath);
      if(ret != 0){ return ret; }

      ret = $get_sndir_from_file(fsdata, snpath, pointerpath);

      if(ret == 0){ // no pointer to a previous snapshot; we've found the earliest one
         return 0;
      }

      if(ret < 0){ return ret; } // error

      strcpy(prevpointerpath, pointerpath);
      // TODO: check for infinite loops here?
   }

}


// Gets the path to the root of the latest snapshot:
//   reads .../snapshots/.hid
//   sets fsdata->sn_is_any
//   sets fsdata->sn_lat_dir
// Returns:
// 0 - on success
// -errno - on failure
static int $sn_get_latest(struct $fsdata_t *fsdata)
{
   int ret;
   char path[$$PATH_MAX]; // the pointer file, .../snapshots/.hid

   // We could store the path to the pointer instead of recreating it here and in $sn_set_latest
   if(unlikely(strlen(fsdata->sn_dir) >= $$PATH_MAX - $$EXT_LEN - 1)){
      return -ENAMETOOLONG;
   }
   strcpy(path, fsdata->sn_dir);
   strncat(path, $$DIRSEP, 1);
   strncat(path, $$EXT_HID, $$EXT_LEN);

   ret = $get_sndir_from_file(fsdata, fsdata->sn_lat_dir, path);

   if(ret == 0){
      fsdata->sn_is_any = 0;
      $dlogi("Get latest sn: %s not found, so there must be no snapshots\n", path);
      return 0;
   }

   if(ret < 0){ return ret; }

   fsdata->sn_is_any = 1;
   fsdata->sn_lat_dir_len = ret;
   $dlogdbg("Get latest sn: found latest snapshot '%s' in '%s'\n", fsdata->sn_lat_dir, path);
   return 0;
}


// Sets the path to the root of the latest snapshot:
//   writes .../snapshots/.hid
//   sets fsdata->sn_is_any
//   sets fsdata->sn_lat_dir
// Returns:
//   0 - on success
//   -errno - on failure
static int $sn_set_latest(struct $fsdata_t *fsdata, char *newpath)
{
   int fd;
   int ret;
   int len;
   char path[$$PATH_MAX]; // the pointer file, .../snapshots/.hid

   if(unlikely(strlen(fsdata->sn_dir) >= $$PATH_MAX - $$EXT_LEN - 1)){
      return -ENAMETOOLONG;
   }
   strcpy(path, fsdata->sn_dir);
   strncat(path, $$DIRSEP, 1);
   strncat(path, $$EXT_HID, $$EXT_LEN);

   fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
   if(fd == -1){
      fd = errno;
      $dlogi("Set latest sn: opening %s failed with %d = %s\n", path, fd, strerror(fd));
      return -fd;
   }

   len = strlen(newpath) + 1;
   ret = pwrite(fd, newpath, len, 0);
   if(ret == -1){
      ret = errno;
      $dlogi("Set latest sn: writing to %s failed with %d = %s\n", path, ret, strerror(ret));
      return -ret;
   }
   if(ret != len){
      $dlogi("Set latest sn: writing to %s returned %d bytes instead of %d\n", path, ret, len);
      return -EIO;
   }

   close(fd);

   strcpy(fsdata->sn_lat_dir, newpath);
   fsdata->sn_lat_dir_len = len - 1;
   fsdata->sn_is_any = 1;

   return 0;
}


// Initialises a new snapshot
//   path is a real path in the form .../snapshots/ID
// Returns:
// 0 - on success
// -1 - on failure
int $sn_create(struct $fsdata_t *fsdata, char *path)
{
   int fd;
   int ret;
   int len;
   int waserror = 0;
   char hid[$$PATH_MAX];

   $dlogi("Creating new snapshot at '%s'\n", path);

   // Create root of snapshot
   if(unlikely(mkdir(path, S_IRWXU) != 0)){
      ret = errno;
      $dlogi("Creating sn: creating %s failed with %d = %s\n", path, ret, strerror(ret));
      return -1;
   }

   do{
      if(fsdata->sn_is_any != 0){

         // Set up pointer file to previos snapshot
         ret = $get_hid_path(hid, path);
         if(ret < 0){
            $dlogi("Creating sn: creating %s failed when getting hid path\n", path);
            waserror = 1;
            break;
         }

         fd = open(hid, O_WRONLY | O_CREAT | O_EXCL, S_IRWXU);
         if(fd == -1){
            fd = errno;
            $dlogi("Creating sn: opening %s failed with %d = %s\n", hid, fd, strerror(fd));
            waserror = 1;
            break;
         }

         do{
            // Write into pointer file
            len = strlen(fsdata->sn_lat_dir) + 1;
            ret = pwrite(fd, fsdata->sn_lat_dir, len, 0);
            if(ret == -1){
               ret = errno;
               $dlogi("Creating sn: writing to %s failed with %d = %s\n", hid, ret, strerror(ret));
               waserror = 1;
               break;
            }
            if(ret != len){
               $dlogi("Creating sn: writing to %s returned %d bytes instead of %d\n", hid, ret, len);
               waserror = 1;
               break;
            }

            // Save latest sn
            if($sn_set_latest(fsdata, path) != 0){
               waserror = 1;
               break;
            }

         }while(0);

         close(fd);

         if(unlikely(waserror == 1)){
            $dlogi("Creating sn: Cleanup: removing %s\n", hid);
            unlink(hid);
         }

      } else { // else: no snapshots yet

            // Save latest sn
            if($sn_set_latest(fsdata, path) != 0){
               waserror = 1;
               break;
            }

      }
   }while(0);

   if(unlikely(waserror == 1)){
      $dlogi("Creating sn: Cleanup: removing %s\n", path);
      rmdir(path);
      return -1;
   }

   return 0;
}


// Destroy the earliest snapshot
// Returns
// 0 - on success
// -errno - on failure
static int $sn_destroy(struct $fsdata_t *fsdata)
{
   char snpath[$$PATH_MAX];
   char prevpointerpath[$$PATH_MAX];
   int ret;

   ret = $sn_get_earliest(fsdata, snpath, prevpointerpath);

   if(ret == 1){ return -ENOENT; } // there are no snapshots at all
   if(ret != 0){ return ret; } // internal error

   $dlogi("Removing snapshot '%s' and pointer '%s'\n", snpath, prevpointerpath);

   // Remove the "previous" pointer from the second earliest snapshot
   if(unlink(prevpointerpath) != 0){ return -errno; }

   // Remove the earliest snapshot
   if((ret = $recursive_remove(snpath)) != 0){ return ret; }

   return 0;
}


// Check if xattrs are suppored by the underlying filesystem
// They are not supported by ext4
// Returns
// 0 on success
// -errno on any failure
// 1 on unexpected value
static int $sn_check_xattr(struct $fsdata_t *fsdata) // $dlogi needs this to locate the log file
{
   int ret;
   char value[2];

#define $$XA_TEST "$test"

   $dlogi("Init: checking xattr support\n");

   ret = lremovexattr(fsdata->sn_dir, $$XA_TEST);
   if(ret != 0){
      ret = errno;
      if(ret != ENODATA){
         $dlogi("Init: removing xattr on %s failed with %d = %s\n", fsdata->sn_dir, ret, strerror(ret));
         return -ret;
      }
   }
   ret = lsetxattr(fsdata->sn_dir, $$XA_TEST, "A", 2, 0);
   if(ret != 0){
      ret = errno;
      $dlogi("Init: setting xattr on %s failed with %s\n", fsdata->sn_dir, strerror(ret));
      return -ret;
   }
   ret = lgetxattr(fsdata->sn_dir, $$XA_TEST, value, 2);
   if(ret != 0){
      $dlogi("Init: getting xattr on %s failed with %s\n", fsdata->sn_dir, strerror(ret));
      return -ret;
   }
   if(strcmp(value, "A") != 0){
      $dlogi("Init: got unexpected value from xattr on %s\n", fsdata->sn_dir);
      return 1;
   }
   ret = lremovexattr(fsdata->sn_dir, $$XA_TEST);
   if(ret != 0){
      ret = errno;
      $dlogi("Init: removing xattr on %s failed with %s\n", fsdata->sn_dir, strerror(ret));
      return -ret;
   }
   return 0;
}

