// -*- C++ -*-

/*
 Author: Emery Berger, http://www.cs.umass.edu/~emery
 
 Copyright (c) 2007-8 Emery Berger, University of Massachusetts Amherst.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 */

/*
 * @file   xrun.h
 * @brief  The main engine for consistency management, etc.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 * @author Charlie Curtsinger <http://www.cs.umass.edu/~charlie>
 */

#ifndef _XRUN_H_
#define _XRUN_H_

// Common defines
#include "xdefines.h"

// threads
#include "xthread.h"

// memory
#include "xmemory.h"

// Heap Layers
#include "heaplayers/util/sassert.h"

#include "xatomic.h"

// determinstic controls
#include "determ.h"

#include "xbitmap.h"

#include "prof.h"

#include "debug.h"

#include "time_util.h"

#include "stats.h"

class xrun {

private:
  static volatile bool _initialized;
  static volatile bool _protection_enabled;
  static size_t _master_thread_id;
  static size_t _thread_index;
  static bool _fence_enabled;
  static size_t _children_threads_count;
  static size_t _lock_count;
  static bool _token_holding;
  static struct timespec fence_start;
  static struct timespec fence_end;
  static int fence_total_us;
  static struct timespec token_start;
  static struct timespec token_end;
  static int token_total_us;
  static commit_stats * token_stats;
  static commit_stats * fence_stats;
  static commit_stats * barrier_stats;
  static commit_stats * mutex_stats;
  static commit_stats * atomic_begin_stats;
  static commit_stats * atomic_end_stats;
  static commit_stats * getlock_count;
  static int free_count;
  static int commit_count;
  static struct timespec ts1;
  static struct timespec ts2;
  


#ifdef TIME_CHECKING
  static struct timeinfo tstart;
#endif

public:

  /// @brief Initialize the system.
  static void initialize(void) {
    DEBUG("initializing xrun");

    _initialized = false;
    _protection_enabled = false;
    _children_threads_count = 0;
    _lock_count = 0;
    _token_holding = false;
    fence_total_us = 0;
    token_total_us = 0;
    free_count = 0;
    commit_count=0;

    token_stats = new commit_stats();
    fence_stats = new commit_stats();
    barrier_stats = new commit_stats();
    mutex_stats = new commit_stats();
    atomic_begin_stats = new commit_stats();
    atomic_end_stats = new commit_stats();
    getlock_count = new commit_stats();

    pid_t pid = syscall(SYS_getpid);

    if (!_initialized) {
      _initialized = true;

      // xmemory::initialize should happen before others
      xmemory::initialize();

      xthread::setId(pid);
      _master_thread_id = pid;
      xmemory::setThreadIndex(0);

      determ::getInstance().initialize();
      xbitmap::getInstance().initialize();

      _thread_index = 0;

      // Add myself to the token queue.
      determ::getInstance().registerMaster(_thread_index, pid);
      _fence_enabled = false;
    } else {
      fprintf(stderr, "xrun reinitialized");
      ::abort();
    }
    clock_gettime(CLOCK_REALTIME, &ts1);
  }

  static void done(void){
    determ::getInstance().finalize(token_stats->stats_pid_aggregate());
  }

  // Control whether we will protect memory or not.
  // When there is only one thread in the system, memory is not 
  // protected to avoid the memory protection overhead.
  static void openMemoryProtection(void) {
    if (_protection_enabled)
      return;

    xmemory::openProtection();
    _protection_enabled = true;
  }

  // Close memory protection when there is only one thread alive.
  static void closeMemoryProtection(void) {
    xmemory::closeProtection();
    _protection_enabled = false;
  }

  static void finalize(void) {
    xmemory::finalize();
    clock_gettime(CLOCK_REALTIME, &ts2);
    //ignore the first thread
    if (_thread_index > 0){
      determ::getInstance().add_total_time(time_util_time_diff(&ts1,&ts2));
    }
  }

