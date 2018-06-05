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
       
 
    
   for(int i=0;i<5;i++) {
        printf("paste 10bytes from region 2\n");
        if(!clipboard_paste(fd,2,data,10)) {
            printf("Error in clipboard_paste.\n");
            exit(-1);
        }
	printf("[paste] %s\n",data);
	memset(data,0,strlen(data));

	sleep(2);
        
	printf("paste 10bytes from region 3\n");
        if(!clipboard_paste(fd,3,data,10)) {
            printf("Error in clipboard_paste.\n");
            exit(-1);
        }
	printf("[paste] %s\n",data);
	memset(data,0,strlen(data));

	sleep(2);

	printf("paste 10bytes from region 6\n");
        if(!clipboard_paste(fd,6,data,10)) {
            printf("Error in clipboard_paste.\n");
            exit(-1);
        }
	printf("[paste] %s\n",data);
	memset(data,0,strlen(data));

	sleep(2);    
    }
      
    
    clipboard_close(fd);
    
    
    exit(0);
}