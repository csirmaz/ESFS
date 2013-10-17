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

/* This file contains functions related to saving changes in the data in
 * the main files.
 *
 * How blocks are saved
 * ====================
 *
 * This is done by splitting the main file into blocks of $$BL_S size.
 * For each block, there is a pointer of $$BLP_L size in the map file.
 * When a block is modified, the old block is appended to the dat file,
 * and its number is saved in the pointer, starting with 1.
 * If the pointer is already non-0, that is, the block has already been
 * saved, it is not saved again.
 * The map file might become a sparse file, but that's fine as
 * uninitalised parts will return 0.
 *
 * Useful information
 * ==================
 *
 * "If the file was open(2)ed with O_APPEND, the file offset is first set to
       the end of the file before writing.  The adjustment of the file offset and the write operation are performed as an atomic step."
 * "POSIX requires that a read(2) which can be proved to occur after a write() has returned returns the new data.  Note that not all file systems are
       POSIX conforming." (man write)
 *
 * http://librelist.com/browser//usp.ruby/2013/6/5/o-append-atomicity/#757836465710253f784f6446f028292d
 *
 * Parallel writes
 * ===============
 *
 * The writing procedure is the following:
 *
 * 1. read map
 * 2. read block from main file
 * 3. write block to dat file
 * 4. write map
 * 5. allow write to main file
 *
 * We need locking because if a different thread reads the map (1) before this thread
 * reaches (4), it will think it needs to save the block. However, the block in the
 * main file may be written by this thread before it reads it.
 *
 * Also, appending to the dat file and getting where we wrote to are not atomic
 * operations.
 */


/** Reads data from a snapshot file
 *
 * Returns:
 * * >=0 - the number of bytes read on success
 * * -errno - on error
 */