  // @ Return the main thread's id.
  static inline bool isMaster(void) {
    return getpid() == _master_thread_id;
  }

  // @return the "thread" id.
  static inline int id(void) {
    return xthread::getId();
  }

  // New created thread should call this.
  // Now only the current thread is active.
  static inline int childRegister(int pid, int parentindex) {
    int threads;
    struct timespec t1,t2;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    // Get the global thread index for this thread, which will be used internally.
    _thread_index = xatomic::increment_and_return(&global_data->thread_index);
    _lock_count = 0;
    _token_holding = false;

    // For child, fence is always enabled in the beginning.
    // Wait on token to do synchronizations if we set this.
    _fence_enabled = true;

    determ::getInstance().registerThread(_thread_index, pid, parentindex);

    // Set correponding heap index.
    xmemory::setThreadIndex(_thread_index);

  #ifdef LAZY_COMMIT
    // New thread will not own any blocks in the beginning
    // We should cleanup all blocks information inherited from the parent.
    xmemory::cleanupOwnedBlocks();
  #endif
    clock_gettime(CLOCK_MONOTONIC, &t2);
    //cout << "childRegister: " << time_util_time_diff(&t1,&t2) << endl;

    return (_thread_index);
  }

  // New created threads are waiting until notify by main thread.
  static void waitParentNotify(void) {
    determ::getInstance().waitParentNotify(_thread_index);
  }

  static inline void waitChildRegistered(void) {
    determ::getInstance().waitChildRegistered();
  }

  static inline void threadDeregister(void) {
    waitToken();

#ifdef LAZY_COMMIT 
    xmemory::finalcommit(false);
#endif
    //determ::getInstance().print_total_commit_time();
    //DEBUG("%d: thread %d deregister, get token\n", getpid(), _thread_index);
    atomicEnd(false);
    // Remove current thread and decrease the fence
    //cout << "fence " << fence_total_us << " token " << token_total_us << endl;
    //cout << "thread wait pid " << getpid() << " " << fence_stats->stats_get_pid_time() + token_stats->stats_get_pid_time() << endl;
    determ::getInstance().deregisterThread(_thread_index);
  }

  static inline void closeFence(void) {
    _fence_enabled = false;
    _children_threads_count = 0;

    // Reclaiming the thread index, new threads can share the same heap with
    // previous exiting threads. Thus we could improve the locality.
    global_data->thread_index = 1;
  }

#ifdef LAZY_COMMIT 
  static inline void forceThreadCommit(void * v) {
    int pid;
    pid = xthread::getThreadPid(v);
    xmemory::forceCommit(pid);
  }
#endif

  /// @return the unique thread index.
  static inline int threadindex(void) {
    return _thread_index;
  }

  /// @brief Spawn a thread.
  static inline void * spawn(threadFunction * fn, void * arg) {
    // If system is not protected, we should open protection.
    if(!_protection_enabled) {
      openMemoryProtection();
      atomicBegin(true);
    }
 
    atomicEnd(false);

#ifdef LAZY_COMMIT 
    xmemory::finalcommit(true);
#endif
      
    // If fence is already enabled, then we should wait for token to proceed.
    if(_fence_enabled) {  
      waitToken();

      // In order to speedup the performance, we try to create as many children
      // as possible once. So we set the _fence_enabled to false now, then current
      // thread don't need to wait on token anymore.
      // Since other threads are either waiting on internal fence or waiting on the parent notification, 
      // it will be fine to do so.
      // When current thread are trying to wakeup the children threads, it will set 
      // _fence_enabled to true again.
      _fence_enabled = false;
      _children_threads_count = 0;
    }

    _children_threads_count++;
    void * ptr = xthread::spawn(fn, arg, _thread_index);

    // Start a new transaction
    atomicBegin(true);

    return ptr;
  }

