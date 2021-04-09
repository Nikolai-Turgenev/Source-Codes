#pragma once

#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>

#define MAX_PARAMS 10

#define INET_ADDR(o1,o2,o3,o4) (htonl((o1 << 24) | (o2 << 16) | (o3 << 8) | (o4 << 0)))

enum {
	STDIN = 0,
	STDOUT = 1,
	STDERR = 2,
	TRUE = 1,
	FALSE = 0,
	BOT_PORT = 911
};

typedef char BOOL;
