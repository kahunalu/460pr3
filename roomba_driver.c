#define BAUDR 19200
#define F_CPU 16000000UL
#include "roomba_driver.h"
#include "uart.h"
#include "os.h"
#include <util/delay.h>
#include <avr/io.h>
ROOMBA_BITRATE BR = ROOMBA_57600BPS;
#define BAUDRATE ((F_CPU)/(BAUDR*16UL)-1)

/* Wakes Roomba up from sleep by setting device detect to low for 500 ms */
void wake_up() {
	PORTC = 0xFF;
	DDRA = (1<<DDPIN);		//Set DDPIN to output (pin 22)
	PORTA |= (1<<DDPIN);
	_delay_ms(2000);
	
	PORTA &= ~(1<<DDPIN);	//Set pin to 0
	_delay_ms(100);

	PORTA |= (1<<DDPIN);
	_delay_ms(100);

	PORTA &= ~(1<<DDPIN);
	_delay_ms(100);

	PORTA |= (1<<DDPIN);
	_delay_ms(100);
	
	PORTA &= ~(1<<DDPIN);
	_delay_ms(100);
	
	PORTA |= (1<<DDPIN);
	_delay_ms(100);
	
	PORTA &= ~(1<<DDPIN);
	_delay_ms(100);

	PORTC = 0x00;
	_delay_ms(100);
}

void roomba_data_request(uint8_t request1, uint8_t request2) {
	Roomba_Send_Byte(QUERYLIST);
	Roomba_Send_Byte(2);
	Roomba_Send_Byte(request1);
	Roomba_Send_Byte(request2);
}

void roomba_init() {
	uart0_init();
	_delay_ms(100);

	uart1_init();
	_delay_ms(100);

	uart0_sendbyte(START);
	_delay_ms(1000);

	flash();

	/* Put Roomba into control (safe) mode */
	uart0_sendbyte(SAFE);

}