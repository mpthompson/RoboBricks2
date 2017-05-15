// serialio.c
//
// Interrupt-based Serial I/O library for AvrX
//
// History: 2002.04.30 - Stephane Gauthier (stephane.gauthier@alcatel.com) - Genesis.
//			2002.05.03 - Stephane Gauthier - Corrected minor bug with interrupts enables.
//			2002.05.29 - Stephane Gauthier - Added mutex to PutChar to prevent clashes with multiple tasks.
//
// TODO: Add circular transmit buffer!
//
#include "avrx.h"
#include "hardware.h"

#define UCSR0B_INIT  ( (1<<TXEN) | (1<<RXEN) | (1<<RXCIE) )  // Enable TX and RX modules. Enable Receive Interrupt.

// Globals (file scope??)
AVRX_MUTEX(RxReady);							// AvrX Semaphore for signalling TX routine
AVRX_MUTEX(TxReady);							// AvrX Semaphore for signalling RX routine
AVRX_MUTEX(SerialTransmitMutex);				// To allow sharing of the PutChar routine.
unsigned char RxByte;								// Receive buffer.

// Function Name:   InitSerialIO()
// 
// Description:     Initialize Serial I/O hardware.
//
//
void InitSerialIO( unsigned int baud )
{
	/* Prime Serial TX mutex */
	AvrXSetSemaphore(&SerialTransmitMutex);
	
	/* Set baud rate */
	outp((unsigned char)(baud>>8), UBRR0H);
	outp((unsigned char)baud, UBRR0L);
	
	/* Enable receiver, transmitter and interrupts on RX and buffer empty */
	outp(UCSR0B_INIT, UCSR0B);
	
	/* Set frame format: 8data, no parity, 1 stop bit */
	outp((3<<UCSZ0), UCSR0C);
}

// Function Name:   PutChar()
// 
// Description:     Send character to serial port
//
//
void PutChar(unsigned char data)
{
	AvrXWaitSemaphore(&SerialTransmitMutex);
	
	if ( bit_is_clear(UCSR0A, UDRE))	// If buffer is already empty just proceed.
	{
		// Enable interrupt
		sbi(UCSR0B, UDRIE);

		// Wait for signal
		AvrXWaitSemaphore(&TxReady);
	}
	
	/* Put data into buffer, sends the data */
	outp(data, UDR0);
	
	AvrXSetSemaphore(&SerialTransmitMutex);
}

// Function Name:   GetChar()
// 
// Description:     Get a character from serial port
//
//
unsigned char GetChar(void)
{
	// Wait for receive interrupt to signal us
	AvrXWaitSemaphore(&RxReady);
	
	return(RxByte);
}
	
// 
// UART0 Transmit Buffer Empty ISR
//
AVRX_SIGINT(SIG_UART0_DATA)
{
	// disable interrupts.
	cbi(UCSR0B, UDRIE);
        
	// Switch to kernel stack
	IntProlog();

	// Signal we are ready for more.
	AvrXIntSetSemaphore(&TxReady);
	
	// Go back to RTOS
	Epilog();
}
	
// 
// UART0 Receive ISR
//
AVRX_SIGINT(SIG_UART0_RECV)
{
	// disable interrupt
	cbi(UCSR0B, RXCIE);

	// Switch to kernel stack
	IntProlog();
        
	// Get it.
	RxByte = inp(UDR0);

	// Signal routine that character is ready.
	AvrXIntSetSemaphore(&RxReady);
	
	// Re-enable interrupts
	sbi( UCSR0B, RXCIE );
      
	// Go  back to RTOS
	Epilog();
}

