// -*- C++ -*-
#ifndef _XPERSIST_H_
#define _XPERSIST_H_
/*
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
 * @file   xpersist.h
 * @brief  Main file to handle page fault, commits.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 * @author Charlie Curtsinger <http://www.cs.umass.edu/~charlie>
 */

#include <set>
#include <list>
#include <vector>
#include <map>

#if !defined(_WIN32)
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>
#include <ksnap.h>

#include "xatomic.h"
#include "heaplayers/ansiwrapper.h"
#include "heaplayers/freelistheap.h"

#include "heaplayers/stlallocator.h"
#include "privateheap.h"
#include "xdefines.h"
#include "xbitmap.h"

#include "debug.h"
#include "xpageentry.h"
#include "stats.h"
#include "time_util.h"

/**
 * @class xpersist
 * @brief Makes a range of memory persistent and consistent.
 */
template<class Type, int NElts = 1>
class xpersist {
public:

  /// @arg startaddr  the optional starting address of the local memory.
  xpersist(void * startaddr = 0, size_t startsize = 0) :
    _startaddr(startaddr), _startsize(startsize) {

    //can't use mkstemp with kSnap, so we randomly generate a name for now...need a better way to do this
    srand(time(NULL));

    // Check predefined globals size is large enough or not. 
    if (_startsize > 0) {
      if (_startsize > NElts * sizeof(Type)) {
        fprintf(stderr, "This persistent region (%Zd) is too small (%Zd).\n", NElts * sizeof(Type), _startsize);
        ::abort();
      }
    }

    // Get a temporary file name (which had better not be NFS-mounted...).
    char _backingFname[L_tmpnam];
    //sprintf(_backingFname, "graceMXXXXXX");
    sprintf(_backingFname, "graceM%d%s", rand() % 10000, (startaddr)?"global":"heap");

    //for global vars, we need to copy the contents somewhere, and then back once we mmap
    //the memory segment.
    Type * tmp;
    if (startaddr != NULL){
      tmp = (Type *)malloc(startsize);
      memcpy(tmp, (Type *)startaddr, startsize);
    }

    snap_memory = conv_open(NElts * sizeof(Type), _backingFname, (startaddr)?startaddr:NULL);

    //cout << "start addr " << startaddr << " " << snap_memory->segment << endl;
    if (startaddr && tmp){
      memcpy((Type *)startaddr,tmp, startsize);
    }

    if (!snap_memory){
      fprintf(stderr, "conversion open failed somehow.\n");
      ::abort();
    }
    _snapMemory = (char *)snap_memory->segment;

    _isProtected = false;
  }

  void clearUserInfo(void){
    }

  void initialize(void) {
    //initialize statistics
  }

  void finalize(void) {
    if (_isProtected)
      closeProtection();

  }

  void openProtection(void * end) {
    conv_commit(snap_memory);
    _isProtected = true;
    _trans = 0;
  }

  void closeProtection(void) {
    _isProtected = false;
  }

  void setThreadIndex(int index) {
    _threadindex = index;
  }

  /// @return true iff the address is in this space.
  inline bool inRange(void * addr) {
    if (((size_t) addr >= (size_t) base()) && ((size_t) addr
        < (size_t) base() + size())) {
      return true;
    } else {
      return false;
    }
  }

  /// @return the start of the memory region being managed.
  inline Type * base(void) const {
    return _snapMemory;
  }

  bool mem_write(void * addr, void *val) {
    unsigned long offset = (intptr_t)addr - (intptr_t)base();
    void **ptr = (void**)((intptr_t)_snapMemory + offset);
    *ptr = val;
    //fprintf(stderr, "addr %p val %p(int %d) with ptr %p\n", addr, val, (unsigned long)val, *ptr);
    return true;
  }

  /// @return the size in bytes of the underlying object.
  inline int size(void) const {
    return NElts * sizeof(Type);
  }
  
  bool nop(void) {
  }

  /// @brief Start a transaction.
  inline void begin(bool cleanup) {
    // Update all pages related in this dirty page list
    conv_update(snap_memory);
  }

  inline void recordPageChanges(int pageNo) {}

  // Get the start address of specified page.
  inline void * getPageStart(int pageNo) {
    return ((void *)((intptr_t)base() + pageNo * xdefines::PageSize));
  }

  inline void merge_for_barrier(){
    conv_merge_barrier_determ(snap_memory);
  }

  inline void update_for_barrier(){
    //ksnap_update_barrier_determ(snap_memory);
    conv_update_only_barrier_determ(snap_memory);
  }

  inline void settle_for_barrier(){
    INC_COUNTER(commit);
    conv_commit_barrier_determ(snap_memory);
  }

  // Commit local modifications to shared mapping
  inline void checkandcommit(bool update) {
    INC_COUNTER(commit);
    //just update...this gets done in parallel and lightens the load on sequential commit
    if (update){
      conv_merge(snap_memory);
    }
    else{
      conv_merge(snap_memory);
      conv_commit(snap_memory);
    }
    return;
  }


  /// @brief Commit all writes.
  inline void memoryBarrier(void) {
    xatomic::memoryBarrier();
  }

private:
  /// True if current xpersist.h is a heap.
  bool _isHeap;

  /// The starting address of the region.
  void * const _startaddr;

  /// The size of the region.
  const size_t _startsize;

  /// The file descriptor for the backing store.
  int _backingFd;

  Type * _snapMemory;
  conv_seg * snap_memory;

  bool _isProtected;

  unsigned int _trans;

  /// The length of the version array.
  enum {  TotalPageNums = NElts * sizeof(Type) / (xdefines::PageSize) };

  int _threadindex;
};

#endif
