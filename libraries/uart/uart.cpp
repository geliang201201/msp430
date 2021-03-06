#include "uart.h"

#define RX_OVER -2

struct baud_config {
	unsigned long baud_val;
	unsigned int UCAxBR0_val;
	unsigned int UCAxBR1_val;
	unsigned int UCAxMCTL_val;
};

struct RX_buff {
	char c[BUFF_SIZE];
	unsigned char head;
	unsigned char tail;
};

// Look at table in MSP430x2xx Family User’s Guide or
// 1Mhz
// {9600, 104, 0, UCBRS_1},
// {115200, 8, 0, UCBRS2 + UCBRS0}
// 16Mhz
// {9600, 130, 6, UCBRS_6 | UCBRF_0},
// {115200, 138, 0, UCBRS_7 | UCBRF_0}
const struct baud_config baud_tb[] = {
	{9600, 65, 3, UCBRS_2 | UCBRF_0},
	{115200, 69, 0, UCBRS_4 | UCBRF_0}
};


static struct RX_buff rx_buff = { {0}, 0, 0};

Uart::Uart(unsigned long BAUDRATE) {

	WDTCTL = WDTPW + WDTHOLD;  // Stop WDT
	UCA0CTL0 = 0;					// 8 bit, no parity, LSB, 1 stop bit, Asynchronous

	if (CALBC1_8MHZ==0xFF) {  // If calibration constant erased
		while(1);          	// do not load, trap CPU!!
	}

   BCSCTL1 = CALBC1_8MHZ;   // Set range
   DCOCTL  = CALBC1_8MHZ;   // Set DCO step + modulation

	P1SEL = BIT1 + BIT2 ;     // P1.1 = RXD, P1.2=TXD
	P1SEL2 = BIT1 + BIT2 ;    // P1.1 = RXD, P1.2=TXD

	UCA0CTL1 |=  UCSSEL_2 + UCSWRST;  				// USCI Clock = SMCLK, USCI_A0 disabled

	for(int i=0; i< sizeof(baud_tb)/sizeof(baud_tb[0]); i++) {
		if(BAUDRATE==baud_tb[i].baud_val) {
			UCA0BR0   =  baud_tb[i].UCAxBR0_val;	// set register UCAxBR0
			UCA0BR1   =  baud_tb[i].UCAxBR1_val;	// set register UCAxBR1
			UCA0MCTL  =  baud_tb[i].UCAxMCTL_val;  // Set modulation
			break;
		}
	}

	UCA0CTL1 &= ~UCSWRST;             // Clear UCSWRST to enable USCI_A0
	// IE2 |= UCA0TXIE;               // Enable the Transmit interrupt
   IE2 |= UCA0RXIE;               // Enable the Receive  interrupt
   __bis_SR_register(GIE);                     // Enable the global interrupt
}

size_t Uart::write(char c) {
	while (!(IFG2 & UCA0TXIFG));                  // USCI_A0 TX buffer ready?
	UCA0TXBUF = c;                    		     // TX -> RXed character
	return 1;
}

size_t Uart::write(const char *s, size_t size) {
	size_t n = 0;
	while (size--) {
		n += write(*s++);
	}
	return n;
}

size_t Uart::print(char c) {
	return write(c);
}

size_t Uart::print(const char *s) {
	return write((const char *)s, strlen(s));
}

size_t Uart::print(const char *s, size_t size) {
	return write((const char *)s, size);
}

size_t Uart::print(unsigned char num, int base) {
	return print((unsigned long) num, base);
}

size_t Uart::print(int num, int base) {
  return print((long) num, base);
}

size_t Uart::print(unsigned int num, int base) {
  return print((unsigned long) num, base);
}

size_t Uart::print(long num, int base) {
	if (base == 0) {
		return write(num);
	} else if (base == 10) {
		if (num < 0) {
			unsigned char t = print('-');
			num = -num;
			return printNumber(num, 10) + t;
		}
		return printNumber(num, 10);
	} else {
		return printNumber(num, base);
	}
}

size_t Uart::print(unsigned long num, int base) {
	if (base == 0) return write(num);
	else return printNumber(num, base);
}

size_t Uart::print(double num, int digits) {
	return printFloat(num, digits);
}

size_t Uart::println(void) {
  size_t n = print('\r');
  n += print('\n');
  return n;
}

size_t Uart::println(char c) {
	size_t n = print(c);
	n += print('\n');
   return n;
}

size_t Uart::println(const char *s) {
   size_t n = print(s);
	n +=  println();
	return n;
}

size_t Uart::println(const char *s, size_t size) {
   size_t n = print(s, size);
	n +=  println();
	return n;
}

size_t Uart::println(unsigned char num, int base) {
	size_t n = print(num, base);
	n +=  println();
	return n;
}

size_t Uart::println(int num, int base) {
	size_t n = print(num, base);
	n +=  println();
	return n;
}

size_t Uart::println(unsigned int num, int base) {
	size_t n = print(num, base);
	n +=  println();
	return n;
}

