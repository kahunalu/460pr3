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

int x_val       = 0;
int y_val       = 0;
int laser_val   = 0;

/* Create loop function which executes while scheduler sleeps
 *
 */
void loop(){
  while(1){};
}

void auto_move(){
  Event_Signal(Task_GetArg());
  Task_Terminate();
}

void avoid_move(){
  Event_Signal(Task_GetArg());
  Task_Terminate();
}


void drive_roomba(){
  // Execute move based on precident of available moves
  /*  
  if(abs(curr_servo_x - servo_x) >= 10){
    if((curr_servo_x - servo_x)>0){
      curr_servo_x -= 10;
      myservo.writeMicroseconds(curr_servo_x);
    }else{
      curr_servo_x += 10;
      myservo.writeMicroseconds(curr_servo_x);
    }
  }else{
    curr_servo_x = servo_x;
    myservo.writeMicroseconds(curr_servo_x);
  }
  */
  
  Event_Signal(Task_GetArg());
  Task_Terminate();
}

//
void write_laser(){
  Event_Signal(Task_GetArg());
  Task_Terminate();
}

void man_move(){

  /*
  for(;;) {
    char* curr = BT_UART_Recv();
    //RMB_UART_Send_String(curr);
  }
  */

  Event_Signal(Task_GetArg());
  Task_Terminate();
}

void packet_recv() {
  uart0_sendbyte(uart1_recvbyte());
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

  int avoid_move_eid  = Event_Init();
  int auto_move_eid   = Event_Init();
  int man_move_eid    = Event_Init();
  
  int drive_roomba_eid= Event_Init();
  int write_laser_eid = Event_Init();
  
  // Begin looping through
  for(;;){
    // Receive Packet
    Task_Create(packet_recv, 2, packet_recv_eid);
    Event_Wait(packet_recv_eid);

    // Detect available moves
    Task_Create(avoid_move, 2, avoid_move_eid);
    Task_Create(auto_move, 2, auto_move_eid);
    Task_Create(man_move, 2, man_move_eid);
    
    // Wait until all moves are considered
    Event_Wait(avoid_move_eid);
    Event_Wait(auto_move_eid);
    Event_Wait(man_move_eid);

    // Drive the roomba and write to the laser
    Task_Create(drive_roomba, 3, drive_roomba_eid);
    Task_Create(write_laser, 3, write_laser_eid);

    // Wait until the roomba has moved and shot the laser
    Event_Wait(drive_roomba_eid);
    Event_Wait(write_laser_eid);
    _delay_ms(1000);
  }

  Task_Terminate();
}

void roomba_init() {
  // Initialize BDC pin
  DDRC = 0x80;

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
