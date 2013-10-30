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


#include "params_c.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h> // AT_FDCWD
#include <fuse.h>
#include <libgen.h>
#include <limits.h> // PATH_MAX
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h> // va_list, &c.
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h> // utimens
#include <sys/select.h> // pselect
#if $$DEBUG > 0
#  include <sys/syscall.h> // for gettid only
#endif
#include <pthread.h> // mutexes
#include <ftw.h> // nftw

#include "types_c.h"
#include "util_c.c"
#include "mflock_c.c"
#include "util_locking_c.c"
#include "snapshot_c.c"
#include "mfd_c.c"
#include "block_c.c"
#include "fuse_fd_close_c.c"
#include "fuse_fd_read_c.c"
#include "fuse_fd_write_c.c"
#include "fuse_path_open_c.c"
#include "fuse_path_read_c.c"
#include "fuse_path_write_c.c"

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come from /usr/include/fuse.h


/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
//   int (*statfs) (const char *, struct statvfs *);
int $statfs(const char *path, struct statvfs *statv)
{
   $$IF_PATH_MAIN_ONLY

   $dlogdbg("statfs(path=\"%s\")\n", path);

   // get stats for underlying filesystem
   if(statvfs(fpath, statv) == 0) { return 0; }
   return -errno;

   // log_statvfs(statv);
}



