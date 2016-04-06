#define F_CPU 16000000UL
#define BAUD 19200
#define AVERAGE_RUN 10

#define DRIVE   137   // control wheels
#define MOTORS  138   // turn cleaning motors on or off
#define START   128   // Start serial command interface
#define SAFE    131   // Enter safe mode
#define DOCK    143   // force the Roomba to seek its dock.

#include "uart.h"
#include "adc.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "os.h"

volatile int servo_x   = 0;
volatile int servo_y   = 0;
volatile int laser_val = 0;

volatile int auto_mode        = 0;
volatile int man_move_avail   = 0;
volatile int avoid_move_avail = 0;

/* Create loop function which executes while scheduler sleeps
 *
 */
void loop(){
  while(1){};
}

void control_roomba(int velocity, int radius) {
  write_serial(DRIVE);
  write_serial(HIGH_BYTE(velocity));
  write_serial(LOW_BYTE(velocity));
  write_serial(HIGH_BYTE(radius));
  write_serial(LOW_BYTE(radius));
}

void auto_move(){

}

void avoid_move(){

}

void man_move(){

}

void control_roomba(){
  if(avoid_move_avail){
    avoid_move();
  }else if(man_move_avail){
    man_move();
  }else{
    auto_move();
  }

  Event_Signal(Task_GetArg());
  Task_Terminate();
}

void packet_recv() {
  char values[50];
  char curr;
  volatile int bytes = -1;

  for(;;){  
    curr = uart1_recvbyte();

    if(bytes == -1 && curr=='#'){
      bytes = bytes + 1;
    }else if(bytes != -1 && curr!='#'){
      values[bytes] = curr;
      bytes = bytes + 1;
    }else if(bytes != -1 && curr=='#'){
      values[bytes] = '\0';
      break;
    }    
  }

  char *token;
  const char s[2] = "-";

  // Tokenize and convert values to corresponding values
  token = strtok(values, s);
  servo_x = atoi(token);
  
  token = strtok(NULL, s);
  servo_y = atoi(token);

  token = strtok(NULL, s);
  laser_val = atoi(token);

  // If the value is greater than a range 
  if(servo_x<300||servo_x<700||servo_y<300||servo_y>700){
    man_move_avail = 1;
    auto_mode      = 0;
  }else{
    man_move_avail = 0;
    auto_mode      = 1;
  }

  // Fire laser if signaled
  if(laser_val<100){
    PORTC |= 0x40;
  }else{
    PORTC &= 0x80;
  }
  
  Event_Signal(Task_GetArg());
  Task_Terminate();
}

/*
 * action
 * Handles creating the tasks and scheduling the output
 *
 */
void action(){
  // Create the events which correspond with each task
  int packet_recv_eid = Event_Init();
  int control_roomba_eid= Event_Init();
  
  // Begin looping through
  for(;;){
    // Receive Packet
    Task_Create(packet_recv, 2, packet_recv_eid);
    Event_Wait(packet_recv_eid);

    // Drive the roomba and write to the laser
    Task_Create(control_roomba, 2, control_roomba_eid);
    Event_Wait(control_roomba_eid);

    _delay_ms(100);
  }

  Task_Terminate();
}

void roomba_init() {
  // Initialize BDC pin
  DDRC = 0xC0;

  // Flash the BDC pin 3 times to set the Baud rate to 19200
  PORTC = 0x80;
  _delay_ms(2500);

  PORTC = 0x00;
  _delay_ms(300);

  PORTC = 0x80;
  _delay_ms(300);

  PORTC = 0x00;
  _delay_ms(300);

  PORTC = 0x80;
  _delay_ms(300);

  PORTC = 0x00;
  _delay_ms(300);

  PORTC = 0x80;

  // Send the start command to the roomba
  uart0_sendbyte(START);
  _delay_ms(200);
  
  // Enter the safe mode
  uart0_sendbyte(SAFE);
}

/*  a_main
 * 
 *    Applications main function which initializes pins, and tasks
 */
void a_main(){  
  // Initialize Uart 0 which is used for the roomba
  uart0_init();
  _delay_ms(100);

  // Initialize Uart 1 which is used for bluetooth
  uart1_init();
  _delay_ms(100);

  // Initialize the Roomba connection
  roomba_init();

  Task_Create(action, 1, 0);

  Task_Terminate();
}
