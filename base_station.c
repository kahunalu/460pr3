#define AVERAGE_RUN 10
#define F_CPU 16000000UL

#include "os.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "adc.h"
#include "uart.h"
#include <string.h>

int poll_count  = 0;
int sensor_pin  = 0; 

uint8_t VRx_avg[10] = {131,131,131,131,131,131,131,131,131,131};
uint8_t VRy_avg[10] = {131,131,131,131,131,131,131,131,131,131};
int x_sum = 1310;
int y_sum = 1310;

uint8_t x_val       = 0;
uint8_t y_val       = 0;
uint8_t laser_val   = 0;

uint8_t smooth_read(int pin, uint8_t *avg, int *sum) {
  int smoothed_val = 0;
  *sum = *sum - avg[poll_count];

  /* shift 10bit reading right twice to get an 8-bit reading for UART communication */
  avg[poll_count] = (readadc(pin)>>2);
  *sum = *sum + avg[poll_count];
  /* Then divide by 10 */
  if(*sum>0) {
    smoothed_val = *sum/10;
  }

  poll_count++;

  if(poll_count == AVERAGE_RUN){
    poll_count = 0;
  }

  return (uint8_t)smoothed_val;
}

void read_joystick(){
  // PORTB |= (1<<PB7);
  // x = (readadc(2)>>2);
  // if(x == 255) {
  //   x == 254;
  // }
  // //x = smooth_read(2, VRx_avg, &x_sum);
  // y = (readadc(3)>>2);
  // if(y == 255) {
  //   y == 254;
  // }
  // //y = smooth_read(3, VRy_avg, &y_sum);
  // laser_val = (readadc(4)>>2);

  // PORTB &= ~(1<<PB7);
     
  Event_Signal(Task_GetArg());
  Task_Terminate();
}

void write_bt(){
  uart1_sendbyte((char)1);
  Event_Signal(Task_GetArg());
  Task_Terminate();
}

void action(){
  int read_joystick_eid = Event_Init();
  int write_bt_eid      = Event_Init();

  for(;;){
    Task_Create(read_joystick, 2, read_joystick_eid);
    Event_Wait(read_joystick_eid);

    Task_Create(write_bt, 2, write_bt_eid);
    Event_Wait(write_bt_eid);
    _delay_ms(1000);
  }
}

void a_main(){
  InitADC();

  // Initialize Uart 1 which is used for bluetooth
  uart1_init();
  _delay_ms(100);  

  Task_Create(action, 1, 0);
  Task_Terminate();
}
