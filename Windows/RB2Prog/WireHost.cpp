#include "stdafx.h"
#include "WireHost.h"

static int wirehost_send_word(wirehost *self, WORD data)
// Send the indicated data words to the wire host.  Notice that a nine 
// bit is sent to the wirehost as two separate bytes.  A flag is set 
// so the echoed word is ignored for reading.
{
	BYTE upper_bits;
	BYTE lower_bits;

	// Prepare the upper and lower bits.
	upper_bits = 0x80 | ((BYTE) (data >> 4) & 0x1f);
	lower_bits = 0xa0 | ((BYTE) data & 0x0f);

	// Send the upper 5 bits.
	if (!serial_write_byte(self->commport, upper_bits)) return 0;

	// Send the lower 4 bits.
	if (!serial_write_byte(self->commport, lower_bits)) return 0;

	// Increment the discard count.
	++self->discard;

	// Return success.
	return 1;
}


static int wirehost_receive_word(wirehost *self, WORD *data)
// Reads a single word from the wirehost.  Read the indicated data bytes.  
{
	// Check to see if we need to read a character to be discarded.
	if (self->discard > 0)
	{
		// Decrement the discard count.
		--self->discard;

		// Recursively call to get the character to be discarded.
		wirehost_receive_word(self, data);
	}

	// If the buffer is empty we need to read more data.
	if (self->buffered_head == self->buffered_tail)
	{
		BYTE i;
		BYTE ch;
		BYTE bits;
		BYTE count;
		BYTE shift;

		// Send the read request.
		if (!serial_write_byte(self->commport, 0xb0)) return 0;

		// Read first word containing the upper bits and count.
		if (!serial_read_byte(self->commport, &ch)) return 0;

		// Get the upper bits.
		bits = (ch >> 3) & 0x1f;

		// Get the count of buffer bytes.
		count = ch & 0x07;

		// Set the shift mask.
		shift = 4;

		// Read the bytes.
		for (i = 0; i < count; ++i)
		{
			WORD word;

			// Read the next set of lower bits.
			if (!serial_read_byte(self->commport, &ch)) return 0;

			// Form the nine bit word.
			word = (((bits >> shift) & 0x01) << 8) | ch;

			// Insert into the buffer.
			self->buffered_data[self->buffered_tail++] = word;

			// Check for looping within the buffer.
			if (self->buffered_tail > 15) self->buffered_tail = 0;

			// Decrement the shift mask.
			--shift;
		}
	}

	// Did we pick up any characters?
	if (self->buffered_head != self->buffered_tail)
	{
		// Return the next buffered data.
		*data = self->buffered_data[self->buffered_head++];

		// Check for looping at the head of the buffer.
		if (self->buffered_head > 15) self->buffered_head = 0;

		// Return success.
		return 1;
	}

	// We failed to get a character.
	return 0;
}


wirehost* wirehost_new(int port)
// Allocate a new wirehost object.
{
	wirehost* self = NULL;
	serial* commport = NULL;
	char portnamebuffer[64];

	// Convert the port number to a port name.
	_snprintf_s(portnamebuffer, sizeof(portnamebuffer), "COM%d", port);

	// Create the serial port object.
	commport = serial_new();

	// Validate the serial port object.
	if (commport == NULL) return NULL;

	// Open the serial port.
	if (!serial_open_port(commport, portnamebuffer))
	{
		serial_free(&commport);
		return NULL;
	}

	// Configure the serial port.
	if (!serial_configure_port(commport, CBR_115200, 8, SERIAL_NOPARITY, SERIAL_ONESTOPBIT))
	{
		serial_close_port(commport);
		serial_free(&commport);
		return NULL;
	}

	// Configure the serial port.
	if (!serial_configure_timeouts(commport, 1000, 1000, 1000, 1000, 1000))
	{
		serial_close_port(commport);
		serial_free(&commport);
		return NULL;
	}

	// Allocate a new wirehost object.
	self = (wirehost*) malloc(sizeof(wirehost));
	
	// Did we allocate the object?
	if (self != NULL)
	{
		// Yes. Initialize here.
		self->discard = 0;
		self->commport = commport;
		self->buffered_head = 0;
		self->buffered_tail = 0;
	}
	else
	{
		// Clean up.
		if (self) free(self);

		return NULL;
	}

	return self;
}


void wirehost_free(wirehost** self)
{
	// Sanity check the object pointer.
	if ((*self) != NULL)
	{
		// Close the commport if open.
		if ((*self)->commport) serial_close_port((*self)->commport);

		// Free the commport.
		if ((*self)->commport) serial_free(&(*self)->commport);

		// Free this object.
		free(*self);

		// Set this object to null.
		*self = NULL;
	}
}


int wirehost_check_slave(wirehost *self, WORD slave_addr)
// Check the slave.
{
	WORD address;
	WORD response;

	// Sanity check the slave address.
	if (slave_addr > 255) return 0;

	// Set the address of the slave with the ninth bit set.
	address = 0x0100 | slave_addr;

	// Select the indicated slave.
	if (!wirehost_send_word(self, address)) return 0;

	// Clear the wire host receive buffer of any characters.
	while (wirehost_receive_word(self, &response));

	// Select the indicated slave.
	if (!wirehost_send_word(self, address)) return 0;

	// Receive and validate the received response.
	if (!wirehost_receive_word(self, &response)) return 0;

	// Return if we succeeded or not.
	if (response != 0x00A5) return 0;

	// Have the slave enter it's bootloader.
	if (!wirehost_send_word(self, 0x00FB)) return 0;

	// Receive and validate the received response.
	if (!wirehost_receive_word(self, &response)) return 0;

	// Return if we succeeded or not.
	if (response != 0x00A5) return 0;

	// Sleep to allow the bootloader to activate.
	Sleep(500);

	// We succeeded.
	return 1;
}


