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


// In case it matters
// ------------------
#define likely(x) (__builtin_expect((x), 1))
#define unlikely(x) (__builtin_expect((x), 0))


// Logging
// -------
// important log lines
#if $$DEBUG > 0
#   define $dlogi(...) $_log(__VA_ARGS__)
#else
#   define $dlogi(...)
#endif

// debug messages
#if $$DEBUG > 1
#   define $dlogdbg(...) $_log(__VA_ARGS__)
#else
#   define $dlogdbg(...)
#endif

static inline void $_log(const struct $fsdata_t *fsdata, const char *format, ...)
{
   time_t mytime;
   struct tm timeinfo;
   int havetime = 0;
   va_list args;

   if(likely(time(&mytime) != -1)) {
      if(likely(gmtime_r(&mytime, &timeinfo) != NULL)) {
         havetime = 1;
      }
   }

   fprintf(
      fsdata->logfile,
      "\%04d-\%02d-\%02d \%02d:\%02d:\%02d[\%05d]",
      (havetime == 1 ? (1900 + timeinfo.tm_year) : 0),
      (havetime == 1 ? timeinfo.tm_mon : 0),
      (havetime == 1 ? timeinfo.tm_mday : 0),
      (havetime == 1 ? timeinfo.tm_hour : 0),
      (havetime == 1 ? timeinfo.tm_min : 0),
      (havetime == 1 ? timeinfo.tm_sec : 0),
      (pid_t)syscall(SYS_gettid)
   );

   va_start(args, format);

   vfprintf(fsdata->logfile, format, args);

   va_end(args);
}

/** Opens the logfile
 */
static FILE *log_open(const char *filename)
{
   FILE *logfile;

   logfile = fopen(filename, "w");
   if(logfile != NULL) {
      // set logfile to line buffering
      setvbuf(logfile, NULL, _IOLBF, 0);
   }

   return logfile;
}


/** Closes the logfile
 */
static void log_close(FILE *logfile)
{
   fclose(logfile);
}


/** Extracts and casts fsdata */
#define $$FSDATA ((struct $fsdata_t *) fuse_get_context()->private_data )

/** Defines, extracts and casts fsdata */
#define $$DFSDATA struct $fsdata_t *fsdata; fsdata = $$FSDATA;

/** Extracts and casts mfd */
// Here we cast a unit64_t to a pointer.
// This triggers warnings where a pointer is less than 8 bytes, like on 32-bit architecture.
// The reverse conversion is done by casting to intptr_t.
#define $$MFD ((struct $mfd_t *) fi->fh)

/** Defines and extracts fsdata and mfd */
#define $$DFSDATA_MFD struct $fsdata_t *fsdata; struct $mfd_t *mfd; fsdata = $$FSDATA; mfd = $$MFD;


/** Sleeps. Needs struct timespec delay */
#define $$SLEEP pselect(0, NULL, NULL, NULL, &delay, NULL);




/** Checks if (virtual) path is in the snapshot space */
#define $$_IS_PATH_IN_SN(path) (unlikely(strncmp(path, $$SNDIR, $$SNDIR_LEN) == 0 && (path[$$SNDIR_LEN] == $$DIRSEPCH || path[$$SNDIR_LEN] == '\0')))


/** Use this when the command writes - we don't allow that in the snapshot dir, only in the main space.
 *
 * Uses path; Defines fpath, fsdata
 */
#define $$IF_PATH_MAIN_ONLY \
   char fpath[$$PATH_MAX]; \
   $$DFSDATA \
   if($$_IS_PATH_IN_SN(path)) { return -EACCES; } \
   if($map_path(fpath, path, fsdata) != 0) { return -ENAMETOOLONG; }


/** Use this when there are two paths, path and newpath, and the command writes.
 *
 * Uses path; Defines fpath, fnewpath, fsdata
 */
