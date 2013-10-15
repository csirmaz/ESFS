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

// This file contains low-level tools that use locking


/** Helper function for the implementation of rm -r
 *
 * Returns 0 or errno.
 */
int $_univ_rm(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
   if(typeflag == FTW_DP || typeflag == FTW_D || typeflag == FTW_DNR) {
      if(rmdir(fpath) != 0) { return errno; }
      return 0;
   }

   if(unlink(fpath) != 0) { return errno; }
   return 0;
}

/** Implementation of rm -r
 *
 * Returns 0 or -errno.
 */
// TODO 2 Make this atomic (and thread safe) by renaming the snapshot first
static inline int $recursive_remove(struct $fsdata_t *fsdata, const char *path)
{
   int ret, lock;

   if((lock = $mflock_lock(fsdata, $string2locklabel($$LOCKLABEL_RMDIR))) < 0) { return lock; }
   ret = nftw(path, $_univ_rm, $$RECURSIVE_RM_FDS, FTW_DEPTH | FTW_PHYS);
   if((lock = $mflock_unlock(fsdata, lock)) != 0) { return lock; }
   if(ret >= 0) { return -ret; } // success or errno
   return -ENOMEM; // generic error encountered by nftw
}


/** Implementation of mkdir -p
 *
 * Creates the directory at *the parent of* path and any parent directories as needed
 *
 * If firstcreated != NULL, it is set to be the top directory created
 *
 * Returns:
 * * 0 if the directory already exists
 * * 1 if a directory has been created
 * * -errno on failure.
 */
// NOTE: dirname() and basename() are not thread safe
static int $mkpath(const char *path, char firstcreated[$$PATH_MAX], mode_t mode)
{
   int slashpos;
   int statret;
   int mkpathret;
   int mkdirret;
   char prepath[$$PATH_MAX];
   struct stat mystat;

   // find previous slash
   slashpos = strlen(path) - 1;
   while(slashpos >= 0 && path[slashpos] != $$DIRSEPCH) { slashpos--; }
   if(slashpos > 0) { // found one
      strncpy(prepath, path, slashpos);
      prepath[slashpos] = '\0';
   } else { // no slash or slash is first character
      return -EBADE;
   }

   // $dlogdbg("mkpath: %s <- %s\n", path, prepath);

   if(lstat(prepath, &mystat) != 0) {
      statret = errno;
      if(statret == ENOENT) { // the parent node does not exist

         mkpathret = $mkpath(prepath, firstcreated, mode);
         // $dlogdbg("mkpath: recursion returned with %d\n", p);
         if(mkpathret < 0) { return mkpathret; } // error

         // attempt to create the directory
         if(mkdir(prepath, mode) != 0) {
            mkdirret = errno;
            // Don't abort on EEXIST as another thread might be busy creating
            // the same directories. Simply assume success.
            if(mkdirret != EEXIST) {
               return -mkdirret;
            }
         }

         // save first directory created
         if(mkpathret == 0 && firstcreated != NULL) { strcpy(firstcreated, prepath); }

         return 1;
      }
      return -statret;
   }

   if(S_ISDIR(mystat.st_mode)) {
      return 0; // node exists and is a directory
   }

   return -ENOTDIR; // found a node but it isn't a directory
}