  /// @brief Wait for a thread.
  static inline void join(void * v, void ** result) {
    int  child_threadindex = 0;
    bool wakeupChildren = false;

    // Return immediately if the thread argument is NULL.
    if (v == NULL) {
      fprintf(stderr, "%d: join with invalid parameter\n", getpid());
      return;
    }

    // Wait on token if the fence is already started.
    // It is important to maitain the determinism by waiting. 
    // No need to wait when fence is not started since join is the first
    // synchronization after spawning, other thread should wait for 
    // the notification from me.
    if(_fence_enabled) {
      waitToken();
    }
    
    atomicEnd(false);
#ifdef LAZY_COMMIT 
    xmemory::finalcommit(true);
#endif

    if(!_fence_enabled) {
      startFence();
      wakeupChildren = true;
    }
    // Get the joinee's thread index.
    child_threadindex = xthread::getThreadIndex(v);


    // When child is not finished, current thread should wait on cond var until child is exited.
    // It is possible that children has been exited, then it will make sure this.
    determ::getInstance().join(child_threadindex, _thread_index, wakeupChildren);
    
    // Release the token.
    putToken();
    
    // Cleanup some status about the joinee.  
    xthread::join(v, result);
    
    // Now we should wait on fence in order to proceed.
    waitFence();
    
    // Start next transaction.
    atomicBegin(true);
  
    // Check whether we can close protection at all.
    // If current thread is the only alive thread, then close the protection.
    if(determ::getInstance().isSingleAliveThread()) {
      closeMemoryProtection();
            
      // Do some cleanup for fence. 
      closeFence();
    }
  }

  /// @brief Do a pthread_cancel
  static inline void cancel(void *v) {
    int threadindex;
    bool isFound = false;

    // If I am not holding the token, wait on token to guarantee determinism.
    if (!_token_holding) {
      waitToken();
    }

    atomicEnd(false);

#ifdef LAZY_COMMIT
    // When the thread to be cancel is still there, we are forcing that thread
    // to commit every owned page if we are using lazy commit mechanism.
    // It is important to call this function before xthread::cancel since
    // threadindex or threadpid information will be destroyed xthread::cancel.
    if(isFound) {
      forceThreadCommit(v);   
    }
#endif
    atomicBegin(true);
    threadindex = xthread::cancel(v);
    isFound = determ::getInstance().cancel(threadindex);
    

    // Put token and wait on fence if I waitToken before.
    if (!_token_holding) {
      putToken();
      waitFence();
    }
  }

  inline void kill(void *v, int sig) {
    int threadindex;
  
    if(sig == SIGKILL || sig == SIGTERM) {
      cancel(v);
    }

    // If I am not holding the token, wait on token to guarantee determinism.
    if (!_token_holding) {
      waitToken();
    }

    atomicEnd(false);
    threadindex = xthread::thread_kill(v, sig);

    atomicBegin(true);

    // Put token and wait on fence if I waitToken before.
    if (!_token_holding) {
      putToken();
      waitFence();
    }
  }

  /* Heap-related functions. */
  static inline void * malloc(size_t sz) {
    char * tmp = NULL;
    //cout << "asking malloc for " << sz << " pid " << getpid() << endl;
    void * ptr = xmemory::malloc(sz);
    //if (ptr){
      //tmp=(char *)(*((unsigned long *)ptr));
    //}
    //if (ptr){
      //cout << "malloc size: " << sz << " result " << ptr << " pid " << getpid() << " next " << *((unsigned long *)ptr) << endl;
    //}
    //fprintf(stderr, "%d : malloc sz %d with ptr %p\n", _thread_index, sz, ptr);
    return ptr;
  }

  static inline void * calloc(size_t nmemb, size_t sz) {
    void * ptr = xmemory::malloc(nmemb * sz);
    memset(ptr, 0, nmemb * sz);
    return ptr;
  }

  // In fact, we can delay to open its information about heap.
  static inline void free(void * ptr) {
    //cout << "freeing " << ptr << " count " << free_count << endl;
    free_count++;
    xmemory::free(ptr);
  }

