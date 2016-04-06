#include <avr/io.h>
#include "LED_Test.h"

void init_LED_PORTL_pin0(void) {
    DDRL |= _BV(DDL0);
}

void init_LED_PORTL_pin1(void) {
    DDRL |= _BV(DDL1);
}

void init_LED_PORTL_pin2(void) {
    DDRL |= _BV(DDL2);
}

void init_LED_PORTL_pin5(void) {
    DDRL |= _BV(DDL5);
}

void init_LED_PORTL_pin6(void) {
    DDRL |= _BV(DDL6);
}

void init_LED_PORTL_pin7(void) {
    DDRL |= _BV(DDL7);
}

void enable_LED(unsigned int mask) {
    PORTL |= _BV(mask);
}

void disable_LED(unsigned int mask) {
    PORTL &= ~_BV(mask);
}

void toggle_LED(unsigned int mask) {
    PORTL ^= _BV(mask);
}