size_t Uart::println(long num, int base) {
	size_t n = print(num, base);
	n +=  println();
	return n;
}

size_t Uart::println(unsigned long num, int base) {
	size_t n = print(num, base);
	n +=  println();
	return n;
}

size_t Uart::println(double num, int digits) {
	size_t n = print(num, digits);
	n +=  println();
	return n;
}

size_t Uart::printNumber(unsigned long n, int base) {
	char buf[8 * sizeof(long) + 1]; // Assumes 8-bit chars plus zero byte.
	char *str = &buf[sizeof(buf) - 1];

	*str = '\0';

	// prevent crash if called with base == 1
	if (base < 2) base = 10;

	do {
		unsigned long m = n;
		n /= base;
		char c = m - base * n;
		*--str = c < 10 ? c + '0' : c + 'A' - 10;
	} while(n);

	return print(str);
}

size_t Uart::printFloat(double number, unsigned char digits) {

	size_t n = 0;
	// Handle negative numbers
	if (number < 0.0) {
		n += print('-');
		number = -number;
	}

	// Round correctly so that print(1.999, 2) prints as "2.00"
	double rounding = 0.5;
	for (uint8_t i=0; i<digits; ++i)
		rounding /= 10.0;

	number += rounding;

	// Extract the integer part of the number and print it
	long int_part = (unsigned long)number;
	double remainder = number - (double)int_part;
	n += print((long)int_part);

	// Print the decimal point, but only if there are digits beyond
	if (digits > 0) {
		n += print(".");
	}

	// Extract digits from the remainder one at a time
	while (digits-- > 0) {
		remainder *= 10.0;
		int toPrint = int(remainder);
		n += print((long)toPrint);
		remainder -= toPrint;
	}

	return n;
}

size_t Uart::printSpecial(const char *s, char delim) {
	size_t n = print(s);
	n += print(delim);
	return n;
}

size_t Uart::printSpecial(const char *s, const char *s2) {
	size_t n = print(s);
	n += print(s2);
	return n;
}

size_t Uart::printString(const char *s) {
	return printSpecial(s);
}

size_t Uart::available() {
	return (unsigned char )(BUFF_SIZE + rx_buff.head - rx_buff.tail) % BUFF_SIZE;
}

char Uart::read() {

	if(rx_buff.tail==rx_buff.head) {
		return RX_OVER;
	}
	char c = rx_buff.c[rx_buff.tail];
	rx_buff.tail = (unsigned char)(rx_buff.tail+1) % (unsigned char)BUFF_SIZE;
	return c;
}

char Uart::peek() {

	if(rx_buff.tail==rx_buff.head)
		return RX_OVER;

	return rx_buff.c[rx_buff.tail];
}

// should change time out for waitting an incomming byte
char Uart::readTimeOut() {
	for (unsigned char i = 0; i < 200; i++) {
		char c = read();
		if(c!=RX_OVER)
			return c;
		__delay_cycles(10000);	// delay with SMCLK = 8Mhz
	}
	#if DEBUG_RX
	println("rx is empty");
	#endif
	return RX_OVER;
}

size_t Uart::readString(char *s) {
	char *p = s;
	while(1) {
		char c = readTimeOut();
		if(c==RX_OVER || c=='\0') break;
		*s++ = c;
	}
	*s = '\0';
	return s-p;
}

size_t Uart::readString(char *s, unsigned char size) {
	return readStringFromIndexTo(s, 0, size-1);
}

size_t Uart::readStringUntil(char *s, char delim) {
	char *p = s;
	while(1) {
		char c = readTimeOut();
		if(c==RX_OVER) {						// not found any more character
			memset(p, '\0', 1);
			return 0;
		}
		if(c==delim) break;				// break when delimiter has detected
		*s++ = c;
	}
	*s = '\0';
	return s-p;
}

size_t Uart::readStringFromTo(char *s, const char tail, const char head) {
	char *p = s;
	bool tail_flag = false;
	while(1) {
		char c = readTimeOut();
		if(c==RX_OVER)						// not found any more character
			break;
		if(tail_flag) {
			if(c==head) break;
			*s++ = c;
		}
		if(c==tail)
			tail_flag = true;
	}
	*s = '\0';
	return s-p;
}

size_t Uart::readStringFromIndexTo(char *s, unsigned char index, unsigned char pos) {
	char *p = s;
	unsigned char n = 0;
	while(1) {
		char c = readTimeOut();
		if(c==RX_OVER)	// not found any more character
			break;
		n++;
		if(n>pos+1) break;
		if( index<=n && n<=pos+1 )
			*s++ = c;
	}
	*s = '\0';
	return s-p;
}

size_t Uart::writeBytes(char *b, size_t size) {
	size_t n = 0;
	for (unsigned char i = 0; i < size; i++) {
		n += write(*b++);
		#if DEBUG_TX
		print("tx_dt[");print(i);print("]:");println(*(b-1), BIN);
		#endif
	}
	#if DEBUG_TX
	print("tx_bytes:");println(n);
	#endif
	return n;
}

