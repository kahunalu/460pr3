#ifndef USB_UART_H_
#define UART_H_
 
#include <stdint.h>

#define UART_BUFFER_SIZE 100			// size of Rx ring buffer.
volatile uint8_t rmb_rx; 				// Flag to indicate uart received a byte
volatile uint8_t bt_rx; 				// Flag to indicate uart received a byte


void RMB_UART_Init(void);
void BTRemote_UART_Init(void);
void BTBase_UART_Init(void);

void RMB_UART_Send_Byte(uint8_t);
void BT_UART_Send_Byte(uint8_t);

char* RMB_UART_Recv(void);
char* BT_UART_Recv(void);

void RMB_UART_Send_String(char*);
void BT_UART_Send_String(char*);
 
#endif /* USB_UART_H_ */