int wirehost_read_buffer(wirehost *self, WORD slave_addr, DWORD buffer_addr, WORD addr_count, WORD buffer_count, BYTE* buffer)
// Read the buffer from the indicated slave.
{
	int i;
	WORD address;
	WORD response;
	WORD send_address;
	WORD send_count;

	// Sanity check the slave address.
	if ((slave_addr < 0) || (slave_addr > 255)) return 0;

	// Sanity check the buffer count.
	if ((buffer_count < 1) || (buffer_count > 256)) return 0;

	// Clear the wire host receive buffer of any characters.
	while (wirehost_receive_word(self, &response));

	// Set the address of the slave with the ninth bit set.
	address = 0x0100 | slave_addr;

	// Select the indicated slave.
	if (!wirehost_send_word(self, address)) return 0;

	// Receive the slave response.
	if (!wirehost_receive_word(self, &response)) return 0;

	// Validate the response.
	if (response != 0x00A5) return 0;

	// Send the read buffer command.
	if (!wirehost_send_word(self, 0x0001)) return 0;

	// Receive the slave response.
	if (!wirehost_receive_word(self, &response)) return 0;

	// Validate the response.
	if (response != 0x00A5) return 0;

	// Send each address byte.
	for (i = 0; i < addr_count; ++i)
	{
		// Get the address byte to send.
		send_address = (WORD) (buffer_addr >> (8 * (addr_count - (i + 1)))) & 0xff;

		// Send the buffer address byte.
		if (!wirehost_send_word(self, send_address)) return 0;

		// Receive the slave response.
		if (!wirehost_receive_word(self, &response)) return 0;

		// Validate the response.
		if (response != send_address) return 0;
	}

	// Set the send count.
	send_count = (buffer_count < 256) ? buffer_count : 0;

	// Send the buffer count.
	if (!wirehost_send_word(self, send_count)) return 0;

	// Receive the slave response.
	if (!wirehost_receive_word(self, &response)) return 0;

	// Validate the response.
	if (response != send_count) return 0;

	// Receive each buffer word.
	for (i = 0; i < buffer_count; ++i)
	{
		// Send the dummy character.
		if (!wirehost_send_word(self, 0x00)) return 0;

		// Receive the data word.
		if (!wirehost_receive_word(self, &response)) return 0;

		// Write the data word to the buffer.
		buffer[i] = (BYTE) response;
	}

	// We succeeded.
	return 1;
}

int wirehost_write_buffer(wirehost *self, WORD slave_addr, DWORD buffer_addr, WORD addr_count, WORD buffer_count, BYTE* buffer)
// Write the buffer to the indicated slave.
{
	int i;
	WORD address;
	WORD response;
	WORD send_address;
	WORD send_count;

	// Sanity check the slave address.
	if ((slave_addr < 0) || (slave_addr > 255)) return 0;

	// Sanity check the buffer count.
	if ((buffer_count < 1) || (buffer_count > 256)) return 0;

	// Clear the wire host receive buffer of any characters.
	while (wirehost_receive_word(self, &response));

	// Set the address of the slave with the ninth bit set.
	address = 0x0100 | slave_addr;

	// Select the indicated slave.
	if (!wirehost_send_word(self, address)) return 0;

	// Receive the slave response.
	if (!wirehost_receive_word(self, &response)) return 0;

	// Validate the response.
	if (response != 0x00A5) return 0;

	// Send the read buffer command.
	if (!wirehost_send_word(self, 0x0002)) return 0;

	// Receive the slave response.
	if (!wirehost_receive_word(self, &response)) return 0;

	// Validate the response.
	if (response != 0x00A5) return 0;

	// Send each address byte.
	for (i = 0; i < addr_count; ++i)
	{
		// Get the address byte to send.
		send_address = (WORD) (buffer_addr >> (8 * (addr_count - (i + 1)))) & 0xff;

		// Send the buffer address byte.
		if (!wirehost_send_word(self, send_address)) return 0;

		// Receive the slave response.
		if (!wirehost_receive_word(self, &response)) return 0;

		// Validate the response.
		if (response != send_address) return 0;
	}

	// Set the send count.
	send_count = (buffer_count < 256) ? buffer_count : 0;

	// Send the buffer count.
	if (!wirehost_send_word(self, send_count)) return 0;

	// Receive the slave response.
	if (!wirehost_receive_word(self, &response)) return 0;

	// Validate the response.
	if (response != send_count) return 0;

	// Send each buffer word.
	for (i = 0; i < buffer_count; ++i)
	{
		// Send the data word.
		if (!wirehost_send_word(self, (WORD) buffer[i])) return 0;

		// Receive the echoed data word.
		if (!wirehost_receive_word(self, &response)) return 0;

		// Validate the response.
		if (response != (WORD) buffer[i]) return 0;
	}

	// We succeeded.
	return 1;
}

