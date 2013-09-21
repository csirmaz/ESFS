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

/* We implement a mutex-based locking mechanism here that allows
 * $$LOCK_NUM files to be written in parallel by different threads,
 * while only one thread may write a file at any time.
 *
 * We need this restriction to ensure that when a block is saved in the dat file,
 * only one thread is writing that file. This applies even if the dat file
 * is a journal storing possibly more changes to the same block, and
 * if it's opened with O_APPEND.
 *
 * We set up a limited number of locks here to save the overhead of a hash
 * storage (from files(e.g. inodes) to a mutex). Mutexes here are organised in a table
 * and can be labelled. We need to protect changes to the table
 * so that a label and a mutex would function as a unit, but instead of
 * locking the whole table whenever it changes, we use MOD mutexes
 * which only lock changes related to certain labels. There are $$LOCK_NUM
 * MOD mutexes, and MOD mutex i is used when label % $$LOCK_NUM == i.
 *
 * Moreover, to save the overhead or re-labelling mutexes, there is a system
 * where mutexes can be handed over to other threads if they are waiting
 * for the mutex on the same label. This is done using the 'want' member.
 *
 * Rules of thumb:
 * - Reverse procedures should be symmetrical
 * - Re-check expected condition after getting mutex
 *
 */

/* GET LOCK:
 *
 * START:
 * if(label is in table -> i){
 *    want i++
 *    get mutex i
 *    want i--
 *    recheck: if(label i != label){
 *       goto START
 *    }
 *    return i
 * }
 *
 * // label is not in table
 * get MOD mutex
 * recheck if(label is in table -> i){
 *    release MOD mutex
 *    want i++
 *    get mutex i
 *    want i--
 *    recheck: if(label i != label){
 *       goto START
 *    }
 *    return i
 * }
 *
 * FIND:
 * try all non-labelled mutexes in a loop until able to get one -> i
 * recheck: if(label no longer empty){
 *    // we must be in the middle of a handover
 *    release mutex i
 *    goto FIND: - OR - release MOD mutex; goto START:
 * }
 * label := new label
 * release MOD mutex
 * return i
 *
 * RELEASE LOCK:
 * if(want==0){
 *    get MOD mutex
 *    recheck if (want==0){
 *       label := 0
 *       release mutex
 *       release MOD mutex
 *       return
 *    }
 *    release MOD mutex
 * }
 * release mutex
 * return
 *
 *
 * Consequences:
 * - an label can only be added to / removed from the table if its MOD mutex is held,
 *    AND the mutex being labelled is held
 *
 * A labels the mutex and hands it over to B:
 *
 * MOD   -----AAAAAAAAAAAA--|--------------|--BBBBBBBBBB-----
 * LABEL ------------+++++++|++++++++++++++|+++++------------
 * MUTEX ---------AAAAAAAAAA|AAAAAAAA--BBBB|BBBBBBBB---------
 * WANT  -------------------|---111111111--|-----------------
 *
 * Handover fails because want is set too late by B. B starts over.
 *
 * MOD   -----AAAAAAAAAAAA--|-B--AAAAAAA-----
 * LABEL ------------+++++++|++++++----------
 * MUTEX ---------AAAAAAAAAA|AAAAAAAA--------
 * WANT  -------------------|----------111---
 *
 * A and B try to add the same label, but A gets the MOD mutex first,
 * so it becomes a handover:
 *
 * MOD    --AAAAAAAAAA--BB-----------
 * LABEL1 -------++++++++++++++++++++
 * MUTEX1 -----AAAAAAAAAAAAAAA-BBBBBB
 * WANT1  ------------------11111----
 *
 *
 */

/** Allocates memory and initialises the mutexes.
 *
 * Returns
 * * 0 on success
 * * -errno on error
 */
static int $mflock_init(struct $fsdata_t *fsdata)
{
   int i;
   pthread_mutexattr_t mutexattr;

   pthread_mutexattr_init(&mutexattr);
   pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK_NP);
   // TODO Later switch to the default by using NULL instead of mutexattr below

   fsdata->mflocks = malloc(sizeof(struct $mflock_t) * $$LOCK_NUM);
   if(fsdata->mflocks == NULL) { return -ENOMEM; }

   for(i = 0; i < $$LOCK_NUM; i++) {
      pthread_mutex_init(&(fsdata->mflocks[i].mutex), &mutexattr);
      pthread_mutex_init(&(fsdata->mflocks[i].mod_mutex), &mutexattr);
      fsdata->mflocks[i].label = 0;
      fsdata->mflocks[i].want = 0;
   }

   pthread_mutexattr_destroy(&mutexattr);
   return 0;
}


