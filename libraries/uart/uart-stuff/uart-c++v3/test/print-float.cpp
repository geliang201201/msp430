#include "msp430g2553.h"
#include "uart.h"

Uart uart(9600);

int  main() {

   WDTCTL = WDTPW + WDTHOLD; // Stop the Watch dog

   uart.print(num_float(123.123));
   return 0;
}
