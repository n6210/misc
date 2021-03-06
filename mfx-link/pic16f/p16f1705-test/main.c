/* ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <info@gerhard-bertelsmann.de> wrote this file. As long as you retain this
 * notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return
 * Gerhard Bertelsmann
 * ----------------------------------------------------------------------------
 */

#include <xc.h>
#include "main.h"
#include "i2c_lcd.h"
#include "lcd.h"
#include "utils.h"
#include "timings.h"

#pragma config FOSC=INTOSC, PLLEN=OFF, MCLRE=ON, WDTE=OFF
#pragma config LVP=ON, CLKOUTEN=OFF

struct serial_buffer tx_fifo, rx_fifo;

uint16_t pulse_high @ 0x2A0;
uint16_t pulse_low  @ 0x2A2;
uint8_t  rx_data @ 0x1A0;
uint8_t  tx_data @ 0x1A1;
volatile uint8_t timer0_counter;
volatile uint16_t adc_poti;
volatile uint16_t adc_sense;
volatile uint16_t pulse_t;
volatile uint8_t complete, c;

void interrupt ISR(void) {
    if (CCP1IF) {
	CCP1IF = 0;
	if (CCP1M0) {
	    CCP1M0 = 0;
	    CCPR1 += pulse_high;
	} else {
	    CCP1M0 = 1;
	    CCPR1 += pulse_low;
	}
    }
    if (CCP2IF) {
	CCP2IF = 0;
	if (CCP2M0) {
	    CCP2M0 = 0;
	    CCPR2 += pulse_low;
	} else {
	    CCP2M0 = 1;
	    CCPR2 += pulse_high;
	}
    }

    if (RCIF) {
	putc(RC1REG);
	if (RC1REG == 0x0d)
	    complete = 1;
    }

    if (TXIF) {
	tx_data=getc();
	if (tx_data) {
	    TX1REG = tx_data;
	} else {
	    TXIE = 0;
	}
    }

    //if (TMR0IF && TMR0IE) {
    if (TMR0IF) {
	TMR0IF = 0;
	TMR0 = TIMER0_VAL;

	timer0_counter++;
	/* kind of state machine
	   to sample two pins */
	if (timer0_counter & 0x02) {
	    if (timer0_counter & 0x01) {
		ADCON0 = (AD_POTI << 2) | 1;
		ADGO = 1;
	    /* check if ADC completed) */
	    } else if (!ADGO) {
		/* use sliding average */
		adc_poti = (adc_poti + ADRESL) >> 1;
	    }
	} else {
	    if (timer0_counter & 0x01) {
		ADCON0 = (AD_SENSE << 2) | 1;
		ADGO = 1;
	    /* check if ADC completed) */
	    } else if (!ADGO) {
		/* use sliding average */
		adc_sense = (adc_sense + ADRESL) >> 1;
	    }
	}
    }
}

/* RA4 SDA I2C
 * RA5 SCL I2C
 * RC1 RxD UART
 * RC2 TxD UART
 * RC3 Rail Signal
 * RB5 CCP1 RPWM
 * RB6 CCP2 LPWM
 * // RC3 CCP1
 */

void pps_init(void) {
    PPSLOCK = 0x55;
    PPSLOCK = 0xaa;
    PPSLOCK = 0;		// unlock PPS

    /* I2C */
    SSPCLKPPS = 0x05;
    RA5PPS = 0b10000;		// RA5 output SCL
    SSPDATPPS = 0x04;
    RA4PPS = 0b10001;		// RA4 output SDA
    /* USART */
    RXPPS = 0b10001;		// input  EUSART RX -> RC1
    LATC2 = 1;			// void first byte error
    RC2PPS = 0b10100;		// RC2 output TX/CK
    /* CLC */
    /* RB4PPS = 0b00100;	// LC1OUT -> ENABLE */
    /* COG */
    // COGINPPS = 0b10011;	// RC3 PULSE
    RC3PPS = 0b01101;		// CCP1 -> RPWM
    RC4PPS = 0b01100;		// CCP2 -> LPWM

    PPSLOCK = 0x55;
    PPSLOCK = 0xaa;
    PPSLOCK = 1;		// lock PPS
}