/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
//   void *(*init) (struct fuse_conn_info *conn);
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
void *$init(struct fuse_conn_info *conn)
{
   struct $fsdata_t *fsdata;

   fsdata = ((struct $fsdata_t *) fuse_get_context()->private_data);

   $dlogi("Initialised ESFS\n");

   return fsdata;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
//   void (*destroy) (void *userdata==private_data);
void $destroy(void *privdata)
{
   // TODO free all memory

   struct $fsdata_t *fsdata;

   fsdata = ((struct $fsdata_t *) privdata);

   $mflock_destroy(fsdata);
   $b_destroy_block_buffer(fsdata);

   $dlogi("Bye!\n");

#if $$DEBUG > 0
   log_close(fsdata->logfile);
#endif

   free(fsdata->rootdir);
   free(fsdata);
}



struct fuse_operations $oper = {
   .getattr = $getattr,
   .readlink = $readlink,
   .mknod   = $mknod,
   .mkdir   = $mkdir,
   .unlink  = $unlink,
   .rmdir   = $rmdir,
   .symlink = $symlink,
   .rename  = $rename,
   .link    = $link,
   .chmod   = $chmod,
   .chown   = $chown,
   .truncate = $truncate,
   .open    = $open,
   .read    = $read,
   .write   = $write,
   .statfs  = $statfs,
   .flush   = $flush,
   .release = $release,
   .fsync     = $fsync,
   .setxattr  = $setxattr,
   .getxattr  = $getxattr,
   .listxattr = $listxattr,
   .removexattr = $removexattr,
   .opendir    = $opendir,
   .readdir    = $readdir,
   .releasedir = $releasedir,
   .fsyncdir   = $fsyncdir,
   .init    = $init,
   .destroy = $destroy,
   .access  = $access,
   .create  = $create,
   .ftruncate = $ftruncate,
   .fgetattr  = $fgetattr,
   .utimens   = $utimens
};


void $usage(void)
{
   fprintf(stderr, "USAGE:  esfs [ FUSE and mount options ] [--local-log] (RootDir) (MountPoint)\n\n");
}


int main(int argc, char *argv[])
{
   int ret;
   int local_log = 0;
   struct $fsdata_t *fsdata;

   // The FS doesn't do any access checking on its own (the comment
   // blocks in fuse.h mention some of the functions that need
   // accesses checked -- but note there are other functions, like
   // chown(), that also need checking!).  Since running bbfs as root
   // will therefore open holes in the system
   // security, we'll check if root is trying to mount the filesystem
   // and refuse if it is.  The somewhat smaller hole of an ordinary
   // user doing it with the allow_other flag is still there because
   // we don't parse the options string.
   if((getuid() == 0) || (geteuid() == 0)) {
      fprintf(stderr, "Running ESFS as root opens security holes. Aborting.\n");
      return 1;
   }

   if((ret = $check_params()) != 0) {
      fprintf(stderr, "There's a problem with the parameters; ESFS needs to be recompiled. Code = %d. Aborting.\n", ret);
      return 1;
   }

   // If the first argument is "-h", call fuse to print the FUSE and mount options
   if(argc > 1 && strcmp(argv[1], "-h") == 0) {
      for(ret = 2; ret < argc; ret++) { argv[ret] = NULL; }
      argc = 2;
      $usage();
      fprintf(stderr, "FUSE AND MOUNT OPTIONS:\n\n");
      fuse_main(argc, argv, NULL, NULL);
      return 1;
   }

   // Perform some sanity checking on the command line:  make sure
   // there are enough arguments, and that neither of the last two
   // start with a hyphen (this will break if you actually have a
   // rootpoint or mountpoint whose name starts with a hyphen)
   if((argc < 3) || (argv[argc - 2][0] == '-') || (argv[argc - 1][0] == '-')) {
      $usage();
      return 1;
   }

   fsdata = malloc(sizeof(struct $fsdata_t));
   if(fsdata == NULL) {
      fprintf(stderr, "Out of memory. Aborting.\n");
      return 1;
   }

   // Pull the rootdir out of the argument list and save it in the internal data
   fsdata->rootdir = realpath(argv[argc - 2], NULL); // this calls malloc
   if(fsdata->rootdir == NULL) {
      fprintf(stderr, "Error getting the root directory from '%s'. Aborting.\n", argv[argc - 2]);
      return 1;
   }

   fsdata->rootdir_len = strlen(fsdata->rootdir);
   fsdata->sn_number = 0;

   argv[argc - 2] = argv[argc - 1];
   argv[argc - 1] = NULL;
   argc--;

   // Pull the optional local-log argument out of the argument list
   if(strcmp(argv[argc - 2], "--local-log") == 0) {
      local_log = 1;
      argv[argc - 2] = argv[argc - 1];
      argv[argc - 1] = NULL;
      argc--;
   }

#if $$DEBUG > 0
   if((fsdata->logfile = log_open(local_log == 0 ? "/var/log/esfs.log" : "esfs.log")) == NULL) {
      fprintf(stderr, "Could not open the logfile at '%s'. Aborting.\n", (local_log == 0 ? "/var/log/esfs.log" : "esfs.log"));
      return 1;
   }
#endif

   // Initial things to do

   // Get the main snapshot dir path
   if($map_path(fsdata->sn_dir, $$SNDIR, fsdata) != 0) {
      fprintf(stderr, "Snapshot directory path is too long. Aborting.\n");
      return 1;
   }

   if($sn_check_dir(fsdata) != 0) {
      fprintf(stderr, "Snapshot directory check failed, please check the logs. Aborting.\n");
      return 1;
   }

   if($sn_get_latest(fsdata) != 0) {
      fprintf(stderr, "Getting the latest snapshot failed, please check the logs. Aborting.\n");
      return 1;
   }

   if($b_init_block_buffer(fsdata) != 0){
      fprintf(stderr, "Failed to initialise the global block buffer. Aborting.\n");
      return 1;
   }

   if($mflock_init(fsdata) != 0) {
      fprintf(stderr, "Failed to initialise the mutexes. Aborting.\n");
      return 1;
   }

   // turn over control to fuse
   // user_data   user data supplied in the context during the init() method
   // Returns: 0 on success, nonzero on failure
   if((ret = fuse_main(argc, argv, &$oper, fsdata)) != 0) {
      fprintf(stderr, "FUSE returned with error '%d'. Aborting.\n", ret);
   }
   return ret;
}
