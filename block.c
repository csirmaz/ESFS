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
 * /

/* Checks a block and saves it if necessary.
 *   blockno is the number of the block, not an offset.
 * Returns:
 * 0 - on success
 * -errno - on failure
 */
static inline int $b_check_save_block(const struct $fsdata_t *fsdata, struct $fd_t *mfd, $$BLP_T blockno){
   $$BLP_T pointer;
   off_t mapoffset;
   int waserror;
   char *buf;

   $dlogdbg("Saving block no %ld from main FD %d\n", blockno, mfd->mainfd);

   mapoffset = (sizeof(struct $mapheader_t)) + blockno * $$BLP_S;

   // Read the pointer from the map file
   // For this to work, we need to make sure that the underlying FS is POSIX conforming,
   // and that any write has returned on this file.
   // To achieve this, we use mutexes.


   
   // Read the pointer from the map file
   if(pread(mfd->mapfd, &pointer, $$BLP_S, mapoffset) != 0){
      waserror = errno;
      $dlogdbg("Error: pread on .map for main file FD %d, map FD %d, err %d = %s\n", mfd->mainfd, mfd->mapfd, waserror, strerror(waserror));
      return -waserror;
   }

   if(pointer != 0){ return 0; } // We don't need to save

   // We need to save the block
   // TODO implement a list of buffers various threads can lock and use
   buf = malloc($$BL_S);
   if(buf == NULL){ return -ENOMEM; }

   do{

// TODO LOCKING!!!! - manual or use mutexes

      if(pread(mfd->mainfd, buf, $$BL_S, (blockno << $$BL_SLOG)) != 0){
         waserror = errno;
         $dlogdbg("Error: pread on main file FD %d, err %d = %s\n", mfd->mainfd, waserror, strerror(waserror));
         break;
      }

      // Sanity check: the size of the dat file should be divisible by $$BL_S
      // We could use a mask here
**      if( ((mfd->dat_size >> $$BL_SLOG) << $$BL_SLOG) != mfd->dat_size ){
         $dlogi("Error: Size of dat file is not divisible by block size (%d = 2^%d) for main FD %d.\n", $$BL_S, $$BL_SLOG, mfd->mainfd);
         waserror = EFAULT;
         break;
      }

      // First we try to append to the dat file
**      if(pwrite(mfd->datfd, buf, $$BL_S, mfd->dat_size) != 0){
         waserror = errno;
         $dlogdbg("Error: pwrite on .dat for main file FD %d, err %d = %s\n", mfd->mainfd, waserror, strerror(waserror));
         break;
      }
**      pointer = (mfd->dat_size >> $$BL_SLOG); // Get where we've written the block
**      mfd->dat_size += $$BL_S; // Update the cached length

      pointer++; // We save pointer+1 in the map
      if(pwrite(mfd->mapfd, &pointer, $$BLP_S, mapoffset) != 0){
         waserror = errno;
         $dlogdbg("Error: pwrite on .map for main file FD %d, err %d = %s\n", mfd->mainfd, waserror, strerror(waserror));
         // TODO If we fail here, why not reuse the block in the dat file:
         break;
      }
   }while(0);

   free(buf);

   if(waserror != 0){
      return -waserror;
   }
   return 0;
}


/* Called when a write operation is attempted
 * Returns the same things as $b_check_save_block.
 */
static inline int $b_write(
   const struct $fsdata_t *fsdata,
   struct $fd_t *mfd,
   size_t size,
   off_t offset
)
{
   off_t bloffs; // starting number of blocks written
   size_t blockno; // number of blocks written
   int ret;

   // Nothing to do if the main file is read-only or there are no snapshots or the file was empty in the snapshot
   if(mfd->datfd < 0){ return 0; }

   $dlogdbg("b_write: offset %lld size %d\n", (long long int)offset, (int)size);

   if(size <= 0){ return 0; } // Nothing to write

   // TODO Don't save blocks outside original length of main file

   bloffs = (offset >> $$BL_SLOG);
   blockno = (size >> $$BL_SLOG) + 1;

   $dlogdbg("b_write: bloffs %lld blockno %d\n", (long long int)bloffs, (int)blockno);

   while(blockno > 0){
      if((ret = $b_check_save_block(fsdata, mfd, bloffs)) != 0){ return ret; }
      blockno--;
      bloffs++;
   }
   return 0;
}
