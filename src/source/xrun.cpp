#include "xrun.h"

size_t xrun::_master_thread_id;
size_t xrun::_thread_index;
bool xrun::_fence_enabled;
struct timespec xrun::fence_start;
struct timespec xrun::fence_end;
int xrun::fence_total_us;
struct timespec xrun::token_start;
struct timespec xrun::token_end;
int xrun::token_total_us;
volatile bool xrun::_initialized = false;
volatile bool xrun::_protection_enabled = false;
size_t xrun::_children_threads_count = 0;
size_t xrun::_lock_count = 0;
bool xrun::_token_holding = false;
int xrun::free_count=0;
int xrun::commit_count=0;
struct timespec xrun::ts1;
struct timespec xrun::ts2;

commit_stats * xrun::token_stats;
commit_stats * xrun::fence_stats;
commit_stats * xrun::barrier_stats;
commit_stats * xrun::mutex_stats;
commit_stats * xrun::atomic_begin_stats;
commit_stats * xrun::atomic_end_stats;
commit_stats * xrun::getlock_count;