static int $b_read(
   char *buf,
   const struct $fsdata_t *fsdata,
   const struct $mfd_t *mfd,
   size_t readsize,
   off_t readoffset
)
{
   $$BLP_T pointer;
   off_t blockoffset, mapoffset, copyfrom;
   size_t blocknumber, copylength;
   ssize_t copyto;
   int sni, ret, waserror, copyfd;

   $dlogdbg("b_read: About to read offset='%zu' size='%td'\n", readoffset, readsize);
   if(readsize == 0) { return 0; }

   // Adjust how many bytes we can read based on the filesize
#define $$B_SNSIZE blockoffset // variable to store original file size
   $$B_SNSIZE = mfd->mapheader.fstat.st_size;
   if(readoffset + readsize > $$B_SNSIZE) {
      if(readoffset >= $$B_SNSIZE) {
         $dlogdbg("Requested offset='%zu' beyond file size='%zu'\n", readoffset, $$B_SNSIZE);
         // The man page for pread suggests it can return the errors of lseek,
         // and for lseek, EINVAL means: "whence is not valid.  Or: the resulting file offset would be negative, or beyond the end of a seekable device".
         // On the other hand, a 0 return value is supposed to signal EOF, and cp appears to expect this.
         return 0;
      }
      readsize = $$B_SNSIZE - readoffset; // warning: size_t is unsigned and can underflow
   }
#undef $$B_SNSIZE

   // See which blocks we need to read
   blockoffset = (readoffset >> $$BL_SLOG);
   blocknumber = (readsize >> $$BL_SLOG) + 1;

   $dlogdbg("b_read: adjusted readoffs='%zu' readsize='%td' bloffs='%zu' blockno='%td'\n", readoffset, readsize, blockoffset, blocknumber);

   copyto = 0;

   for(; blocknumber > 0; blocknumber--, blockoffset++, copyto += copylength) {

      // blocknumber -- number of blocks still to read
      // blockoffset -- the offset of the current block to read in the reconstructed file
      // copyto -- offset in buf to read data to
      // copyfrom -- offset in file being read
      // copylength -- bytes to copy from file to buf
      // copyfd -- fd to use

      $dlogdbg("b_read: reading block no '%zu'\n", blockoffset);

      mapoffset = (sizeof(struct $mapheader_t)) + blockoffset * $$BLP_S; // TODO 2 There shouldn't be overflow as blockoffset is off_t

      copylength = $$BL_S;
      copyfrom = 0;
      if(copyto == 0) { // if we're reading the first block
         // we initialise copyfrom with the offset from the beginning of the block to the real offset
         copyfrom = readoffset - (blockoffset << $$BL_SLOG);
         // copylength is then smaller
         copylength -= copyfrom;
         // $dlogdbg("b_read: first block: readoffset='%zu', blockoffset='%zu', blockoffset**='%zu' -> copyfrom='%zu'\n", readoffset, blockoffset, (blockoffset << $$BL_SLOG), copyfrom);
         // $dlogdbg("b_read: first block: -> copylength='%td', blocksize='%d'\n", copylength, $$BL_S);
      }

      if(blocknumber == 1) { // if this is the last block
         // copylength is smaller again.
         // At this point, blockoffset is already original_blockoffset + blocknumber - 1
         copylength -= ((blockoffset + 1) << $$BL_SLOG) - readoffset - readsize;
      }

      $dlogdbg("b_read: initial copyfrom='%zu', copylength='%td'\n", copyfrom, copylength);

      // Now see where we can read the block from
      for(sni = mfd->sn_first_file; sni >= 0; sni--) {

         $dlogdbg("b_read: trying snapshot '%d' = '%s'\n", sni, mfd->sn_steps[sni].path);

         if(sni > 0) { // a snapshot file

            // TODO do we need any locking when reading?

            if(mfd->sn_steps[sni].mapfd < 0) { continue; } // go the next snapshot if there is not map file here

            ret = pread(mfd->sn_steps[sni].mapfd, &pointer, $$BLP_S, mapoffset);
            if(unlikely(ret != $$BLP_S && ret != 0)) {
               waserror = (ret == -1 ? errno : ENXIO);
               $dlogi("*** Error: pread on map ret=%d err=%s\n", ret, strerror(waserror));
               return -waserror;
            }
            if(ret == 0) { pointer = 0; }
            $dlogdbg("b_read: pointer read from map at mapoffs='%td', ret='%d', pointer='%td'\n", mapoffset, ret, pointer);

            if(pointer == 0) { continue; } // go to next snapshot

            $dlogdbg("b_read: block found in snapshot '%d' at '%td'\n", sni, pointer - 1);
            copyfd = mfd->sn_steps[sni].datfd;
            copyfrom += ((pointer - 1) << $$BL_SLOG);

         } else { // we are reading from the main file

            $dlogdbg("b_read: reading block from main file\n");
            copyfd = mfd->sn_steps[sni].datfd;
            copyfrom += (blockoffset << $$BL_SLOG);

         }

         $dlogdbg("b_read: final copyfrom='%zu', copyto='%td', copylength='%td'\n", copyfrom, copyto, copylength);

         ret = pread(copyfd, buf + copyto, copylength, copyfrom);
         if(unlikely(ret != copylength)) {
            waserror = (ret == -1 ? errno : ENXIO);
            $dlogi("*** Error: pread from file '%d' sni='%d' ret='%d' err='%s'\n", copyfd, sni, ret, strerror(waserror));
            return -waserror;
         }

         // We've found the block -- no need to look further
         break;

      } // end for sni

   } // end for block

   $dlogdbg("b_read: Read '%zu' bytes\n", copyto);
   return (int)copyto;
}


#define $$BLOCK_READ_POINTER \
      ret = pread(mfd->mapfd, &pointer, $$BLP_S, mapoffset); \
      if(unlikely(ret != $$BLP_S && ret != 0)){ \
         waserror = (ret==-1 ? errno : ENXIO); \
         $dlogdbg("Error: pread on .map for main file FD %d, map FD %d, err (%d) %d = %s\n", mfd->mainfd, mfd->mapfd, ret, waserror, strerror(waserror)); \
         break; \
      } \
      if(ret == 0){ pointer = 0; } \
      $dlogdbg("b_write: Read %zu as pointer from fd %d offs %td for main FD %d\n", pointer, mfd->mapfd, mapoffset, mfd->mainfd);

