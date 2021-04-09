#pragma once

#include <stdint.h>

void init_killer(void);
void terminate_process_via_port(uint16_t);
void terminate_killer_process(void);

enum{
	START_PID = 300,
	SCAN_RESET_TIME = 60
};
