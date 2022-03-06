#include "logging.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

void lg::info(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);

	// prints info in blue
	printf("\e[0;34minfo:\e[0m ");
	vprintf(fmt, args);
	printf("\n");

	va_end(args);
}

void lg::warn(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);

	// prints warning in yellow
	printf("\e[0;33mwarning:\e[0m ");
	vprintf(fmt, args);
	printf("\n");

	va_end(args);
}

void lg::error(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);

	// prints error in red
	printf("\e[0;31merror:\e[0m ");
	vprintf(fmt, args);
	printf("\n");

	va_end(args);
}

void lg::critical(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);

	// prints critical in bold high intensity red
	printf("\e[1;91mcritical:\e[0m ");
	vprintf(fmt, args);
	printf("\n");

	va_end(args);

	exit(1);
}