#define $$IF_MULTI_PATHS_MAIN_ONLY \
   char fpath[$$PATH_MAX]; \
   char fnewpath[$$PATH_MAX]; \
   $$DFSDATA \
   if($$_IS_PATH_IN_SN(path)) { return -EACCES; } \
   if($map_path(fpath, path, fsdata) != 0) { return -ENAMETOOLONG; } \
   if($$_IS_PATH_IN_SN(newpath)) { return -EACCES; } \
   if($map_path(fnewpath, newpath, fsdata) != 0) { return -ENAMETOOLONG; }


/** Use these when there are different things to do in the two spaces
 *
 * Uses path; Defines fpath, fsdata, snpath, ret
 *
 * First branch: in snapshot space. fpath contains the mapped path; snpath the decomposed paths.
 * DO NOT RETURN FROM THE FOLLOWING BLOCK! SET 'snret' INSTEAD!
 * ! For performance, we only allocate memory for snpath when needed, but because of this,
 * one cannot return in the SN branch.
 */
#define $$IF_PATH_SN \
   char fpath[$$PATH_MAX]; \
   struct $snpath_t *snpath; \
   int snret = -EBADE; \
   $$DFSDATA \
   if($map_path(fpath, path, fsdata) != 0) { return -ENAMETOOLONG; } \
   if(unlikely($$_IS_PATH_IN_SN(path))) { \
      snpath = malloc(sizeof(struct $snpath_t)); \
      if(snpath == NULL){ return -ENOMEM; } \
      $decompose_sn_path(snpath, path);

/** Second branch: in main space. fpath contains the mapped path. */
#define $$ELIF_PATH_MAIN \
      free(snpath); \
      return snret; \
   }

/** Uses ret */
#define $$FI_PATH \
   return -EFAULT;


/** Adds a prefix to a path
 *
 * Returns:
 * * 0 - success
 * * -ENAMETOOLONG - if the new path is too long
 */
static inline int $get_prefix_path(char *newpath, const char *oldpath, const char *prefix, int prefixlen)
{
   if(likely(strlen(oldpath) < $$PATH_MAX - prefixlen)) {
      strncpy(newpath, prefix, prefixlen);
      newpath[prefixlen] = '\0';
      strcat(newpath, oldpath);
      return 0;
   }
   return -ENAMETOOLONG;
}


/** Adds the "map" suffix and a prefix to a path
 *
 * Returns:
 * * 0 - success
 * * -ENAMETOOLONG - if the new path is too long
 */
static inline int $get_map_prefix_path(char *newpath, const char *oldpath, const char *prefix, int prefixlen)
{
   if(likely(strlen(oldpath) < $$PATH_MAX - $$EXT_LEN - prefixlen)) {
      strncpy(newpath, prefix, prefixlen);
      newpath[prefixlen] = '\0';
      strcat(newpath, oldpath);
      strncat(newpath, $$EXT_MAP, $$EXT_LEN);
      return 0;
   }
   return -ENAMETOOLONG;
}


/** Adds the "dat" suffix and a prefix to a path
 *
 * Returns:
 * * 0 - success
 * * -ENAMETOOLONG - if the new path is too long
 */
static inline int $get_dat_prefix_path(char *newpath, const char *oldpath, const char *prefix, int prefixlen)
{
   if(likely(strlen(oldpath) < $$PATH_MAX - $$EXT_LEN - prefixlen)) {
      strncpy(newpath, prefix, prefixlen);
      newpath[prefixlen] = '\0';
      strcat(newpath, oldpath);
      strncat(newpath, $$EXT_DAT, $$EXT_LEN);
      return 0;
   }
   return -ENAMETOOLONG;
}


/** Adds the "hidden" suffix to a path
 *
 * Returns:
 * * 0 - success
 * * -ENAMETOOLONG - if the new path is too long
 */