static int $mflock_destroy(struct $fsdata_t *fsdata)
{
   int i;

   for(i = 0; i < $$LOCK_NUM; i++) {
      pthread_mutex_destroy(&(fsdata->mflocks[i].mutex));
      pthread_mutex_destroy(&(fsdata->mflocks[i].mod_mutex));
   }
   free(fsdata->mflocks);
   return 0;
}


/** Gets a lock on a particular file
 *
 * Returns:
 * * lock number on success (>=0)
 * * -errno on error
 */
static int $mflock_lock(struct $fsdata_t *fsdata, $$LOCKLABEL_T label)
{
   int i;
   int ret;
   int ml = -1;
   struct $mflock_t *mylock;
   pthread_mutex_t *modmutex = NULL;
   struct timespec delay = { 0, 10000000 }; // nanoseconds: 1 000 000 000

   while(1) { // START:

      mylock = NULL;
      for(i = 0; i < $$LOCK_NUM; i++) {
         if(fsdata->mflocks[i].label == label) {
            ml = i;
            mylock = &(fsdata->mflocks[i]);
            break;
         }
      }

      if(mylock != NULL) { // if label is in the table
         if(unlikely(modmutex != NULL)) {
            // recheck failed; release the modmutex and carry on as usual
            pthread_mutex_unlock(modmutex);
            modmutex = NULL;
         }
         (mylock->want)++; // request handover
         pthread_mutex_lock(&(mylock->mutex));
         (mylock->want)--;
         if(likely(mylock->label == label)) { // recheck
            // We have (successfully taken over) the mutex and it's labelled with the label
            return ml;
         }
         // If recheck fails, retry from start
         $$SLEEP
         continue;
      }

      // label is not in the table:
      // get the modmutes and re-check that the label is still not in the table

      if(modmutex == NULL) { // If we don't yet have the modmutex
         modmutex = &(fsdata->mflocks[label & ($$LOCK_NUM - 1)].mod_mutex);
         if(unlikely((ret = pthread_mutex_lock(modmutex)) != 0)) { return -ret; }
         continue; // Start over for a re-check.
      }

      // We have the modmutex and label is not in the table
      // try all non-labelled locks in the table
      while(1) { // FIND:
         ml = -1;
         for(i = 0; i < $$LOCK_NUM; i++) {
            if(likely(fsdata->mflocks[i].label == 0)) {
               ret = pthread_mutex_trylock(&(fsdata->mflocks[i].mutex));
               if(likely(ret == 0)) { // got the lock
                  ml = i;
                  break;
               } else if(ret == EBUSY) { // lock is busy
                  continue;
               } else { // error
                  return -ret;
               }
            }
         }

         if(ml > -1) { // managed to get a mutex
            // recheck if the mutex is still unlabelled
            if(likely(fsdata->mflocks[ml].label == 0)) {
               // Success
               fsdata->mflocks[ml].label = label;
               if(unlikely((ret = pthread_mutex_unlock(modmutex)) != 0)) { return -ret; }
               return ml;
            }
            // if recheck fails, release the mutex and continue from FIND
            // (this must be the middle of a handover)
            if(unlikely((ret = pthread_mutex_unlock(&(fsdata->mflocks[i].mutex))) != 0)) { return -ret; }
         }

         $$SLEEP

      } // end FIND

   } // end START

   return -EIO; // unreachable
}


/** Release a lock
 *
 * Returns:
 * * 0 on success
 * * -errno on error
 */
static int $mflock_unlock(struct $fsdata_t *fsdata, int lockid)
{
   int ret;
   struct $mflock_t *mylock;
   pthread_mutex_t *modmutex;

   mylock = &(fsdata->mflocks[lockid]);

   if(mylock->want == 0) { // no one wants a handover
      modmutex = &(fsdata->mflocks[mylock->label & ($$LOCK_NUM - 1)].mod_mutex);
      if(unlikely((ret = pthread_mutex_lock(modmutex)) != 0)) { return -ret; }
      if(mylock->want == 0) { // recheck
         mylock->label = 0;
         if(unlikely((ret = pthread_mutex_unlock(&(mylock->mutex))) != 0)) { return -ret; }
         if(unlikely((ret = pthread_mutex_unlock(modmutex)) != 0)) { return -ret; }
         return 0;
      }
      // oops - they want a handover
      if(unlikely((ret = pthread_mutex_unlock(modmutex)) != 0)) { return -ret; }
   }
   // handover - release lock without removing label
   if(unlikely((ret = pthread_mutex_unlock(&(mylock->mutex))) != 0)) { return -ret; }
   return 0;
}
