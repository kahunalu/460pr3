#define AVERAGE_RUN 10



#include "os.h"
#include <avr/io.h>
#include <stdio.h>
#include "adc.h"
#include "uart.h"
#include <string.h>

int poll_count  = 0;
int sensor_pin  = 0; 


/* initialize averages to the joystick's center value = 131 */
uint8_t VRx_avg[10] = {131,131,131,131,131,131,131,131,131,131};
/* initialize sums */
int x_sum = 1310;
uint8_t VRy_avg[10] = {131,131,131,131,131,131,131,131,131,131};
int y_sum = 1310;

uint8_t x     = 0;
uint8_t y     = 0;
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
  int event = Task_GetArg();

    PORTB |= (1<<PB7);
    x = (readadc(2)>>2);
    if(x == 255) {
      x == 254;
    }
    //x = smooth_read(2, VRx_avg, &x_sum);
    y = (readadc(3)>>2);
    if(y == 255) {
      y == 254;
    }
    //y = smooth_read(3, VRy_avg, &y_sum);
    laser_val = (readadc(4)>>2);

    PORTB &= ~(1<<PB7);
     
  Event_Signal(event);

}

void write_bt(){
  int event = Task_GetArg();
  PORTB |= (1<<PB4);
  BT_UART_Send_Byte(0xFF);
  BT_UART_Send_Byte(x);
  BT_UART_Send_Byte(y);
  BT_UART_Send_Byte(laser_val);
  BT_UART_Send_Byte(0xFF);

  
  PORTB &= ~(1<<PB4);

  Event_Signal(event);


}

void action(){
  int read_joystick_eid = Event_Init();
  int write_bt_eid      = Event_Init();

  for(;;){
    
    
    
    Task_Create(read_joystick, 2, read_joystick_eid);
    PORTB |= (1<<PB6);
    Event_Wait(read_joystick_eid);
    PORTB &= ~(1<<PB6);

    Task_Create(write_bt, 3, write_bt_eid);
    PORTB |= (1<<PB5);
    Event_Wait(write_bt_eid);
    PORTB &= ~(1<<PB5);


    PORTC = 0x0F;
    Task_Sleep(20); // sleep for 200 ms (20*MSPERTICK) = 200
    PORTC = 0x00;
    Task_Sleep(20); // sleep for 200 ms
  }
}

void loop(){
  for(;;);
}

void a_main(){
  DDRC    = 0x0F;
  DDRB |= (1<<PB7)|(1<<PB6)|(1<<PB5)|(1<<PB4);
  DDRA |= (1<<PA0)|(1<<PA1);
  
  InitADC();
  BTBase_UART_Init();

  Task_Create(action, 1, 0);
  Task_Create(loop, 8, 0);
  Task_Terminate();
}
