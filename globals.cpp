#include "globals.h"
unsigned long long int max_bytes = (1 << 30) * (unsigned long long int)16;
unsigned long long int min_free = (2U << 30);
std::binary_semaphore computed{0};
int default_max_depth_extension = -3;
size_t min_depth = 4;
size_t thread_count = 0;
bool threads_maxed = false;
size_t max_threads = 6;