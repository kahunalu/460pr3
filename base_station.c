#define AVERAGE_RUN 10
#define F_CPU 16000000UL

#include "os.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "adc.h"
#include "uart.h"
#include <string.h>

volatile int poll_count  = 0;

volatile int VRx_avg[10] = {0,0,0,0,0,0,0,0,0,0};
volatile int VRy_avg[10] = {0,0,0,0,0,0,0,0,0,0};
volatile int LS_avg[10]  = {0,0,0,0,0,0,0,0,0,0};
 
volatile int servo_x       = 2;
volatile int servo_y       = 3;
volatile int laser_val     = 4;


int read(int pin, int avg[]) {
  avg[poll_count] = readadc(pin);
  int sum = 0;
  int i   = 0;
  for(i = 0; i < AVERAGE_RUN; i++){
    sum += avg[i];
  }
  return (sum/AVERAGE_RUN);
}


void read_joystick(){
  servo_x = readadc(0);
  servo_y = readadc(1);
  laser_val = readadc(2);
 
  poll_count++;
  if(poll_count == AVERAGE_RUN){
    poll_count = 0;
  }

  Event_Signal(Task_GetArg());
  Task_Terminate();
}


void write_bt(){
  char servo_x_str[10];
  char servo_y_str[10];
  char laser_val_str[10];

  // Weird issue where servo x is garbage when first read in, thus read in twice
  sprintf(servo_x_str, "%d", servo_x);
  sprintf(servo_x_str, "%d", servo_x);
  sprintf(servo_y_str, "%d", servo_y);
  sprintf(laser_val_str, "%d", laser_val);

  // Send formatted packet
  uart1_sendbyte('#');
  uart1_sendstr(&servo_x_str[0]);
  uart1_sendbyte(0x00);
  uart1_sendstr(&servo_y_str[0]);
  uart1_sendbyte(0x00);
  uart1_sendstr(&laser_val_str[0]);
  uart1_sendbyte('#');

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
    _delay_ms(200);
  }
}

void a_main(){
  InitADC();

  // Initialize Uart 1 which is used for bluetooth
  uart0_init();
  _delay_ms(100);  

  uart1_init();
  _delay_ms(100);  

  Task_Create(action, 1, 0);
  Task_Terminate();
}
