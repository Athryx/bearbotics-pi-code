#pragma once

#include "logging.h"
#include <functional>
#include <stdio.h>
#include <string>

class PanicException {
		PanicException(std::string message): m_message(message) {}
	private:
		std::string m_message;
};

#define panic(message) throw PanicException(message)

long get_usec();

template<typename T>
T time(const char *op_name, std::function<T ()> op, long *out_time = nullptr) {
	long old_usec = get_usec();
	auto ret = op();
	long new_usec = get_usec();
	long elapsed_usec = new_usec - old_usec;

	lg::info("%s elapsed time: %ld usec", op_name, elapsed_usec);
	if (out_time != nullptr) *out_time = elapsed_usec;
	return ret;
}

void time(const char *op_name, std::function<void ()> op, long *out_time = nullptr);
