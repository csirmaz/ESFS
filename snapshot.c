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
 * All these pointers contain the real paths to the snapshot roots: "ROOT/snapshots/<ID>"
 */


/** Filters out internal names in the /snapshots/ dir
 *
 * Returns:
 * 1 - if the name should be shown
 * 0 - if the name is internal
 */
static inline int $sn_filter_name(char *name)
{
   $$PATH_LEN_T plen;

   plen = strlen(name);
   if(plen < $$EXT_LEN) { return 1; }
   if(strcmp(name, $$EXT_HID) == 0) { return 0; }
   if(strcmp(name + plen - $$EXT_LEN, $$EXT_HID) == 0) { return 0; }
   return 1;
}


/** Creates the snapshot dir if necessary.
 *
 * Returns
 * * 0 - on success
 * * -errno - if lstat or mkdir fails
 * * 1 - if the node exists but is not a directory
 */
static int $sn_check_dir(struct $fsdata_t *fsdata) // $dlogi needs this to locate the log file
{
   struct stat mystat;
   int ret;

   $dlogi("Checking the snapshots dir '%s'\n", fsdata->sn_dir);

   if(lstat(fsdata->sn_dir, &mystat) == 0) {
      // Check if it is a directory
      if(S_ISDIR(mystat.st_mode)) { return 0; }
      $dlogi("ERROR Found something else than a directory at '%s'\n", fsdata->sn_dir);
      return 1;
   } else {
      ret = errno;
      if(ret == ENOENT) {
         $dlogi("Creating the snapshots dir\n");
         if(mkdir(fsdata->sn_dir, 0700) == 0) { return 0; }
         ret = errno;
         $dlogi("Error: mkdir on '%s' failed with %d = '%s'\n", fsdata->sn_dir, ret, strerror(ret));
         return -ret;
      }
      $dlogi("Error: lstat on '%s' failed with %d = '%s'\n", fsdata->sn_dir, ret, strerror(ret));
      return -ret;
   }

   return 0;
}


/** Gets the earliest snapshot
 *
 * Reads fsdata ->sn_is_any, sn_lat_dir
 *
 * Sets snpath to the real path of the earliest snapshot (".../snapshots/ID")
 * and prevpointerpath to the real path of the pointer file of the second earliest snapshot.
 *
 * Returns
 * * 1 - if there are no snapshots
 * * 2 - if there is only one snapshot: snpath is set, but prevpointerpath is invalid
 * * 0 - on success
 * * -errno - on internal error
 */
static int $sn_get_earliest(const struct $fsdata_t *fsdata, char snpath[$$PATH_MAX], char prevpointerpath[$$PATH_MAX])
{
   int ret;
   int p = 0;
   char pointerpath[$$PATH_MAX];

   if(fsdata->sn_is_any == 0) { return 1; } // no snapshots at all

   strcpy(snpath, fsdata->sn_lat_dir);

   while(p < $$MAX_SNAPSHOTS) { // TODO 2 Use different infinite loop detection

      if((ret = $get_hid_path(pointerpath, snpath)) != 0) { return ret; }

      ret = $read_sndir_from_file(fsdata, snpath, pointerpath);

      if(ret == 0) { // no pointer found -- this is the earliest snapshot
         return (p == 0 ? 2 : 0);
      }

      if(ret < 0) { return ret; } // error

      strcpy(prevpointerpath, pointerpath);
      p++;
   }

   $dlogi("Error: too many snapshots or there is an infinite loop in the %s/ID%s files (probably caused by a bug). Sorry.\n", $$SNDIR, $$EXT_HID);
   return -EIO;

}


// TODO Invalidate open snapshot files on snapshot deletion - or rely on files continuing to exist?
/** Allocates memory for and compiles a list of paths for each snapshot up to a given one.
 *
 * Allocates memory for mfd->sn_steps and:
 * * Copies the real paths of the snapshot roots ("ROOT/snapshots/[ID]") into mfd->sn_steps->path[1..sn_current]
 * * Sets mfd->sn_current
 *
 * Returns
 * * 0 on success
 * * -errno on error
 */
