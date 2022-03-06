#include "logging.h"
#include <stdio.h>
#include <stdarg.h>

void warn(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	
	printf("\e[0;33mwarning:\e[0;0m ");
	vprintf(fmt, args);
	printf("\n");

	va_end(args);
}

void error(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	
	printf("\e[0;31merror:\e[0;0m ");
	vprintf(fmt, args);
	printf("\n");

	va_end(args);
}
