#define F_CPU 16000000UL
#define BAUD 19200
#define AVERAGE_RUN 10

#define DRIVE       137   // control wheels
#define MOTORS      138   // turn cleaning motors on or off
#define START       128   // Start serial command interface
#define SAFE        131   // Enter safe mode
#define DOCK        143   // force the Roomba to seek its dock.
#define QUERYLIST   149   // make a request for sensor data

#include "uart.h"
#include "adc.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "os.h"

volatile int servo_x   = 0;
volatile int servo_y   = 0;
volatile int laser_val = 0;

volatile int man_move_avail   = 0;

volatile int avoid_move_avail = 0;
volatile int wall_detected    = 0;
volatile int bump_detected    = 0;


volatile int auto_mode        = 0;
volatile int auto_move_count  = 0;

#define STRAIGHT  32768
#define FORWARD   250
#define BACKWARD  -250
#define WIDETURN  200
#define TIGHTTURN 1
#define STILL     0

void roomba_data_request(uint8_t request1, uint8_t request2) {
  uart0_sendbyte(QUERYLIST);
  uart0_sendbyte(2);
  uart0_sendbyte(request1);
  uart0_sendbyte(request2);
}

/* Create loop function which executes while scheduler sleeps
 *
 */
void loop(){
  while(1){};
}

void drive_roomba(int16_t velocity, int16_t radius) {
  uart0_sendbyte(DRIVE);
  uart0_sendbyte(velocity>>8);
  uart0_sendbyte(velocity);
  uart0_sendbyte(radius>>8);
  uart0_sendbyte(radius);
}

void auto_move(){
  if(auto_move_count < 10){
    drive_roomba(FORWARD, -TIGHTTURN);
  }else if(auto_move_count < 20){
    drive_roomba(FORWARD, STRAIGHT);
  }else if(auto_move_count < 30){
    drive_roomba(BACKWARD, -TIGHTTURN);
  }else if(auto_move_count < 40){
    drive_roomba(FORWARD, STRAIGHT);
  }else{
    auto_move_count = 1;
  }

  auto_move_count++;
}

void avoid_move(){
}

void man_move(){
  int radius    = 0;
  int velocity  = 0;
  
  if(servo_x > 700){
    velocity = FORWARD;
  }else if(servo_x < 300){
    velocity = BACKWARD;
  }

  if(servo_y > 700){
    if(!velocity){
      velocity = FORWARD;
      radius = -TIGHTTURN;
    }else{
      radius =  -WIDETURN;
    }
  }else if(servo_y < 300){
    if(!velocity){
      velocity = FORWARD;
      radius = TIGHTTURN;
    }else{
      radius =  WIDETURN;
    }
  }else{
    radius = STRAIGHT;
  }

  drive_roomba(velocity, radius);
}

void control_roomba(){
  if(avoid_move_avail){
    avoid_move();
    uart0_sendstr("Avoid move\n");
  }else if(man_move_avail){
    man_move();
    uart0_sendstr("man move\n");
  }else{
    auto_move();
    uart0_sendstr("auto move\n");
  }

  Event_Signal(Task_GetArg());
  Task_Terminate();
}

void collision_detect(){
  roomba_data_request(7, 13);
  
  bump_detected = uart0_recvbyte();
  _delay_ms(100);
  wall_detected = uart0_recvbyte();
  _delay_ms(100);

  if((bump_detected >= 1 && bump_detected <= 3) || wall_detected ){
    avoid_move_avail = 1;
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
  if(servo_x>300 && servo_x<700 && servo_y>300 && servo_y<700){
    man_move_avail = 0;
    auto_mode      = 1;
  }else{
    man_move_avail = 1;
    auto_mode      = 0;
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
  int packet_recv_eid     = Event_Init();
  int collision_detect_eid= Event_Init();
  int control_roomba_eid  = Event_Init();
  
  // Begin looping through
  for(;;){
    // Receive Packet
    Task_Create(packet_recv, 2, packet_recv_eid);
    
    //REMOVE COMMENTS FOR EXPERIMENTAL AVOIDANCE CODE
    //Task_Create(collision_detect, 2, collision_detect_eid);
    //Event_Wait(collision_detect_eid);
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
