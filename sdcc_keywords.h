#ifndef __SDCC_KEYWORDS_H__
#define __SDCC_KEYWORDS_H__

#ifndef SDCC
#define __code
#define __data
#define __xdata
#define __at(X)
#ifndef SBIT
#define SBIT(X, Y, Z)  const uint8_t X = Y
#endif
#define INTERRUPT_USING(X, Y, Z) void X()
#endif

#endif