static int $sn_get_paths_to(struct $mfd_t *mfd, const struct $snpath_t *snpath, const struct $fsdata_t *fsdata)
{
   int ret;
   void *pret = NULL;
   int allocated = 4;
   int p = 1;
   int waserror = 0; // negative on error
   char pointerpath[$$PATH_MAX];
   char mysnroot[$$PATH_MAX];

   // Get the real snapshot path we'll be comparing to
   if(snpath->is_there == $$snpath_root) {
      $dlogdbg("Attempted to list something outside a snapshot\n");
      return -EFAULT;
   }

   if(strlen(snpath->id) + strlen(fsdata->sn_dir) >= $$PATH_MAX) {
      return -ENAMETOOLONG;
   }
   strcpy(mysnroot, fsdata->sn_dir);
   strcat(mysnroot, snpath->id);

   mfd->sn_steps = malloc(sizeof(struct $sn_steps_t) * allocated);
   if(mfd->sn_steps == NULL) { return -ENOMEM; }

   strcpy(mfd->sn_steps[0].path, fsdata->rootdir); // the root of the main space
   strcpy(mfd->sn_steps[1].path, fsdata->sn_lat_dir); // the root of the latest snapshot

   while(1) {

      // $dlogdbg("sn_get_paths_to: searching for '%s' at '%s' (%d)\n", mysnroot, mfd->sn_steps[p].path, p);

      if(strcmp(mysnroot, mfd->sn_steps[p].path) == 0) {
         // We have found our snapshot ID
         break;
      }

      // Go to the previous snapshot

      if((ret = $get_hid_path(pointerpath, mfd->sn_steps[p].path)) != 0) {
         waserror = ret;
         break;
      }

      p++;
      if(p >= allocated) {
         allocated *= 2;
         if((pret = realloc(mfd->sn_steps, sizeof(struct $sn_steps_t) * allocated)) == NULL) {
            waserror = -ENOMEM;
            break;
         }
         mfd->sn_steps = pret;
      }

      if(p >= $$MAX_SNAPSHOTS) { // TODO 2 Use different infinite loop detection
         // Error: possible infinite loop?
         $dlogi("error: too many snapshots or there is an infinite loop in the %s/ID%s files (probably caused by a bug). Sorry.\n", $$SNDIR, $$EXT_HID);
         waserror = -EIO;
         break;
      }

      ret = $read_sndir_from_file(fsdata, mfd->sn_steps[p].path, pointerpath);

      if(ret == 0) { // no pointer found -- this is the earliest snapshot
         // The ID we searched for was not found.
         $dlogi("The requested ID '%s' was not found. Sorry.\n", snpath->id);
         waserror = -EFAULT;
         break;
      }
      if(ret < 0) { // error
         waserror = ret;
         break;
      }

   }

   if(waserror != 0) {
      free(mfd->sn_steps);
      return waserror;
   }

   mfd->sn_current = p;
   return 0;
}


/** Gets the path to the root of the latest snapshot
 *
 * * reads .../snapshots/.hid
 * * sets fsdata->sn_is_any
 * * sets fsdata->sn_lat_dir
 *
 * Returns:
 * * 0 - on success
 * * -errno - on failure
 */
static int $sn_get_latest(struct $fsdata_t *fsdata)
{
   int ret;
   char path[$$PATH_MAX]; // the pointer file, .../snapshots/.hid

   if((ret = $get_dir_hid_path(path, fsdata->sn_dir)) != 0) { return ret; }

   ret = $read_sndir_from_file(fsdata, fsdata->sn_lat_dir, path);

   if(ret == 0) {
      fsdata->sn_is_any = 0;
      $dlogdbg("'%s' not found, so there must be no snapshots\n", path);
      return 0;
   }

   if(ret < 0) { return ret; }

   fsdata->sn_is_any = 1;
   fsdata->sn_lat_dir_len = ret;
   $dlogdbg("Found latest snapshot '%s' in '%s'\n", fsdata->sn_lat_dir, path);
   return 0;
}


/** Sets the path to the root of the latest snapshot
 *
 * * writes .../snapshots/.hid
 * * sets fsdata->sn_is_any
 * * increases fsdata->sn_number
 * * sets fsdata->sn_lat_dir and _len
 *
 * Returns:
 * * 0 - on success
 * * -errno - on failure
 */
static int $sn_set_latest(struct $fsdata_t *fsdata, char *newpath)
{
   int fd;
   int ret;
   int len;
   char path[$$PATH_MAX]; // the pointer file, .../snapshots/.hid

   if((ret = $get_dir_hid_path(path, fsdata->sn_dir)) != 0) { return ret; }

   fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
   if(fd == -1) {
      fd = errno;
      $dlogi("ERROR opening %s failed with %d = %s\n", path, fd, strerror(fd));
      return -fd;
   }

   len = strlen(newpath) + 1;
   ret = pwrite(fd, newpath, len, 0);
   if(ret == -1) {
      ret = errno;
      $dlogi("ERROR writing to %s failed with %d = %s\n", path, ret, strerror(ret));
      return -ret;
   }
   if(ret != len) {
      $dlogi("ERROR writing to %s returned %d bytes instead of %d\n", path, ret, len);
      return -EIO;
   }

   close(fd);

   strcpy(fsdata->sn_lat_dir, newpath);
   fsdata->sn_lat_dir_len = len - 1;
   fsdata->sn_is_any = 1;
   fsdata->sn_number++;

   return 0;
}


