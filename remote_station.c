#define F_CPU 16000000UL
#define BAUD 19200
#define AVERAGE_RUN 10

#include "uart.h"
//#include "adc.h"
//#include "roomba_driver.h"
#include <avr/io.h>
#include <stdio.h>
#include "os.h"

extern int bt_bytes;
extern int bt_recv_eid;

int laser_pin   = 13;

int x     = 0;
int y     = 0;
int laser_val   = 0;

void auto_move(){
  int event = Task_GetArg();
  Event_Signal(event);
}

void avoid_move(){
  int event = Task_GetArg();
  Event_Signal(event);
}


/*void write_servo(){
  int event = Task_GetArg();
  
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
  
  Event_Signal(event);
}
*/

/*void map_movement() {

}*/


void write_laser(){
  int event = Task_GetArg();

  if(!laser_val) {
    /* if the button was pressed shoot the laser */
  }
  Event_Signal(event);
}

void man_move(){
  int event = Task_GetArg();
  bt_recv_eid     = Event_Init();
  
  for(;;) {
    /* wait for next bluetooth packet */
    Event_Wait(bt_recv_eid);
    PORTC = 0x0F;
    Task_Sleep(20); // sleep for 0.2 seconds


    char* curr = BT_UART_Recv();
    //RMB_UART_Send_String(curr);
    PORTC = 0x00;
    Task_Sleep(20); // sleep for 0.2 seconds
    
    Event_Signal(event);
  }
  
}

void read_bt() {
  int event       = Task_GetArg();
  bt_recv_eid     = Event_Init();
  int read_index = 0;
  int bytes_read = 0;
  uint8_t *buff, curr;
  uint8_t packet[10];
  
  for(;;) {

    /* wait for next byte*/

    Event_Wait(bt_recv_eid);
    buff = BT_UART_Recv(); 

    while(read_index != bt_bytes) {

      curr = buff[read_index];

      if(bytes_read == 0 && curr == 255) {
        bytes_read++;

      } else if(bytes_read != 0 && curr == 255) {

        bytes_read = 0;
        /* signal that packet is read */
        Event_Signal(event);    

      } else if(bytes_read != 0 && curr != 255) {

        if(bytes_read == 1) x = curr;
        if(bytes_read == 2) y = curr;
        if(bytes_read == 3) laser_val = curr;
        bytes_read++;

      }

      read_index = (read_index + 1) % UART_BUFFER_SIZE;
    }


    
  }

}

/*
 * action
 * Handles creating the tasks and scheduling the output
 *
 */
void action(){
  //int avoid_move_eid  = Event_Init();
  int man_move_eid    = Event_Init();
  int packet_recv_eid = Event_Init();
  int mapped_eid = Event_Init();
  
  int write_laser_eid = Event_Init();
  //int write_servo_eid = Event_Init();

  //Task_Create(read_bt, 2, packet_recv_eid);

  for(;;){
    //Task_Create(avoid_move, 2, avoid_move_eid);
    
    //Event_Wait(avoid_move_eid);
    //Event_Wait(packet_recv_eid);
    Event_Wait(bt_recv_eid);


    /*Task_Create(map_movement, 2, mapped_eid);
    Event_Wait(mapped_eid);
    Task_Create(man_move_eid, 2, man_move_eid);
    Event_Wait(man_move_eid);*/

    //Task_Create(write_servo, 3, write_servo_eid);
    //Task_Create(write_laser, 3, write_laser_eid);
  }
}


/* Create loop function which executes while scheduler sleeps
 *
 */
void loop(){
  for(;;);
}


/*  a_main
 * 
 *    Applications main function which initializes pins, and tasks
 */
void a_main(){
  //RMB_UART_Init();
  BTRemote_UART_Init(); 

  roomba_init(); 
  
  DDRC = 0x0F;

  Task_Create(action, 1, 0);
  Task_Create(loop, 8, 0);  

  Task_Terminate();
}