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


/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.    An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
// TODO Implement snapshots
//   int (*read) (const char *, char *, size_t, off_t,
//           struct fuse_file_info *);
// BBFS: I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.
int $read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
   int ret;

   log_msg("read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x, main fd=%d)\n", path, buf, size, offset, fi, $$MFD->mainfd);
   // TODO Check if path is null, as expected

   ret = pread($$MFD->mainfd, buf, size, offset);
   if(ret >= 0) { return ret; }
   return -errno;
}


/**
 * Helper function to read directories in snapshots
 *
 * We read the same directory in all snapshots (going forward), and
 * in the main space.
 * We read map files as necessary, as if a file is marked nonexistent,
 * it needs to be ignored in later snapshots.
 * We can pass the stat to FUSE, so we do that here, and lstat
 * the things for which there is no mapfile.
 */
static int $_sn_readdir(
   const char *path,
   void *buf,
   fuse_fill_dir_t filler,
   off_t offset,
   const struct $fsdata_t *fsdata,
   const struct $mfd_t *mfd
)
{
   int sni, j, k, p, fd;
   int allocated = 4;
   int used = 0;
   int waserror = 0; // negative on error
   struct dirent *de;
   struct $pathmark_t *pathmark;
   struct $pathmark_t *pathmark2;
   struct $sn_steps_t *step;
   char name[$$PATH_MAX];
   char fpath[$$PATH_MAX];
   struct $mapheader_t maphead;

   pathmark = malloc(allocated * sizeof(struct $pathmark_t));
   if(pathmark == NULL) {
      return -ENOMEM;
   }

   // Read all the directories in all the snapshots
   for(sni = mfd->sn_current; sni >= 0; sni--) { // begin for (a)

      step = &(mfd->sn_steps[sni]);

      $dlogdbg("Reading sn dir '%s' '%d'\n", step->path, step->mapfd);

      // Skip snapshots with no information
      if(step->mapfd == $$SN_STEPS_UNUSED) { continue; }

      // The directories are already open
      // Read everything in this directory
      while(1) { // begin while (b)
         errno = 0;
         de = readdir(step->dirfd);
         if(de == NULL) {
            if(errno != 0) {
               waserror = -errno;
            }
            break; // break while (b)
         }

         $dlogdbg("Found name '%s'\n", de->d_name);

         // Generate the absolute path for this file
         if(strlen(step->path) + 1 + strlen(de->d_name) > $$PATH_MAX) {
            waserror = -ENAMETOOLONG;
            break; // break while (b)
         }
         strcpy(fpath, step->path);
         strcat(fpath, $$DIRSEP);
         strcat(fpath, de->d_name);

         // Decide what this name means
         p = 0; // 0=check if new; 1=store; 2=nonexistent; -1=skip

         if(sni > 0) {

            // Decide how to present names in the snapshots
            j = $mfd_filter_name(de->d_name);
            $dlogdbg("Name sn type: j='%d'\n", j);

            if(j == 0) { continue; } // cont while (b). should not be shown (.dat file)

            strcpy(name, de->d_name);

            if(j == 2) { // .map file - remove the extension

               name[strlen(name) - $$EXT_LEN] = '\0';

               // This is a map file.
               // Unless we have already seen this file, we need to load
               // the mapheader to see if it is nonexistent.
               for(k = 0; k < used; k++) {
                  if(strcmp(name, pathmark[k].path) == 0) {
                     p = -1;
                     break; // break inner for (1)
                  }
               } // end inner for (1)
               if(p == -1) { continue; } // cont while. If we've seen this file, go to the next file in the dir

               // We haven't seen this file
               // we need to load the mapheader
               $dlogdbg("Directory listing: opening mapheader '%s'\n", fpath);
               fd = open(fpath, O_RDONLY);
               if(fd == -1) {
                  waserror = -errno;
                  $dlogdbg("Opening '%s' failed with %d = %s\n", fpath, -waserror, strerror(-waserror));
                  break; // break while (b)
               }
               if((waserror = $mfd_load_mapheader(&maphead, fd, fsdata)) != 0) {
                  $dlogdbg("mfd_load_mapheader failed with '%s'\n", strerror(-waserror));
                  close(fd); // cleanup
                  break; // break while (b)
               }
               if(close(fd) != 0) {
                  waserror = -errno;
                  break; // break while (b)
               }

               p = (maphead.exists == 1 ? 1 : 2);

            }

         } else {

            // In the main space, the names are names.
            strcpy(name, de->d_name);

         }

         if(p != 2) {

            // Is it stored already?
            if(p == 0) {
               for(j = 0; j < used; j++) {
                  if(strcmp(name, pathmark[j].path) == 0) {
                     p = -1;
                     break; // break inner for (2)
                  }
               } // end inner for (2)
               if(p == -1) { continue; } // cont. while (b)
            }

            // Get the stat
            if(p != 1) {
               // Note: we also stat . and .. here, but all from the selected snapshot dir
               if(lstat(fpath, &(maphead.fstat)) != 0) {
                  waserror = -errno;
                  break; // break while (b)
               }
            }

            // Give the data to FUSE
            $dlogdbg(" - '%s'\n", name);
            if(filler(buf, name, &(maphead.fstat), 0) != 0) {
               waserror = -ENOMEM;
               break; // break while (b)
            }

         }

         // Store the name as seen, even if it was marked as nonexistent

         $dlogdbg("Name seen: '%s' p='%d' used='%d'\n", name, p, used);

         if(used >= allocated) {
            // Allocate more memory
            allocated *= 2;
            pathmark2 = realloc(pathmark, allocated * sizeof(struct $pathmark_t));
            if(pathmark2 == NULL) {
               waserror = -ENOMEM;
               break; // break while (b)
            }
            pathmark = pathmark2;
         }

         strcpy(pathmark[used].path, name);
         used++;

      } // end while (b)

      if(waserror != 0) { break; } // break for (a)

   } // end for (a)

   $dlogdbg("Directory listing complete with error %d = %s\n", -waserror, strerror(-waserror));

   free(pathmark);

   return waserror;
}


