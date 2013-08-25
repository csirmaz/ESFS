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
// fcntl -- AT_FDCWD
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>
// sys/stat.h -- utimens
#include <sys/stat.h>

#include "log_c.h"

#include "util_c.c"
#include "snapshot_c.c"
#include "stat_c.c"
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
   // log_msg("\nstatfs(path=\"%s\", statv=0x%08x)\n", path, statv);
   $$IF_PATH_MAIN_ONLY

   // get stats for underlying filesystem
   if( statvfs(fpath, statv) == 0){ return 0; }
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

   return $$FSDATA; // TODO -- We put user_data into fuse_context
}

   /**
    * Clean up filesystem
    *
    * Called on filesystem exit.
    *
    * Introduced in version 2.3
    */
//   void (*destroy) (void *);
void $destroy(void *userdata)
{
   // TODO free all memory -- search for malloc
   /*
    * fsdata
    * realpath
    */
   log_msg("\ndestroy(userdata=0x%08x)\n", userdata);
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

void $usage()
{
   fprintf(stderr, "usage:  esfs [FUSE and mount options] rootDir mountPoint\n");
   abort();
}

int main(int argc, char *argv[])
{
   struct $fsdata_t *fsdata;

   // ESFS doesn't do any access checking on its own (the comment
   // blocks in fuse.h mention some of the functions that need
   // accesses checked -- but note there are other functions, like
   // chown(), that also need checking!).  Since running bbfs as root
   // will therefore open Metrodome-sized holes in the system
   // security, we'll check if root is trying to mount the filesystem
   // and refuse if it is.  The somewhat smaller hole of an ordinary
   // user doing it with the allow_other flag is still there because
   // I don't want to parse the options string.
   if ((getuid() == 0) || (geteuid() == 0)) {
      fprintf(stderr, "Running ESFS as root opens unnacceptable security holes. Aborting.\n");
      return 1;
   }

   // Perform some sanity checking on the command line:  make sure
   // there are enough arguments, and that neither of the last two
   // start with a hyphen (this will break if you actually have a
   // rootpoint or mountpoint whose name starts with a hyphen, but so
   // will a zillion other programs)
   if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
      $usage();

   fsdata = malloc(sizeof(struct $fsdata_t));
   if (fsdata == NULL) {
      fprintf(stderr, "out of memory\n");
      return 1;
   }

   // Pull the rootdir out of the argument list and save it in my
   // internal data
   fsdata->rootdir = realpath(argv[argc-2], NULL); // this calls malloc
   fsdata->rootdir_len = strlen(fsdata->rootdir);
   argv[argc-2] = argv[argc-1];
   argv[argc-1] = NULL;
   argc--;
   
   fsdata->logfile = log_open();

   // Initial things to do

   if($check_params() != 0){
      fprintf(stderr, "There's a problem with the parameters; ESFS need to be recompiled. Aborting.\n");
      return 1;
   }

   // Get the main snapshot dir path
   if($map_path(fsdata->sn_dir, $$SNDIR, fsdata) == -ENAMETOOLONG){
      fprintf(stderr, "Snapshot dir path is too long. Aborting.\n");
      return 1;
   }

   if($sn_check_dir(fsdata) != 0){
      fprintf(stderr, "Snapshot dir check failed, please check logs. Aborting.\n");
      return 1;
   }

   if($sn_get_latest(fsdata) != 0){
      fprintf(stderr, "Getting latest snapshot failed, please check logs. Aborting.\n");
      return 1;
   }

   // turn over control to fuse
   return fuse_main(argc, argv, &$oper, fsdata);
}

/*
FUSE options:
-d -o debug    enable debug output (implies -f)
-f    foreground operation
-s    disable multi-threaded operation
-o allow_other    allow access to other users
-o allow_root   allow access to root
-o nonempty    allow mounts over non-empty file/dir
-o default_permissions  enable permission checking by kernel
-o fsname=NAME    set file system name
-o large_read  issue large read requests (2.4 only)
-o max_read=N  set maximum size of read requests
-o hard_remove    immediate removal (don't hide files)
-o use_ino     let file system set inode numbers
-o readdir_ino    try to fill in d_ino in readdir
-o direct_io   use direct I/O
-o kernel_cache   cache files in kernel
-o umask=M  set file permissions (octal)
-o uid=N    set file owner
-o gid=N    set file group
-o entry_timeout=T   cache timeout for names (1.0s)
-o negative_timeout=T    cache timeout for deleted names (0.0s)
-o attr_timeout=T    cache timeout for attributes (1.0s)
http://sysdocs.stu.qmul.ac.uk/sysdocs/Comment/FuseUserFileSystems/FuseBase.html
*/