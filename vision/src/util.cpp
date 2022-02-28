#include "util.h"
#include <sys/time.h>

// gets microseconds since unix epoch
long get_usec()
{
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	return 1000000 * tv.tv_sec + tv.tv_usec;
}

void time(const char *op_name, std::function<void ()> op, long *out_time)
{
	long old_usec = get_usec();
	op();
	long new_usec = get_usec();
	long elapsed_usec = new_usec - old_usec;

	printf("%s elapsed time: %ld usec\n", op_name, elapsed_usec);
	if (out_time != nullptr) *out_time = elapsed_usec;
}
