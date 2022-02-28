#pragma once

#include <functional>
#include <stdio.h>

long get_usec();

template<typename T>
T time(const char *op_name, std::function<T ()> op, long *out_time = nullptr)
{
	long old_usec = get_usec();
	auto ret = op();
	long new_usec = get_usec();
	long elapsed_usec = new_usec - old_usec;

	printf("%s elapsed time: %ld usec\n", op_name, elapsed_usec);
	if (out_time != nullptr) *out_time = elapsed_usec;
	return ret;
}

void time(const char *op_name, std::function<void ()> op, long *out_time = nullptr);
