#define F_CPU 16000000UL
#define BAUD 19200
#include "uart.h"
#include "os.h"

void a_main(){
	RMB_UART_Init();
	for(;;){
		RMB_UART_Send_String("Hello Roomba\n");
	}
}