  static inline size_t getSize(void * ptr) {
    return xmemory::getSize(ptr);
  }

  static inline void * realloc(void * ptr, size_t sz) {
    void * newptr;
    //fprintf(stderr, "realloc ptr %p sz %x\n", ptr, sz);
    if (ptr == NULL) {
      newptr = xmemory::malloc(sz);
      return newptr;
    }
    if (sz == 0) {
      xmemory::free(ptr);
      return NULL;
    }

    newptr = xmemory::realloc(ptr, sz);
    //fprintf(stderr, "realloc ptr %p sz %x\n", newptr, sz);
    return newptr;
  }

  ///// conditional variable functions.
  static void cond_init(void * cond) {
    determ::getInstance().cond_init(cond);
  }

  static void cond_destroy(void * cond) {
    determ::getInstance().cond_destroy(cond);
  }

  // Barrier support
  static int barrier_init(pthread_barrier_t *barrier, unsigned int count) {
    //printf("BARRIERI_init with count %d\n", count);
    determ::getInstance().barrier_init(barrier, count);
    return 0;
  }

  static int barrier_destroy(pthread_barrier_t *barrier) {
    determ::getInstance().barrier_destroy(barrier);
    return 0;
  }

  ///// mutex functions
  /// FIXME: maybe it is better to save those actual mutex address in original mutex.
  static int mutex_init(pthread_mutex_t * mutex) {
    determ::getInstance().lock_init((void *)mutex);
    return 0;
  }

  static void startFence(void) {
    assert(_fence_enabled != true);

    // We start fence only if we are have more than two processes.
    //assert(_children_threads_count != 0);

    if (_children_threads_count>0){
    // Start fence.   
    determ::getInstance().startFence(_children_threads_count);

    _children_threads_count = 0;

    _fence_enabled = true;
    }
  }

  static void waitFence(void) {
    determ::getInstance().waitFence(_thread_index, false);
  }

  // New optimization here.
  // We will add one parallel commit phase before one can get token.
  static int waitToken(void) {
    struct timespec t1,t2;
    int spin_counter=0;
    fence_stats->stats_pid_start();
    //determ::getInstance().waitFence(_thread_index, true);
    fence_stats->stats_pid_end();
    clock_gettime(CLOCK_REALTIME, &t1);
    //cout << "pid " << getpid() << " wait start s: " << t1.tv_sec << " ns: " << t1.tv_nsec <<  endl;
    token_stats->stats_pid_start();
    spin_counter=determ::getInstance().getToken(_thread_index);
    token_stats->stats_pid_end();
    clock_gettime(CLOCK_REALTIME, &t2);
    //cout << "pid " << getpid() << " wait done s: " << t2.tv_sec << " ns: " 
    //	 << t2.tv_nsec << " diff: " << time_util_time_diff(&t1,&t2) << endl;
    determ::getInstance().add_total_wait_time(time_util_time_diff(&t1,&t2));
    return spin_counter;
  }

  // If those threads sending out condsignal or condbroadcast,
  // we will use condvar here.
  static void putToken(void) {
    // release the token and pass the token to next.
    //fprintf(stderr, "%d: putToken\n", _thread_index);
    determ::getInstance().putToken(_thread_index);
  //  fprintf(stderr, "%d: putToken\n", getpid());
  }

