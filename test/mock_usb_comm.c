#include "usb_comm.h"

#define BUFFER_SIZE 256 * 2

static char lineBuffer[BUFFER_SIZE];


void usb_init(unsigned int bits, unsigned int parity, unsigned int stopBits, unsigned int baud){}

void usb_flush(void){}

char usb_getcharWait(size_t delay){
	return 0;
}

char usb_getchar(void){
	return usb_getcharWait(0);
}

int usb_readLine(char *s, int len)
{
	return usb_readLineWait(s,len,0);
}

int usb_readLineWait(char *s, int len, size_t delay)
{
	return 0;
}

void usb_puts(const char *s){

}

void usb_putchar(char c){

}

void onUSBCommTask(void *pvParameters) {
}


