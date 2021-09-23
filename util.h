#ifndef __UTIL_H__
#define __UTIL_H__
#include <stdio.h>
#if 1
#define DEBUG_OUT(...) printf(__VA_ARGS__);
#else
#define DEBUG_OUT(...) (void)0;
#endif

void initClock();
void delayUs(unsigned short n);
void delay(unsigned short n);
void get_chip_id(__xdata uint8_t *dst); // writes 8 bytes to dst
void init_watchdog(uint8_t enable);
void clear_watchdog();

typedef void(*  FunctionReference)();
extern FunctionReference runBootloader;

#endif