void system_init(void) {
    // switch off analog
    OSCCON = 0b11110000;	// Configure oscillator
    //1------- use PLL to get 4x8 Mhz (system clock)
    //-1110--- 8 MHz internal oscillator (instruction clock)
    //------00 oscillator selected with INTOSC
    ANSELA = 0;
    ANSELC = 0;
    ADCON0 = 0;
    ADCON1 = 0;
    ADCON2 = 0;
    CM1CON0 = 0;
    CM1CON1 = 0;
    CM2CON0 = 0;
    CM2CON1 = 0;

    /* debug */
    TRISA0 = 0;
    /* I2C MSSP 40001729B.pdf page 302 */
    TRISA4 = 1;
    TRISA5 = 1;
    /* USART */
    TRISC1 = 1;
    TRISC2 = 0;
    /* RA2&RC0 analog input */
    TRISA2 = 1;
    TRISC0 = 0;		/* enable */
    TRISC3 = 0;		/* CCP1 */
    TRISC4 = 0;		/* CCP2 */
    TRISC5 = 0;		/* LED */
    // setup interrupt events
    //clear all relevant interrupt flags
    SSP1IF = 0;
    TMR1IF = 0;
    CCP1IF = 0;
    CCP2IF = 0;
    //activate interrupt bits
    CCP1IE = 1;			// start
    CCP2IE = 1;			// start
    PEIE = 1;
    GIE = 1;
}

void i2c_init(void) {
    /* Initialise I2C MSSP Master */
    SSP1CON1 = 0b00101000;
    SSP1CON2 = 0x00;
    SSP1CON3 = 0x00;
    /* Master at 100KHz */
    /* clock = FOSC/(4 * (SSPADD + 1)) */
    SSP1ADD = SSP1ADD_VAL;
    /* Slew rate disabled */
    SSP1STAT = 0b11000000;
}

void ad_init(void) {
    /* RA2&RC0 analog */
    ;ANSELA = 1 << 2;
    ANSELA = 0;
    ;ANSELC = 1 << 0;
    ANSELC = 0;
    /* right justified ; FOSC/64 ;VREF- GND & VREF+ VDD */
    ADCON1 = 0b11100000;
}

void uart_init(void) {
    TX9 = 1;			// 8-bit transmission
    TX9D = 1;			//  one extra stop bit
    TXEN = 1;			// transmit enabled
    SYNC = 0;			// asynchronous mode
    BRGH = 1;			// high speed
    SPEN = 1;			// enable serial port (configures RX/DT and TX/CK pins as serial port pins)
    RX9 = 0;			// 8-bit reception
    CREN = 1;			// enable receiver
    BRG16 = USE_BRG16;		// 8-bit baud rate generator
    SPBRG = SBRG_VAL;		// calculated by defines
    RCIE = 1;
}

void timer0_init(void) {
    TMR0CS = 0;			// FOSC / 4
    PSA = 0;			// use prescaler
    PS0 = 0;			// prescaler 1:8
    TMR0 = TIMER0_VAL;
    TMR0IE = 1;
}

void timer1_init(void) {
    T1CON = 0b00110001;
          /*  00------ FOSC/4 as counting source
              --11---- prescaler 1:8 (counting every us)
              -------1 timer on */
    TMR1GE = 0;			// timer is not controlled by gate.
    TMR1H = 0;			// reset timer1 high
    TMR1L = 0;			// and low bytes - prescaler automatic reset
    CCP1CON = 0b00001000;	// set up compare mode
    CCP2CON = 0b00001000;	// set up compare
    //----1000   Compare mode: set output on compare match (set CCPxIF)
    // set CCPx register to the highest value to avoid useless interrupt
    CCPR1H = 0xFF;
    CCPR1L = 0xFF;
    CCPR2H = 0xFF;
    CCPR2L = 0xFF;
    // use it
    CCP1IE = 1;
    CCP2IE = 1;
}

void timer2_init(void) {
    // default (FOSC/4)
    T2CON = 0b00000100;
    //-----1-- timer on
    //------00 prescaler 1:1 (overflow every 32us)
    TMR2 = 0;			// reset timer2
}

void clc_init(void) {
    CLC1SEL0 = 0;
    CLC1SEL1 = 0;

    //LC1OE = 1;
    //LC1EN = 1;
}

