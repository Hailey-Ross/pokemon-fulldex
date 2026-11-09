#pragma once
#include <cstdint>
int socketInitializeDefault();
void socketExit();
void svcSleepThread(uint64_t);
#define R_FAILED(rc) ((rc) != 0)