size_t Uart::writeBytes(int *d, size_t size) {
	size_t n = 0;

	char *p = (char *) &d[0];								// must assign with &d[0]
	for (unsigned char i = 0; i < size; i++) {
		n += write(*p++);
		n += write(*p++);
		#if DEBUG_TX
		print("tx_dt[");print(i);print(".0]:");println(*(p-2), BIN);
		print("tx_dt[");print(i);print(".1]:");println(*(p-1), BIN);
		#endif
	}
	#if DEBUG_TX
	print("tx_bytes:");println(n/2);
	#endif
	return n/2;
}

size_t Uart::writeNumber(char num) {
	return write(num);
}

size_t Uart::writeNumber(unsigned char num) {
	return writeBytes((char *) &num, CHAR);
}

size_t Uart::writeNumber(int num) {
	return writeBytes((char *) &num, INT);
}

size_t Uart::writeNumber(unsigned int num) {
	return writeBytes((char *) &num, INT);
}

size_t Uart::writeNumber(long num) {
	return writeBytes((char *) &num, LONG);
}

size_t Uart::writeNumber(unsigned long num) {
	return writeBytes((char *) &num, LONG);
}

size_t Uart::writeNumber(float num) {
	return writeBytes((char *) &num, FLOAT);
}

size_t Uart::writeNumber(double num) {
	// must do this way to send double value.
	_data_float dt = {0,0};		// Need clear garbage value. there was problem with double in union
	dt.num = num;
	return writeBytes(dt.b, DOUBLE);
}

size_t Uart::readBytes(char *dt, size_t size) {
	size_t n = 0;
	for (unsigned char i = 0; i<size; i++) {
		char c = readTimeOut();
		if(c==RX_OVER) {
			#if DEBUG_RX
			println("rx is empty");
			#endif
			goto RETURN;
		}
		dt[i] = c;
		#if DEBUG_RX
		print("rx_dt[");print(i);print("]:");println(dt[i], BIN);
		#endif
		n++;
	}

RETURN:
	#if DEBUG_RX
	print("rx_bytes:");println(n);
	#endif
	return n;
}

size_t Uart::readBytes(int *dt, size_t size) {
	size_t n = 0;
	_data_hex dt_h;
	for (unsigned char i = 0; i<size; i++) {
		for (unsigned char j = 0; j < 2; j++) {
			char c = readTimeOut();
			if(c==RX_OVER) {
				#if DEBUG_RX
				println("rx is empty");
				#endif
				goto RETURN;
			}
			dt_h.b[j] = c;
			#if (DEBUG_RX==2)
			print("rx_dt[");print(i);print('.');print(j);print("]:");println(dt_h.b[j], BIN);
			#endif
		}
		dt[i] = dt_h.hex;
		#if DEBUG_RX
		print("rx_dt[");print(i);print("]:");println(dt[i], HEX);
		#endif
		n++;
	}

RETURN:
	#if DEBUG_RX
	print("rx_bytes:");println(n);
	#endif
	return n;
}

size_t Uart::readNumber(char &num) {
	char c = readTimeOut();
	if(c==RX_OVER)
		return 0;
	num = c;
	return 1;
}

size_t Uart::readNumber(unsigned char &num) {
	_data_int dt;
	size_t n = readBytes(dt.b, CHAR);
	num = (unsigned char)dt.num;
	return n;
}

size_t Uart::readNumber(int &num) {
	_data_int dt;
	dt.num = 0;
	size_t n = readBytes(dt.b, INT);
	num = (int)dt.num;
	return n;
}

size_t Uart::readNumber(unsigned int &num) {
	_data_int dt;
	dt.num = 0;
	size_t n = readBytes(dt.b, INT);
	num = (unsigned int)dt.num;
	return n;
}

size_t Uart::readNumber(long &num) {
	_data_int dt;
	dt.num = 0;
	size_t n = readBytes(dt.b, LONG);
	num = dt.num;
	return n;
}

size_t Uart::readNumber(unsigned long &num) {
	_data_int dt;
	dt.num = 0;
	size_t n = readBytes(dt.b, LONG);
	num = (unsigned long)dt.num;
	return n;
}

size_t Uart::readNumber(float &num) {
	_data_float dt;
	size_t n = readBytes(dt.b, FLOAT);
	num = (float)dt.num;
	return n;
}

size_t Uart::readNumber(double &num) {
	_data_float dt;
	dt.num = 0;
	size_t n = readBytes(dt.b, DOUBLE);
	num = dt.num;
	return n;
}

//////////////////////////////////////////
#pragma vector = USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void) {

	while (!(IFG2 & UCA0RXIFG));                   // wait unitl receive character
	if( ((BUFF_SIZE + rx_buff.head - rx_buff.tail) % BUFF_SIZE) < (BUFF_SIZE-1) ) {	// ingnore character when oververflow RX buffer
		rx_buff.c[rx_buff.head] = UCA0RXBUF;
		rx_buff.head = (rx_buff.head + 1) % BUFF_SIZE;
	}

	IFG2 &= ~UCA0RXIFG; // Clear RX flag
}