void cog_init(void) {
    COG1CON0 = 0;
    COG1CON1 = 0;

    COG1RIS = 0b00010000;	/* CCP1 rising event	*/
    COG1RSIM = 0x0;		/* no rising delay	*/
    COG1FIS = 0b00010000;	/* CCP1 falling event	*/
    COG1FSIM = 0x0;		/* no falling delay	*/

    COG1ASD0 = 0;		/* don't use shutdown           */
    COG1ASD1 = 0;

    COG1STR = 0;		/* don't use steering control   */
    COG1DBR = 0;		/* no dead band                 */
    COG1DBF = 0;
    COG1BLKR = 0;		/* don't use blanking           */
    COG1BLKF = 0;
    COG1PHR = 0;		/* normal phase                 */
    COG1PHF = 0;

    COG1CON0 = 0b10001100;
              /* 1-------       G1EN COG1 Enable
                 -0------       G1LD0 no buffer
                 ---01---       G1CS Fosc clock source
                 -----100       G1MD COG Half Bridge mode */

}

/* Instructions per millisecond. */
#define INSNS_PER_MS    (XTAL_FREQ / 8000U)
/* Delay loop is about 10 cycles per iteration. */
#define LOOPS_PER_MS    (INSNS_PER_MS / 10U)
void delay_ms(uint16_t ms) {
    uint16_t u;
    while (ms--) {
	/* Inner loop takes about 10 cycles per iteration + 4 cycles setup. */
	for (u = 0; u < LOOPS_PER_MS; u++) {
	    /* Prevent this loop from being optimized away. */
	    asm("nop");
	}
    }
}

uint8_t ad(uint8_t channel) {
    ADCON0 = (channel << 2) | 1;
    ADGO = 1;
    delay_ms(1);
    while (ADGO) ;
    return (ADRESH);
}

void command_parser() {
    char c, cn;
    uint8_t tmp;

    if (c = getchar_fifo(&rx_fifo)) {
	switch (c) {
	case 'F':
	    putchar_fifo('*', &tx_fifo);
	    pulse_high = 13;
	    pulse_low = 13;
	    break;
	case 'M':
	    putchar_fifo('*', &tx_fifo);
	    pulse_high = 13;
	    pulse_low = 26;
	    break;
	case 'S':
	    putchar_fifo('*', &tx_fifo);
	    pulse_high = 26;
	    pulse_low = 26;
	    break;
	case 'T':
	    putchar_fifo('*', &tx_fifo);
	    pulse_high = hex_to_byte(&rx_fifo);
	    pulse_low =  hex_to_byte(&rx_fifo);
	    break;
	default:
	    break;
	}
	putchar_fifo(c, &tx_fifo);
	/* start USART transmit */
	TXIE = 1;
    }
}

void main(void) {
    uint8_t counter = 0;
    uint8_t temp, c;
    uint16_t ad_value;

    pps_init();
    system_init();
    uart_init();
    i2c_init();
    //ad_init();
    timer0_init();
    timer1_init();
    // cog_init();

    pulse_high = 50;
    pulse_low = 150;

    /* empty circular buffers */
    tx_fifo.head = 0;
    tx_fifo.tail = 0;
    rx_fifo.head = 0;
    rx_fifo.tail = 0;

    GIE = 1;
    LCD_init(LCD_01_ADDRESS);

    while (1) {
	/* command complete */
	if (complete) {
	    complete = 0;
	    //command_parser();
	    while ((c = getchar_fifo(&rx_fifo)) != 0)
	    	putchar_fifo(c, &tx_fifo);
	}
	if (timer0_counter == 0) {
	    // temp = ad(AD_SENSE);
	    /* 14mA per digit / atomic read */
	    ad_value = adc_sense * 14;
	    temp = adc_poti;
	    LCD_putcmd(LCD_01_ADDRESS, LCD_CLEAR, 1);
	    LCD_puts(LCD_01_ADDRESS, "Booster Max=8.0A\0");
	    LCD_goto(LCD_01_ADDRESS, 2, 1);
	    LCD_putch(LCD_01_ADDRESS, nibble_to_hex(temp >> 4));
	    LCD_putch(LCD_01_ADDRESS, nibble_to_hex(temp));
	    LCD_puts(LCD_01_ADDRESS, "  On      0.0%\0");
	    //LATCbits.LATC0 = 1;
	    //LATCbits.LATC0 ^= 1;
	    // LATA0 ^= 1;
	    //LATC5 ^= 1;
	}
    }
}
