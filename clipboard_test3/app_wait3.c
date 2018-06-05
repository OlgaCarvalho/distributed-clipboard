#include "clipboard.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(){
    char data[100];
    char cmd[50]={0};
    int fd;
    
    
    
    fd = clipboard_connect(SOCK_ADDRESS);
    if(fd== -1){
        printf("Error in connect.\n");
	exit(-1);
    }
       
    
    for(int i=0; i<5; i++) {
	printf("wait for region 2 and paste 10 bytes\n");
        if(!clipboard_wait(fd,2,data,10)) {
           printf("Error in clipboard_wait.\n");
            exit(-1);
        }
	printf("[wait] %s\n",data);
	memset(data,0,strlen(data));

	sleep(2);
        
	printf("wait for region 3 and paste 10 bytes\n");
        if(!clipboard_wait(fd,3,data,10)) {
           printf("Error in clipboard_wait.\n");
            exit(-1);
        }
	printf("[wait] %s\n",data);
	memset(data,0,strlen(data));

	sleep(2);
    }
      
    
    clipboard_close(fd);
    
    
    exit(0);
}