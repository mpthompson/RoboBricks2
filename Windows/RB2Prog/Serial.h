#ifndef SERIAL_INCLUDED
#define SERIAL_INCLUDED

// Parity scheme.
#define SERIAL_NOPARITY		NOPARITY
#define SERIAL_EVENPARITY	EVENPARITY
#define SERIAL_ODDPARITY	ODDPARITY
#define SERIAL_MARKPARITY	MARKPARITY
#define SERIAL_SPACEPARITY	SPACEPARITY

// Number of stop bits used.
#define SERIAL_ONESTOPBIT	ONESTOPBIT
#define SERIAL_ONE5STOPBITS	ONE5STOPBITS
#define SERIAL_TWOSTOPBITS	TWOSTOPBITS

// Serial types.
typedef struct _serial serial;

// Serial structures.
struct _serial
{
	// Windows specific types.
	HANDLE hcomm;
	DCB dcb;
	COMMTIMEOUTS timeouts;
	BOOL port_ready;
};

// Serial methods.
serial* serial_new(void);
void serial_free(serial** self);

int serial_open_port(serial* self, const char* portname);
int serial_close_port(serial* self);

int serial_configure_port(serial* self, int baud_rate, int byte_size, int parity, int stop_bits);
int serial_configure_timeouts(serial* self, int read_interval_timeout, int read_total_timeout_multiplier, int read_total_timeout_constant, int write_total_timeout_multiplier, int write_total_timeout_constant);

int serial_read_byte(serial* self, BYTE* byte);
int serial_write_byte(serial* self, BYTE byte);

#endif // SERIAL_INCLUDED