/** Saves the overwritten part of a file
 *
 * Returns:
 * * 0 - on success
 * * -errno - on failure
 */
static inline int $b_write(
   struct $fsdata_t *fsdata, // not const as locks are written
   struct $mfd_t *mfd,
   size_t writesize,
   off_t writeoffset
)
{
   $$BLP_T pointer;
   off_t mapoffset;
   off_t datsize;
   int waserror = 0;
   int lock = -1;
   char *buf = NULL;
   off_t blockoffset; // starting number of blocks written
   size_t blocknumber; // number of blocks written
   int ret;

   // Nothing to do if the main file is read-only or there are no snapshots or the file was empty in the snapshot
   if(mfd->datfd < 0 || writesize == 0) { return 0; }

   $dlogdbg("b_write: woffset: %zu wsize: %td size_in_sn: %zu\n", writeoffset, writesize, mfd->mapheader.fstat.st_size);

   // Don't save blocks outside original length of main file
   // Because datfd>=0, we know the file existed when the snapshot was taken, so mapheader.fstat should be valid.
#define $$B_SNSIZE blockoffset // variable to store the original file size in during this short block
   $$B_SNSIZE = mfd->mapheader.fstat.st_size;
   if(writeoffset + writesize > $$B_SNSIZE) {
      if(writeoffset >= $$B_SNSIZE) { // the offset is already past the length
         $dlogdbg("b_write: nothing to write\n");
         return 0;
      }
      writesize = $$B_SNSIZE - writeoffset; // warning: size_t is unsigned and can underflow
   }
#undef $$B_SNSIZE

   // See which blocks we need to write
   blockoffset = (writeoffset >> $$BL_SLOG);
   blocknumber = (writesize >> $$BL_SLOG);
   if((blocknumber << $$BL_SLOG) != writesize) { blocknumber++; }

   $dlogdbg("b_write: bloffs %zu blockno %td (%td,%td)\n", blockoffset, blocknumber, writesize, (writesize >> $$BL_SLOG));

   for(; blocknumber > 0; blocknumber--, blockoffset++) {

      // ============== BLOCK LOOP =================

      $dlogdbg("b_write: Processing block no %zu from main FD %d\n", blockoffset, mfd->mainfd);

      mapoffset = (sizeof(struct $mapheader_t)) + blockoffset * $$BLP_S; // TODO 2 There shouldn't be overflow as blockoffset is off_t

      // Read the pointer from the map file.
      // This may not be the final pointer if we haven't got the lock, but if it's already non-0, we
      // save ourselves the trouble of getting the lock.
      $$BLOCK_READ_POINTER

      if(pointer != 0) { continue; } // We don't need to save again, so go to next block

      // Read the pointer from the map file - for real.
      // For this to work correctly, we need to make sure that the underlying FS is POSIX conforming,
      // and that any write has returned on this file (see the quote above).
      // It seems we can only be sure that we're reading the new value then.
      // So we lock here.
      if(lock == -1) { // If we already have the lock, the first read was for real.

         if(unlikely((lock = $mflock_lock(fsdata, mfd->locklabel)) < 0)) {
            waserror = -lock;
            $dlogi("*** Error: lock for main file FD %d, err %d = %s\n", mfd->mainfd, waserror, strerror(waserror));
            break;
         }
         $dlogdbg("b_write: Got lock %d for main file FD %d\n", lock, mfd->mainfd);


         // Read the pointer from the map file
         $$BLOCK_READ_POINTER

         if(pointer != 0) { continue; } // We don't need to save

      }

      // We need to save the block
      // TODO implement a list of buffers various threads can lock and use
      if(buf == NULL) { // only allocate once in the loop
         buf = malloc($$BL_S);
         if(unlikely(buf == NULL)) {
            waserror = ENOMEM;
            break;
         }
      }

      // Read the old block from the main file
      ret = pread(mfd->mainfd, buf, $$BL_S, (blockoffset << $$BL_SLOG)); // TODO check all left shifts for potential overflow. Here, blockoffset is off_t
      if(unlikely(ret < 1)) { // We should be able to read from the main file at least 1 byte
         waserror = (ret == -1 ? errno : ENXIO);
         $dlogi("*** Error: pread from main file FD %d count %d offset %td, ret %d err %d = %s\n", mfd->mainfd, $$BL_S, (blockoffset << $$BL_SLOG), ret, waserror, strerror(waserror));
         break;
      }

      // Get the size of the dat file -- this is where we'll write
      if(unlikely((datsize = lseek(mfd->datfd, 0, SEEK_END)) == -1)) {
         waserror = errno;
         $dlogi("*** Error: lseek on dat for main file FD %d, err %d = %s\n", mfd->mainfd, waserror, strerror(waserror));
         break;
      }

      // Sanity check: the size of the dat file should be divisible by $$BL_S
      if(unlikely((datsize & ($$BL_S - 1)) != 0)) {
         $dlogi("*** Error: Size of dat file (%td) is not divisible by block size (%d = 2^%d) for main FD '%d', path '%s'; datfd '%d'.\n", datsize, $$BL_S, $$BL_SLOG, mfd->mainfd, mfd->vpath, mfd->datfd);
         waserror = EFAULT;
         break;
      }

      // First we try to append to the dat file
      ret = write(mfd->datfd, buf, $$BL_S);
      if(unlikely(ret != $$BL_S)) {
         waserror = (ret == -1 ? errno : ENXIO);
         $dlogi("*** Error: write into .dat for main file FD %d, ret %d err %d = %s\n", mfd->mainfd, ret, waserror, strerror(waserror));
         break;
      }

      pointer = (datsize >> $$BL_SLOG); // Get where we've written the block
      pointer++; // We save pointer+1 in the map

      ret = pwrite(mfd->mapfd, &pointer, $$BLP_S, mapoffset);
      if(unlikely(ret != $$BLP_S)) {
         waserror = (ret == -1 ? errno : ENXIO);
         $dlogi("*** Error: pwrite on .map for main file FD %d, ret %d err %d = %s\n", mfd->mainfd, ret, waserror, strerror(waserror));
         break;
      }

      $dlogdbg("b_write: wrote pointer %zu to fd %d offs %td for main fd %d\n", pointer, mfd->mapfd, mapoffset, mfd->mainfd);

   } // end for

   // Cleanup
   if(lock != -1) {
      $dlogdbg("b_write: Releasing lock %d for main file FD %d\n", lock, mfd->mainfd);
      if(unlikely((lock = $mflock_unlock(fsdata, lock)) < 0)) {
         $dlogi("*** Error: unlock for main file FD %d, err %d = %s\n", mfd->mainfd, lock, strerror(lock));
         return -lock;
      }

      if(buf != NULL) { free(buf); }
   }

   return -waserror; // this is 0 if waserror==0
}


/** Saves the truncated part of a file
 *
 * Returns:
 * * 0 - on success
 * * -errno - on failure
 */
static inline int $b_truncate(struct $fsdata_t *fsdata, struct $mfd_t *mfd, off_t newsize)
{
   int ret;

   // If the file existed and was larger than newsize, save the blocks
   // NB Any blocks outside the current main file should have already been saved
   if(mfd->mapheader.exists == 1 && newsize < mfd->mapheader.fstat.st_size) { // TODO check, but in all cases we should know that mfd->mapheader.exists == 1
      ret = $b_write(fsdata, mfd, mfd->mapheader.fstat.st_size - newsize, newsize);
      if(ret == 0) {
         return 0;
      }
      $dlogi("ERROR b_truncate: b_write on main fd %d failed with err %d = %s\n", mfd->mainfd, -ret, strerror(-ret));
      return ret;
   }
   return 0;
}
