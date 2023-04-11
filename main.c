/*
	MIT License

	Copyright (c) 2023 Truong Hy

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:
	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.

	Version: 20230319
	Target: Arm Cortex A9 on the DE10-Nano development board

	A basic "Hello world!" bare-metal C program for the DE10-Nano
	development board.  It is for the SoC (aka HPS) part of the FPGA so it
	should work on other Cyclone V SoC-FPGA dev boards, e.g.: DE1-SoC, DE-10
	Standard, Arrow SoCKit, etc.

	Uses the Intel/Altera HWLIB bare-metal library for many of the low level
	hardware access, and these are identified by their naming prefix "alt_" or
	"ALT_".
*/

#include "alt_16550_uart.h"
#include "alt_uart.h"
#include "hps.h"
#include "socal.h"
#include "tru_logger.h"
#include <string.h>

#ifdef SEMIHOSTING
	extern void initialise_monitor_handles(void);  // Reference function header from the external Semihosting library
#endif

/*
	Blocking wait on the transmit empty register to become empty.  It becomes
	empty when all pending data in the FIFO (FIFO mode) or holding register
	(non-FIFO mode) is transmitted.  This ensures all pending data has gone out.
*/
void hps_uart_flush_tx_wait(ALT_16550_HANDLE_t *handle){
	// Wait while not empty. Bit 6 of LSR register (TEMT bit): 0 = not empty, 1 = empty
	switch(handle->device){
		case ALT_16550_DEVICE_SOCFPGA_UART0: while((alt_read_word(ALT_UART_LSR_ADDR(ALT_UART0_ADDR)) & 0x00000040) == 0); break;
		case ALT_16550_DEVICE_SOCFPGA_UART1: while((alt_read_word(ALT_UART_LSR_ADDR(ALT_UART1_ADDR)) & 0x00000040) == 0); break;
		default: while((alt_read_word(ALT_UART_LSR_ADDR(handle->location)) & 0x00000040) == 0);
	}
}

/*
	Setup the UART. For UART0 most of this is already setup by the U-Boot-SPL,
	however, for HWLIB we need to enable the FIFO trigger (threshold) levels
	because it doesn't work without them set.  The actual problem is that
	U-Boot-SPL sets up UART0 in non-threshold FIFO mode and HWLIB does not
	support that mode.  See my newlib_ext.c, _write() function to learn how to
	support both modes.
*/
void hps_uart_setup(ALT_16550_HANDLE_t *handle){
	handle->device = ALT_16550_DEVICE_SOCFPGA_UART0;  // Select HPS UART0
	handle->location   = 0;  // This will be filled in by the init function, just default to 0
	handle->clock_freq = 0;  // This will be filled in by the init function, just default to 0

	alt_16550_init(handle->device, handle->location, handle->clock_freq, handle);  // Initialize the UART
	alt_16550_line_config_set(handle, ALT_16550_DATABITS_8, ALT_16550_PARITY_DISABLE, ALT_16550_STOPBITS_1);  // Configure for 8-N-1. This is not really needed as the default configuration is 8-N-1
	alt_16550_baudrate_set(handle, ALT_16550_BAUDRATE_115200);
	alt_16550_fifo_enable(handle);
	alt_16550_fifo_trigger_set_rx(handle, ALT_16550_FIFO_TRIGGER_RX_HALF_FULL);
	alt_16550_fifo_trigger_set_tx(handle, ALT_16550_FIFO_TRIGGER_TX_QUARTER_FULL);
	alt_16550_enable(handle);  // Enable the UART
}

void hps_uart_write_hello(ALT_16550_HANDLE_t *handle){
	char message[] = "Hello, World!\r\n";
	alt_16550_fifo_write_safe(handle, message, strlen(message), true);  // Test transmit using HWLIB function
}

void hps_uart_test(ALT_16550_HANDLE_t *handle){
	DEBUG_PRINTF("DEBUG: Setting up UART"_NL);  // Macro _NL contains the correct line ending

	hps_uart_flush_tx_wait(handle);  // Optional: If there were UART transmissions earlier then we should flush and wait first, before we re-setup the UART
	hps_uart_setup(handle);          // Setup the UART controller
	hps_uart_write_hello(handle);    // Once the UART is set up, we can use the HWLIB alt_16550_fifo_write or alt_16550_fifo_write_safe functions to transmit messages
}

void wait_forever(){
	DEBUG_PRINTF("DEBUG: Starting infinity loop"_NL);

	volatile unsigned char i = 1;
	while(i);
}

int main(int argc, char **argv){
	#ifdef SEMIHOSTING
		initialise_monitor_handles();  // Initialise Semihosting
	#endif

	ALT_16550_HANDLE_t handle;  // HWLIB UART handle
	hps_uart_test(&handle);

	wait_forever();

	return 0;
}
