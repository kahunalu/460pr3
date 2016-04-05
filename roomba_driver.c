#define BAUDR 19200
#define F_CPU 16000000UL
#include "roomba_driver.h"
#include <util/delay.h>
#include <avr/io.h>
ROOMBA_BITRATE BR = ROOMBA_57600BPS;
#define BAUDRATE ((F_CPU)/(BAUDR*16UL)-1)


/* Wakes Roomba up from sleep by setting device detect to low for 500 ms */
void wake_up() {
	DDRA = (1<<DDPIN);		//Set DDPIN to output (pin 22)
	PORTA |= (1<<DDPIN);
	_delay_ms(100);
	PORTA &= ~(1<<DDPIN);	//Set pin to 0
	_delay_ms(3000);			//Wait 2 s

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
	
}

void usart_init() {
	/*UBRR1H = (BAUDRATE>>8);
	UBRR1L = BAUDRATE;*/

	PRR1 &= ~(1<<PRUSART1);
	UBRR1 = 51;

	UCSR1A = (1<<TXC1);
	/* enable receive and transmit */
	UCSR1B |= (1<<RXEN1)|(1<<TXEN1);
	/* Set frame format: 8data data format */
	UCSR1C |= (1<<UCSZ11)|(1<<UCSZ10);
	UCSR1A &= ~(1<<U2X1);
}

void usart_write(unsigned char data) {
	while(!(UCSR1A & (1<<UDRE1)));		/* wait until the empty bit flag is set */
	UDR1 = data;
}

unsigned char usart_read() {
	while(!(UCSR1A) & (1<<RXC1));		/* wait until all data is received */
	return UDR1;
}

void roomba_init() {
	wake_up();

	usart_write(START);
	_delay_ms(200);

	/* Put Roomba into control (safe) mode */
	usart_write(SAFE);

}