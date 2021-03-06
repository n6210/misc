/* ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <info@gerhard-bertelsmann.de> wrote this file. As long as you retain this
 * notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return
 * Gerhard Bertelsmann
 * ----------------------------------------------------------------------------
 */

#include "main.h"
#include "can.h"
#include "usart.h"

#if 1
//_FOSCSEL(FNOSC_PRIPLL & IESO_OFF)
#pragma config FNOSC    = PRIPLL
#pragma config IESO     = OFF
// Enable Clock Switching and Configure POSC in XT mode
//_FOSC(FCKSM_CSECMD & OSCIOFNC_ON & POSCMD_EC)
#pragma config FCKSM    = CSECMD
#pragma config OSCIOFNC = ON
#pragma config POSCMD   = XT
#else
// Select Internal FRC at POR
_FOSCSEL(FNOSC_FRC & IESO_OFF)
// Enable Clock Switching and Configure Primary Oscillator in XT mode
_FOSC(FCKSM_CSECMD & OSCIOFNC_OFF & POSCMD_NONE)

_FWDT(FWDTEN_OFF)
#endif 

struct serial_buffer_t tx_fifo, rx_fifo;
unsigned int counter = 0;
unsigned int counter_can = 0;

void init_io(void) {
    /* ADC1 Digital Mode */
    // AD1CON1.bits = 0x8000;
    _ADON = 0;
    _CVR2OE = 0;
    _CVR1OE = 0;
    ANSELA = 0;
    ANSELB = 0;
    // AD1PCFG = 0xFFFF;

    /* RA0 LED */
    TRISAbits.TRISA0 = 0;
    LATAbits.LATA0 = 0;

    TRISBbits.TRISB2 = 1;	/* Pin 6  as input  pin for UART RxD */
    TRISBbits.TRISB3 = 0;	/* Pin 7  as output pin for UART TxD */
    TRISBbits.TRISB4 = 1;	/* Pin 11 as input  pin for CAN RxD */
    TRISAbits.TRISA4 = 0;	/* Pin 12 as output pin for CAN TxD */
}

void init_pps(void) {
    __builtin_write_OSCCONL(OSCCON & ~(1 << 6));
    RPINR18bits.U1RXR = 34;	/* U1RxD */
    RPOR0bits.RP35R   = 1;	/* U1TxD */
    RPINR26bits.C1RXR = 36;	/* PIN 11 RB4 RP36 CAN RxD */
    RPOR0bits.RP20R   = 14;	/* PIN 12 RA4 RP20 CAN TxD (C1TX = 0b001110) */
    __builtin_write_OSCCONL(OSCCON | (1 << 6));
}

int main(void) {
    /* Init Clock */
    PLLFBD = 126;
    CLKDIVbits.PLLPOST = 0;
    CLKDIVbits.PLLPRE = 1;

    __builtin_write_OSCCONH(0x03);
    __builtin_write_OSCCONL(OSCCON | 0x01);

    while (OSCCONbits.COSC != 3) ;
    while (OSCCONbits.LOCK != 1) ;

    tx_fifo.head=0;
    tx_fifo.tail=0;
    rx_fifo.head=0;
    rx_fifo.tail=0;

    /* Init PORT I/O */
    init_io();
    init_pps();
    init_uart();
    init_can();

    while (true) {

	counter++;
	counter_can++;
	fifo_putchar(&tx_fifo);
	if (can_test_receive())
	    print_rom_fifo("received CAN packet\r\n", &tx_fifo);
	if (counter_can == 30000) {
	    can_test_send();
	    counter_can = 0;
	}
	if (counter == 50000) {
	    print_rom_fifo("Hello dsPIC33 !\r\n", &tx_fifo);
	    //print_debug_fifo(&tx_fifo);
	    // ClrWdt();
	    counter = 0;
	}
	__delay_us(10);

	__builtin_btg((unsigned int *)&LATA, 0);
	__builtin_btg((unsigned int *)&LATA, 0);
	__builtin_btg((unsigned int *)&LATA, 0);
	__builtin_btg((unsigned int *)&LATA, 0);
	__builtin_btg((unsigned int *)&LATA, 0);
	__builtin_btg((unsigned int *)&LATA, 0);
	__builtin_btg((unsigned int *)&LATA, 0);
	__builtin_btg((unsigned int *)&LATA, 0);
    }
}
