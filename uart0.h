#ifndef __UART0_H__
#define __UART0_H__

void initUART0(unsigned long baud, int alt);
unsigned char UART0Receive();
void UART0Send(unsigned char b);

#endif