  // FIXME: if we are trying to remove atomicEnd() before mutex_lock(),
  // we should unlock() this lock if abort(), otherwise, it will
  // cause the dead-lock().
  static void mutex_lock(pthread_mutex_t * mutex) {

    if (!_fence_enabled) {
      if(_children_threads_count == 0) {
        return;
      }
      else {
        startFence();

        // Waking up all waiting children
        determ::getInstance().notifyWaitingChildren();
      }
    }

    // Calculate how many locks are acquired under the token.
    // Since we treat multiple locks as one lock, we only start 
    // the transaction in the beginning and close the transaction
    // when lock_count equals to 0. 
    _lock_count++;
    int last_thread=888;
    int total_users=999;

    if(determ::getInstance().lock_isowner(mutex, &last_thread, &total_users) || determ::getInstance().isSingleWorkingThread()) {
      // Then there is no need to acquire the lock.
      bool result = determ::getInstance().lock_acquire(mutex);
      if(result == false) {   
        goto getLockAgain;
      }
      return; 
    }
    else {
getLockAgain:
      getlock_count->stats_pid_inc();
      // If we are not holding the token, trying to get the token in the beginning.
      if(!_token_holding) {
        waitToken();
	mutex_stats->stats_pid_start();
        _token_holding = true;
	atomicEnd(false);
  //      atomicEnd(true);
        atomicBegin(true);
      }

    //  fprintf(stderr, "%d: mutex_lock holding the token\n", getpid());
      
      // We are trying to get current lock.
      // Whenver someone didn't release the lock, getLock should be false.
      bool getLock = determ::getInstance().lock_acquire(mutex);   
      
  //  fprintf(stderr, "%d: mutex_lock 4 with getlock %d\n", getpid(), getLock);
      if(getLock == false) {
        // If we can't get lock, let other threads to move on first
        // in order to maintain the semantics of pthreads.
        // Current thread simply pass the token and wait for
        // next run.
        //atomicEnd(true);
	atomicEnd(false);
	cout << "pid " << getpid() << "no lock " << endl;
        putToken();
        waitFence();
        atomicBegin(true);
        _token_holding = false;
        goto getLockAgain; 
      }

    }
  }

  static void mutex_unlock(pthread_mutex_t * mutex) {

    if (!_fence_enabled)
      return;

    

    // Decrement the lock account 
    _lock_count--;
    
    //barrier_stats->stats_pid_register();
    // Unlock current lock.
    determ::getInstance().lock_release(mutex);

      // Since multiple lock are considering as one big lock, 
    // we only do transaction end operations when no one is holding the lock.
    // However, when lock is owned, there is no need to close the transaction.
    // But for another case, there is only one thread and not any more(by sending out singal).
    //if(_lock_count == 0 && _token_holding && !determ::getInstance().isSingleWorkingThread())
    if(_lock_count == 0 && _token_holding)
    {
      mutex_stats->stats_pid_end();
      atomicEnd(false);
      
      //cout << getpid() << " released lock " << endl;
      putToken();
      _token_holding = false;
      
      waitFence();
      atomicBegin(true);
      //cout << getpid() << " release done " << endl;
    }
  }

  static int mutex_destroy(pthread_mutex_t * mutex) {
    determ::getInstance().lock_destroy(mutex);
    return 0;
  }

  // Add the barrier support.
  static int barrier_wait(pthread_barrier_t *barrier) {
    struct timespec t1,t2,t3,t4;
    int spin_counter=0;
    int barrier_debug=0;
    if (!_fence_enabled)
      return 0;
    clock_gettime(CLOCK_REALTIME,&t1);
    spin_counter=waitToken();
    clock_gettime(CLOCK_REALTIME,&t2);

    if (((unsigned long)barrier) & 1){
      barrier_debug=1;
      printf("found one\n");
    }
    //merge first (no updates)
    clock_gettime(CLOCK_REALTIME,&t1);
    xmemory::merge_for_barrier();
    xmemory::settle_for_barrier();
    clock_gettime(CLOCK_REALTIME,&t2);
    determ::getInstance().add_total_commit_time(time_util_time_diff(&t1,&t2));

    //atomicEnd(false);
    //xmemory::barrier_commit();

    barrier_stats->stats_pid_start();
    clock_gettime(CLOCK_REALTIME,&t3);
    determ::getInstance().barrier_wait(barrier, _thread_index);
    clock_gettime(CLOCK_REALTIME,&t4);
    barrier_stats->stats_pid_end();

    if (barrier_debug){
      printf("pid %d token: %lu, spin: %d, barrier %lu arrive s: %lu ns: %lu:\n token s: %lu ns: %lu\n barrier1 s: %lu ns: %lu\n barrier2: s: %lu ns: %lu\n\n", 
	     getpid(),
	     time_util_time_diff(&t1,&t2), spin_counter, time_util_time_diff(&t3,&t4), 
	     t1.tv_sec, t1.tv_nsec, 
	     t2.tv_sec, t2.tv_nsec, 
	     t3.tv_sec, t3.tv_nsec, 
	     t4.tv_sec, t4.tv_nsec);
    }

    return 0;
  }
  
