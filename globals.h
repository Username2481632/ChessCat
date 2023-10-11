#pragma once
#include <semaphore>
#include <mutex>

extern unsigned long long int max_bytes;
extern  unsigned long long int min_free;
extern std::binary_semaphore computed;
inline std::mutex position_mutex;
extern int default_max_depth_extension;
extern size_t min_depth;
extern size_t thread_count;
inline std::mutex t_c_mutex;
inline std::mutex cv_mtx;
extern bool threads_maxed;
extern size_t max_threads;
inline std::condition_variable threads_maxed_cv;