/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int $readdir(
   const char *path,
   void *buf,
   fuse_fill_dir_t filler,
   off_t offset,
   struct fuse_file_info *fi
)
{
   struct $mfd_t *mfd;
   struct dirent *de;
   $$DFSDATA

   $dlogdbg("readdir(path=\"%s\", offset=%lld)\n", path, (long long int)offset);

   mfd = $$MFD;

   switch(mfd->is_main) {
      case $$MFD_MAIN:

         /* readdir: If the  end  of  the  directory stream is reached, NULL is returned
            and errno is not changed.
            If an error occurs, NULL is returned and errno is set
            appropriately.
         */

         while(1) {
            errno = 0;
            de = readdir(mfd->maindir);
            if(de == NULL) {
               if(errno != 0) { return -errno; }
               break;
            }
            if(filler(buf, de->d_name, NULL, 0) != 0) {
               return -ENOMEM;
            }
         }

         return 0;

      case $$MFD_SNROOT:

         // A normal directory read, but skip the .hid files

         while(1) {
            errno = 0;
            de = readdir(mfd->maindir);
            if(de == NULL) {
               if(errno != 0) { return -errno; }
               break;
            }

            if($sn_filter_name(de->d_name) == 0) {
               continue;
            }

            if(filler(buf, de->d_name, NULL, 0) != 0) {
               return -ENOMEM;
            }
         }

         return 0;

      case $$MFD_SN:

         return $_sn_readdir(path, buf, filler, offset, fsdata, mfd);

      default:

         $dlogdbg("Unknown mfd->is_main %d\n", mfd->is_main);
         return -EBADE;
   }
}


/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
// TODO Implement snapshots
//   int (*fgetattr) (const char *, struct stat *, struct fuse_file_info *);
// Since it's currently only called after bb_create(), and bb_create()
// opens the file, I ought to be able to just use the fd and ignore
// the path...
int $fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{

   // log_msg("fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n", path, statbuf, fi);

   if(fstat($$MFD->mainfd, statbuf) == 0) { return 0; }
   return -errno;
}