static inline int $get_hid_path(char *newpath, const char *oldpath)
{
   if(likely(strlen(oldpath) < $$PATH_MAX - $$EXT_LEN)) {
      strcpy(newpath, oldpath);
      strncat(newpath, $$EXT_HID, $$EXT_LEN);
      return 0;
   }
   return -ENAMETOOLONG;
}


/** Adds the "map" suffix to a path
 *
 * Returns:
 * * 0 - success
 * * -ENAMETOOLONG - if the new path is too long
 */
static inline int $get_map_path(char *newpath, const char *oldpath)
{
   if(likely(strlen(oldpath) < $$PATH_MAX - $$EXT_LEN)) {
      strcpy(newpath, oldpath);
      strncat(newpath, $$EXT_MAP, $$EXT_LEN);
      return 0;
   }
   return -ENAMETOOLONG;
}


/** Adds the "dat" suffix to a path
 *
 * Returns:
 * * 0 - success
 * * -ENAMETOOLONG - if the new path is too long
 */
static inline int $get_dat_path(char *newpath, const char *oldpath)
{
   if(likely(strlen(oldpath) < $$PATH_MAX - $$EXT_LEN)) {
      strcpy(newpath, oldpath);
      strncat(newpath, $$EXT_DAT, $$EXT_LEN);
      return 0;
   }
   return -ENAMETOOLONG;
}


/** Adds a DIRSEP and the "hidden" suffix to a path, to get the main snapshot pointer
 *
 * Returns:
 * * 0 - success
 * * -ENAMETOOLONG - if the new path is too long
 */
static inline int $get_dir_hid_path(char *newpath, const char *oldpath)
{
   if(likely(strlen(oldpath) < $$PATH_MAX - $$EXT_LEN - 1)) {
      strcpy(newpath, oldpath);
      strncat(newpath, $$DIRSEP, 1);
      strncat(newpath, $$EXT_HID, $$EXT_LEN);
      return 0;
   }
   return -ENAMETOOLONG;
}


/** Maps virtual path into real path
 *
 * Puts the mapped path in fpath and returns:
 * * 0 - on success
 * * -ENAMETOOLONG - if the mapped path is too long
 */
static inline int $map_path(char *fpath, const char *path, const struct $fsdata_t *fsdata)
{
   if(likely(strlen(path) < $$PATH_MAX - fsdata->rootdir_len)) {
      strncpy(fpath, fsdata->rootdir, fsdata->rootdir_len);
      fpath[fsdata->rootdir_len] = '\0';
      strcat(fpath, path);
      return 0;
   }
   return -ENAMETOOLONG;
}


/** Breaks up a VIRTUAL path in the snapshots space into a struct $snpath_t
 *
 * For example: "/snapshots/ID/dir/dir/file"
 *
 * Returns snpath->is_there:
 * * $$snpath_root - if the string is "/snapshots" or "/snapshots?" (from $$_IS_PATH_IN_SN we'll know that ?='/') or "/snapshots//"
 * * $$snpath_id - if the string is "/snapshots/ID($|/)"
 * * $$snpath_full - if the string is "/snapshots/ID/..."
 */
static int $decompose_sn_path(struct $snpath_t *snpath, const char *path)
{
   $$PATH_LEN_T len;
   $$PATH_LEN_T idlen;
   const char *idstart;
   const char *nextslash;

   len = strlen(path);
   // we know the string starts with "/snapshots"
   if(len <= $$SNDIR_LEN + 1) { // the string is "/snapshots" or "/snapshots?"
      snpath->is_there = $$snpath_root;
      return 0;
   }

   idstart = $$SNDIR_LEN + path; // points to the slash at the end of "/snapshots/"
   nextslash = strchr(idstart + 1, $$DIRSEPCH);

   if(nextslash == NULL) { // there's no next '/', so the string must be "/snapshots/ID"
      snpath->is_there = $$snpath_id;
      strcpy(snpath->id, idstart);
      return 1;
   }

   if(nextslash == idstart + 1) { // the string must be "/snapshots//"
      snpath->is_there = $$snpath_root;
      return 0;
   }

   if(nextslash == path + len - 1) { // the next slash the last character, so "/snapshots/ID/"
      snpath->is_there = $$snpath_id;
      idlen = len - $$SNDIR_LEN - 1;
      strncpy(snpath->id, idstart, idlen);
      snpath->id[idlen] = '\0';
      return 1;
   }

   // otherwise we've got "/snapshots/ID/..."
   snpath->is_there = $$snpath_full;
   idlen = nextslash - idstart;
   strncpy(snpath->id, idstart, idlen);
   snpath->id[idlen] = '\0';
   strcpy(snpath->inpath, nextslash);
   return 0;
}