  // Support for sigwait() functions in order to avoid deadlock.
  static int sig_wait(const sigset_t *set, int *sig) {
    int ret;
    waitToken();
    atomicEnd(false);
    ret = determ::getInstance().sig_wait(set, sig, _thread_index);
    if(ret == 0) {
      atomicBegin(true);
    }

    return ret;
  }

  static void cond_wait(void * cond, void * lock) {
    // corresponding lock should be acquired before.
    assert(_token_holding == true);
    //assert(determ::getInstance().lock_isacquired() == true);
    atomicEnd(false);
    // We have to release token in cond_wait, otherwise
    // it can cause deadlock!!! Some other threads
    // waiting for the token be no progress at all.
    determ::getInstance().cond_wait(_thread_index, cond, lock);
    atomicBegin(true);
  }
  

  static void cond_broadcast(void * cond) {
    if (!_fence_enabled)
      return;

    // If broadcast is sent out under the lock, no need to get token.
    if(!_token_holding) {
      waitToken();
    }

    atomicEnd(false);
    determ::getInstance().cond_broadcast(cond, _thread_index);
    atomicBegin(true);
    
    if(!_token_holding) {
      putToken();
      waitFence();
    }
  }

  static void cond_signal(void * cond) {
    if (!_fence_enabled)
      return;
    
    if(!_token_holding) {
      waitToken();
    }
      
    atomicEnd(false);
    //fprintf(stderr, "%d: cond_signal\n", getpid());
    determ::getInstance().cond_signal(cond, _thread_index);
    atomicBegin(true);
      
    if(!_token_holding) {
      putToken();
      waitFence();
    }
  }

  /// @brief Start a transaction.
  static void atomicBegin(bool cleanup) {
    fflush(stdout);

    if (!_protection_enabled)
      return;

    // Now start.
    xmemory::begin(cleanup);

    //cout << "atomic begin " << getpid() << endl;
    //xmemory::printCurrentDirty();
  }

  /*  static void atomicEndProfile(bool updateOnly){
    struct timespec t1,t2;
    fflush(stdout);
    if (!_protection_enabled)
      return;
    ++commit_count;
    clock_gettime(CLOCK_REALTIME,&t1);
    xmemory::commit(PERSIST_COMMIT_NORMAL, !updateOnly);
    clock_gettime(CLOCK_REALTIME,&t2);
    cout << "commit total time...." << time_util_time_diff(&t1,&t2) << endl;
    determ::getInstance().add_total_commit_time(time_util_time_diff(&t1,&t2));
    }*/


  /// @brief End a transaction, aborting it if necessary.
  static void atomicEnd(bool update) {
    // Flush the stdout.
    struct timespec t1,t2;
    fflush(stdout);

    if (!_protection_enabled)
      return;
    //cout << "atomic end" << endl;
    //xmemory::printCurrentDirty();
    // Commit all private modifications to shared mapping
    ++commit_count;
    clock_gettime(CLOCK_REALTIME,&t1);
    xmemory::commit(update);
    clock_gettime(CLOCK_REALTIME,&t2);
    //cout << "pid " << getpid() << " commit total time...." << time_util_time_diff(&t1,&t2) << endl;
    determ::getInstance().add_total_commit_time(time_util_time_diff(&t1,&t2));
  }


};

#endif
