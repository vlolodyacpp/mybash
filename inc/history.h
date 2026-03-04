#pragma once

#include <stddef.h>

void history_init();
void history_shutdown();
void history_add(const char *);
const char *history_get_from_tail(int);
const char *history_get(int);
int history_size();