/** Initialises a new snapshot
 *
 * Returns:
 * * 0 - on success
 * * -errno - on failure
 */
static int $sn_create(
   struct $fsdata_t *fsdata,
   char *path /**< path is a real path in the form .../snapshots/ID */
)
{
   int fd;
   int ret;
   int len;
   int waserror = 0; // positive on error
   char hid[$$PATH_MAX];

   $dlogi("Creating new snapshot at '%s'\n", path);

   // Create root of snapshot
   if(unlikely(mkdir(path, S_IRWXU) != 0)) {
      ret = errno;
      $dlogi("ERROR creating %s failed with %d = %s\n", path, ret, strerror(ret));
      return -ret;
   }

   do {
      if(fsdata->sn_is_any != 0) {

         // Set up pointer file to previos snapshot
         ret = $get_hid_path(hid, path);
         if(ret < 0) {
            $dlogi("ERROR creating %s failed when getting hid path\n", path);
            waserror = -ret;
            break;
         }

         fd = open(hid, O_WRONLY | O_CREAT | O_EXCL, S_IRWXU);
         if(fd == -1) {
            fd = errno;
            $dlogi("ERROR opening %s failed with %d = %s\n", hid, fd, strerror(fd));
            waserror = fd;
            break;
         }

         do {
            // Write into pointer file
            len = strlen(fsdata->sn_lat_dir) + 1;
            ret = pwrite(fd, fsdata->sn_lat_dir, len, 0);
            if(ret == -1) {
               ret = errno;
               $dlogi("ERROR writing to %s failed with %d = %s\n", hid, ret, strerror(ret));
               waserror = ret;
               break;
            }
            if(ret != len) {
               $dlogi("ERROR writing to %s returned %d bytes instead of %d\n", hid, ret, len);
               waserror = EIO;
               break;
            }

            // Save latest sn
            if((ret = $sn_set_latest(fsdata, path)) != 0) {
               waserror = -ret;
               break;
            }

         } while(0);

         close(fd);

         if(unlikely(waserror != 0)) {
            $dlogdbg("Cleanup: removing %s\n", hid);
            unlink(hid);
         }

      } else { // else: no snapshots yet

         // Save latest sn
         if((ret = $sn_set_latest(fsdata, path)) != 0) {
            waserror = -ret;
            break;
         }

      }
   } while(0);

   if(unlikely(waserror != 0)) {
      $dlogdbg("Cleanup: removing %s\n", path);
      rmdir(path);
      return -waserror;
   }

   return 0;
}


/** Destroys the earliest snapshot
 *
 * May set fsdata->sn_is_any
 *
 * Returns
 * * 0 - on success
 * * -errno - on failure
 */
static int $sn_destroy(struct $fsdata_t *fsdata)
{
   char snpath[$$PATH_MAX];
   char prevpointerpath[$$PATH_MAX];
   int ret;

   ret = $sn_get_earliest(fsdata, snpath, prevpointerpath);

   if(ret < 0) { // internal error
      $dlogdbg("sn_get_earliest returned error %d = %s\n", -ret, strerror(-ret));
      return ret;
   }

   if(ret == 1) { // there are no snapshots at all
      $dlogi("Cannot remove snapshot as there are no snapshots.\n");
      return -ENOENT;
   }

   if(ret == 2) { // removing the only snapshot

      $dlogi("Removing the last remaining snapshot '%s'\n", snpath);

      // Remove the pointer to the latest snapshot (snapshots/ID/.hid)
      if((ret = $get_dir_hid_path(prevpointerpath, fsdata->sn_dir)) != 0) { return ret; }
      if(unlink(prevpointerpath) != 0) { return -errno; }

      // Remove the snapshot
      if((ret = $recursive_remove(fsdata, snpath)) != 0) { return ret; }

      fsdata->sn_is_any = 0;

      return 0;
   }

   $dlogi("Removing snapshot '%s' and pointer '%s'\n", snpath, prevpointerpath);

   // Remove the "previous" pointer from the second earliest snapshot
   if(unlink(prevpointerpath) != 0) { return -errno; }

   // Remove the earliest snapshot
   if((ret = $recursive_remove(fsdata, snpath)) != 0) { return ret; }

   return 0;
}

