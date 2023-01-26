#pragma once

#include <stdbool.h>

#define CMD_MAX   128
#define LINE_MAX  512

void init_sig();
void prompt();
bool read_line(char *line);
void run_line(char *line);
