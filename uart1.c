#include <stdio.h>
#include "CH559.h"
#include "uart1.h"

#ifndef FREQ_SYS
#define FREQ_SYS 48000000
#endif


void initUART1(uint32_t baud)
{
	uint16_t div = UART1CalculateDivisor(baud);
	initUART1withDivisor(div);
}

void initUART1withDivisor(uint16_t div)
{
	// Allow access to RBR, THR, IER, ADR
	SER1_LCR =  0;

	// set pin mode, RXD->P2.6 TXD->P2.7
	SER1_IER = bIER_PIN_MOD1;

	// Allow access to Baudrate control register
	SER1_LCR = bLCR_DLAB;

	// set baudrate divisor
	SER1_DIV = 1;
	SER1_DLL = div & 0xFF; 
	SER1_DLM = (div >> 8) & 0xFF; 

	// enable FIFO
	SER1_FCR = MASK_U1_FIFO_TRIG | bFCR_T_FIFO_CLR | bFCR_R_FIFO_CLR | bFCR_FIFO_EN;

	// set data format: 8bit, none, 1bit
	// Allow access to RBR, THR, IER, ADR
	SER1_LCR = 0x03;
}

uint16_t UART1CalculateDivisor(uint32_t baud)
{
	return (FREQ_SYS / 8) / 1 / baud;
	// return (FREQ_SYS / 8) / SER1_DIV / baud;
}

unsigned char UART1Available()
{
    return ((SER1_LSR & bLSR_DATA_RDY) != 0);
}

unsigned char UART1Receive()
{
	// Wait data available
	while (!(SER1_LSR & bLSR_DATA_RDY))
	{
		continue;
	}

	return SER1_RBR;
}

void UART1Send(unsigned char b)
{
	// Wait FIFO empty
	while (!(SER1_LSR & bLSR_T_FIFO_EMP))
	{
		continue;
	}

	// Send to FIFO
	SER1_THR = b;
}

unsigned char UART1TxIsEmpty()
{
	return ((SER1_LSR & bLSR_T_FIFO_EMP) != 0);
}
void UART1SendAsync(unsigned char b)
{
	SER1_THR = b;
}
