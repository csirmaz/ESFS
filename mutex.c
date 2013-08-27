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
 * The label and the mutex need to be a unit, so we protect
 * changing them with the MOD mutexes.
 *
 * Rules:
 * - Reverse procedures should be symmetrical
 * - Re-check expected condition after getting mutex
 * 
 * GET LOCK:
 *
 * START:
 * if(inode is in table -> i){
 *    want i++
 *    get mutex i
 *    want i--
 *    recheck: if(label i != inode){
 *       goto START
 *    }
 *    return i
 * }
 *
 * // inode is not in table
 * get MOD mutex
 * recheck if(inode is in table -> i){
 *    release MOD mutex
 *    want i++
 *    get mutex i
 *    want i--
 *    recheck: if(label i != inode){
 *       goto START
 *    }
 *    return i
 * }
 *
 * FIND:
 * find mutex that looks free and unlabelled -> i TODO RANDOMISE
 * get mutex i
 * recheck: if(label no longer empty){
 *    release mutex i
 *    goto FIND: - OR - release MOD mutex; goto START:
 * }
 * label := inode
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
 * - an inode can only be added to the table if its MOD mutex is held,
 *    AND the mutex being labelled is held
 *
 * A labels the mutex and B takes over:
 * 
 * MOD   -----AAAAAAAAAAAA--|--------------|--BBBBBBBBBB-----
 * LABEL ------------+++++++|++++++++++++++|+++++------------
 * MUTEX ---------AAAAAAAAAA|AAAAAAAA--BBBB|BBBBBBBB---------
 * WANT  -------------------|---111111111--|-----------------
 *
 * Handover fails because want is set too late:
 *
 * MOD   -----AAAAAAAAAAAA--|-B--AAAAAAA-----
 * LABEL ------------+++++++|++++++----------
 * MUTEX ---------AAAAAAAAAA|AAAAAAAA--------
 * WANT  -------------------|----------111---
 * 
 *
 * A and B try to add the same inode, but A gets the MOD mutex first,
 * so it becomes a handover:
 *
 * MOD    --AAAAAAAAAA--BB-----------
 * LABEL1 -------++++++++++++++++++++
 * MUTEX1 -----AAAAAAAAAAAAAAA-BBBBBB
 * WANT1  ------------------11111----
 *
 * Inserting is delayed because the mutex is taken over by C
 * and handed over to B:
 * 
 * MOD1  -----AAAAAAAAAAAAAA|AAAAAAAAAA|AAAAAAAAAAAAAAA----
 * MOD2  -----CCCCCCCCCCCC--|----------|---BBBBBBBbbb------
 * LABEL ------------jjjjjjj|jjjjjjjjjj|jjjjjj-----iiiiiiii
 * MUTEX ---------CCCCCCCCCC|CCC--BBBBB|BBBBBBBB-AAAAAAAAAA
 * WANT  -----------------11|1111111---|-------------------
 *
 * Inserting goes wrong because the mutex is taken over, and
 * then is waited for when A gets it. A tries to find another mutex.
 *
 * MOD1  -----AAAAAAAAAAAAAA|AAAAAAAAAAAAAAAAA
 * MOD2  -----CCCCCCCCCCCC--|-----------------
 * LABEL ------------jjjjjjj|jjjjjjjjjjjjjjjjj
 * MUTEX ---------CCCCCCCCCC|CCC--AAAAA--BBBBB
 * WANT  -----------------11|11111111111111---

 *
 */

/* Allocate memory and initialise the mutexes.
 * Returns
 * 0 on success
 * -errno on error
 */
static int $flock_init(struct $fsdata_t *fsdata){
   int i;
   pthread_mutexattr_t mutexattr;

   pthread_mutexattr_init(&mutexattr);
   pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK_NP);
   // TODO Later switch to the default by using NULL instead of mutexattr below

   fsdata->mflocks = malloc(sizeof($flock_t) * $$LOCK_NUM);
   if(fsdata->mflocks == NULL){ return -ENOMEM; }

   for(i=0; i<$$LOCK_NUM; i++){
      pthread_mutex_init(fsdata->mflocks[i].mutex, &mutexattr);
      fsdata->mflocks[i].inode = 0;
   }

   pthread_mutexattr_destroy(&mutexattr);
   return 0;
}


static int $flock_destroy(struct $fsdata_t *fsdata){
   free(fsdata->mflocks);
   return 0;
}


/* Returns:
 * 0 on success
 * -errno on error
 */
static int $flock_lock(struct $fsdata_t *fsdata, ino_t inode){
   int i;
   int ret;
   timespec delay = { 0, 10000000 }; // nanoseconds: 1 000 000 000

   do{

      // First we check that the inode is not locked already
      ret = 0;
      for(i=0; i<$$LOCK_NUM; i++){
         if(fsdata->flocks[i].inode == inode){
            ret = 1;
            // Sleep
            pselect(0, NULL, NULL, NULL, &delay, NULL);
            break;
         }
      }

      if(ret==1){ continue; }

      // If not, we try to acquire any of the locks

      for(i=0; i<$$LOCK_NUM; i++){

         ret = pthread_mutex_trylock(fsdata->mflocks[i].mutex);

         if(ret == 0){ // the lock is free, so try to get it

               // Set the inode *before* getting the mutex, for additional safety
               fsdata->mflocks[i].inode = inode;
               if((ret = pthread_mutex_lock(fsdata->mflocks[i].mutex)) != 0){ // this blocks
                  fsdata->mflocks[i].inode = 0;
                  return -ret; // error
               }
               // We've got the mutex
               fsdata->mflocks[i].inode = inode; // set the inode again -- now no one else should set this
               return 0;
         }

         if(ret == EBUSY){ // the lock is busy; carry on
            continue;
         }

         // Error
         return -ret;
      }

      // Sleep
      pselect(0, NULL, NULL, NULL, &delay, NULL);

   }while(1);

}

