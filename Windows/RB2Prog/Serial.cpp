#include "stdafx.h"
#include "Serial.h"

serial* serial_new(void)
// Allocate a new serial object.
{
	serial* self = NULL;

	// Allocate a new serial object.
	self = (serial*) malloc(sizeof(serial));
	
	// Did we allocate the object.
	if (self != NULL)
	{
		// Reset serial variables.
		self->hcomm = NULL;
		self->port_ready = FALSE;
	}
	else
	{
		// Clean up.
		if (self) free(self);

		return NULL;
	}

	return self;
}


void serial_free(serial** self)
{
	// Sanity check the object pointer.
	if ((*self) != NULL)
	{
		// Close the serial port if needed.
		serial_close_port(*self);

		// Free this object.
		free(*self);

		// Set this object to null.
		*self = NULL;
	}
}


int serial_open_port(serial* self, const char* portname)
{
	char portbuffer[64];

	// Close the serial port if needed.
	serial_close_port(self);

	// Use speical characters to open the serial port device directly.
	_snprintf_s(portbuffer, sizeof(portbuffer), "//./%s", portname);

	// Open the serial port.
	self->hcomm = CreateFile(portbuffer, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);

	// Did we open the device?
	if (self->hcomm == INVALID_HANDLE_VALUE)
	{
		// No. Reset the handle.
		self->hcomm = NULL;
	}

	return (self->hcomm != NULL) ? 1 : 0;
}


int serial_close_port(serial* self)
{
	if (self->hcomm != NULL)
	{
		// Close the serial port.
		CloseHandle(self->hcomm);

		self->hcomm = NULL;
	}

	return 1;
}


int serial_configure_port(serial* self, int baud_rate, int byte_size, int parity, int stop_bits)
{
	// Make sure the port is open.
	if (self->hcomm == NULL) return 0;

	// Get the current serial port configuration.
	if ((self->port_ready = GetCommState(self->hcomm, &self->dcb)) != 0)
	{
		self->dcb.BaudRate = (DWORD) baud_rate;
		self->dcb.ByteSize = (BYTE) byte_size;
		self->dcb.Parity = (BYTE) parity;
		self->dcb.StopBits = (BYTE) stop_bits;
		self->dcb.fBinary = TRUE;
		self->dcb.fDsrSensitivity = FALSE;
		self->dcb.fParity = (parity == NOPARITY) ? FALSE : TRUE;
		self->dcb.fOutX = FALSE;
		self->dcb.fInX = FALSE;
		self->dcb.fNull = FALSE;
		self->dcb.fAbortOnError = TRUE;
		self->dcb.fOutxCtsFlow = FALSE;
		self->dcb.fOutxDsrFlow = FALSE;
		self->dcb.fDtrControl = DTR_CONTROL_DISABLE;
		self->dcb.fDsrSensitivity = FALSE;
		self->dcb.fRtsControl = RTS_CONTROL_DISABLE;
		self->dcb.fOutxCtsFlow = FALSE;
		self->dcb.fOutxCtsFlow = FALSE;

		// Update the serial port configuration.
		self->port_ready = SetCommState(self->hcomm, &self->dcb);
	}

	return (self->port_ready != 0) ? 1 : 0;
}


int serial_configure_timeouts(serial* self, int read_interval_timeout, int read_total_timeout_multiplier, int read_total_timeout_constant, int write_total_timeout_multiplier, int write_total_timeout_constant)
{
	// Get the current serial timeout configuration.
	if ((self->port_ready = GetCommTimeouts(self->hcomm, &self->timeouts)) != 0)
	{
		self->timeouts.ReadIntervalTimeout = (DWORD) read_interval_timeout;
		self->timeouts.ReadTotalTimeoutConstant = (DWORD) read_total_timeout_constant;
		self->timeouts.ReadTotalTimeoutMultiplier = (DWORD) read_total_timeout_multiplier;
		self->timeouts.WriteTotalTimeoutConstant = (DWORD) write_total_timeout_constant;
		self->timeouts.WriteTotalTimeoutMultiplier = (DWORD) write_total_timeout_multiplier;

		// Update the serial timeout configuration.
		self->port_ready = SetCommTimeouts(self->hcomm, &self->timeouts);
	}

	return (self->port_ready != 0) ? 1 : 0;
}


int serial_read_byte(serial* self, BYTE* byte)
{
	int rv = 0;
	BYTE buffer;
	DWORD bytes_read = 0;

	// Initialize the return byte.
	*byte = 0;

	// Make sure the port is open.
	if (self->hcomm == NULL) return 0;

	// Read a byte from the serial port.
	if (ReadFile(self->hcomm, &buffer, 1, &bytes_read, 0))
	{
		// Did we read a byte?
		if (bytes_read == 1)
		{
			*byte = (unsigned char) buffer;

			// We succeeded.
			rv = 1;
		}
	}

	return rv;
}


int serial_write_byte(serial* self, BYTE byte)
{
	DWORD bytes_written = 0;

	// Make sure the port is open.
	if (self->hcomm == NULL) return 0;

	return WriteFile(self->hcomm, &byte, 1, &bytes_written, NULL) != 0 ? 1 : 0;
}

