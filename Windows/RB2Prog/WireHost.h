#ifndef WIREHOST_INCLUDED
#define WIREHOST_INCLUDED

#include "Serial.h"

// WireHost types.
typedef struct _wirehost wirehost;

// WireHost structures.
struct _wirehost
{
	serial *commport;

	// Discard flag for echoed data.
	int discard;

	// Buffered data.
	int buffered_head;
	int buffered_tail;
	WORD buffered_data[16];
};

// WireHost methods.
wirehost* wirehost_new(int port);
void wirehost_free(wirehost** self);
int wirehost_check_slave(wirehost *self, WORD slave_addr);
int wirehost_read_buffer(wirehost *self, WORD slave_addr, DWORD buffer_addr, WORD addr_count, WORD buffer_count, BYTE* buffer);
int wirehost_write_buffer(wirehost *self, WORD slave_addr, DWORD buffer_addr, WORD addr_count, WORD buffer_count, BYTE* buffer);

#endif // WIREHOST_INCLUDED