/** Hash a string into a number
 *
 * This is a modified version of djb2.
 * For djb2, please see
 * http://stackoverflow.com/questions/1579721/can-anybody-explain-the-logic-behind-djb2-hash-function
 */
static inline unsigned long $djb2(const unsigned char *s)
{
   unsigned long key = 5381;
   int i;
   $$PATH_LEN_T j;

   for(i = 0; i < 5; i++) {
      for(j = 0; s[j]; j++) {
         key = ((key << 5) + key) + s[j];
      }
   }
   return key;
}


/** Helper function to get a suitable lock label from a string
 *
 */
static inline $$LOCKLABEL_T $string2locklabel(const char *s)
{
   unsigned long key;

   key = $djb2((unsigned char *) s);
   if(key != 0) { return key; } // 0 is a special value in the lock system
   return 1;
}


/** Reads a snapshot dir path from a file
 *
 * Sets:
 * * buf
 *
 * Returns:
 * * length of path - on success
 * * 0 - if the file does not exist
 * * -errno - on other failure
 */
static int $read_sndir_from_file(const struct $fsdata_t *fsdata, char buf[$$PATH_MAX], const char *filepath)
{
   int fd;
   int ret;

   if((fd = open(filepath, O_RDONLY)) == -1) {
      fd = errno;
      if(fd == ENOENT) {
         return 0;
      }
      $dlogi("ERROR opening %s failed with %d = %s\n", filepath, fd, strerror(fd));
      return -fd;
   }

   ret = pread(fd, buf, $$PATH_MAX, 0);
   if(ret == -1) {
      ret = errno;
      $dlogi("ERROR reading from %s failed with %d = %s\n", filepath, ret, strerror(ret));
      return -ret;
   }
   if(ret == 0 || buf[ret - 1] != '\0') {
      $dlogi("ERROR reading from '%s' returned '%d'==0 bytes or string is not 0-terminated ('%c').\n", filepath, ret, buf[ret]);
      return -EIO;
   }

   close(fd);

   // TODO 2 check if the directory really exists before returning success?

   return (ret - 1);
}


/** Checks consistency of constants
 *
 * Returns
 * * 0 on success
 * * <0 on failure
 */
static int $check_params(void)
{
   // Check string lengths
   if(strlen($$SNDIR) != $$SNDIR_LEN) { return -51; }
   if(strlen($$EXT_DAT) != $$EXT_LEN) { return -52; }
   if(strlen($$EXT_HID) != $$EXT_LEN) { return -53; }
   if(strlen($$DIRSEP) != 1) { return -54; }

   // Check if the block and block pointer sizes make it possible to store files off_t large.
   // The max file size + 1 on the system is 2^( sizeof(off_t)*8 ) bytes.
   // The max file size + 1 in ESFS is $$BL_S * 2^($$BLP_S*8) = 2^( log($$BL_S) + $$BLP_S*8 ) bytes.
   if(sizeof($$BLP_T) != $$BLP_S) { return -10; }
   if(sizeof(off_t) * 8.0 > ($$BL_SLOG + ((double)$$BLP_S) * 8.0)) { return -11; }
   if((1 << $$BL_SLOG) != $$BL_S) { return -12; }

   return 0;
}

