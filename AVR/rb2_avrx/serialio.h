// serialio.h
//
// Low Level Serial I/O routines (for AvrX)
//
// History: Created on May 1st 2002 by Stephane Gauthier.
//

// Function Name:   InitSerialIO()
// 
// Description:     Initialize Serial I/O hardware.
//
//
void InitSerialIO( unsigned int baud);

// Function Name:   PutChar()
// 
// Description:     Send character to serial port
//
//
void PutChar(unsigned char data);

// Function Name:   GetChar()
// 
// Description:     Get a character from serial port
//
//
unsigned char GetChar(void);
