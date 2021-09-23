#ifndef __UART1_H__
#define __UART1_H__

void initUART1(uint32_t baud);
void initUART1withDivisor(uint16_t div);
uint16_t UART1CalculateDivisor(uint32_t baud);
unsigned char UART1Available();
unsigned char UART1Receive();
void UART1Send(unsigned char b);
unsigned char UART1TxIsEmpty();
void UART1SendAsync(unsigned char b);

#endif
