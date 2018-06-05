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
    int fd;
     
    fd = clipboard_connect(SOCK_ADDRESS);
    if(fd== -1){
        printf("Error in connect.\n");
	exit(-1);
    }
       
 
    for(int i=0; i<5; i++) {
	printf(">copy 'hello1' to region 0\n");
        if(!clipboard_copy(fd,0,"hello1\0",7)) {
            printf("Error in clipboard_copy.\n");
            exit(-1);
        }
      
        sleep(2);

	printf(">copy 'hello1' to region 2\n");
        if(!clipboard_copy(fd,2,"hello1\0",7)) {
            printf("Error in clipboard_copy.\n");
            exit(-1);
        }

        sleep(2);

      
	printf(">copy 'hello1' to region 6\n");
    	if(!clipboard_copy(fd,6,"hello1\0",7)) {
           printf("Error in clipboard_copy.\n");
           exit(-1);
       	}

	sleep(2);
 
    }
      
    
    clipboard_close(fd);
    
    
    exit(0);
}