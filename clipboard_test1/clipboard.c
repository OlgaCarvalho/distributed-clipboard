#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "clipboard.h"
#include "mylib.h"

/****************************************************************************
 * clipboard_connect()
 * 
 * This function is called by the application to interact
 * with the distributed clipboard. Its return will be used in all other 
 * functions as clipboard_id.
 * 
 * @param clipboard_dir - directory  where  the  local clipboard was launched
 * @return -1 if cannot be accessed or >0 in case of success. 
 ***************************************************************************/
int clipboard_connect(char * clipboard_dir){
   
    struct sockaddr_un server_addr;
    int fd, c;
    
    //OPEN SOCKET TO THE LOCAL CLIPBOARD
    fd=socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1){
	perror("socket: ");
	exit(-1);
    }
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, clipboard_dir);
    
    c=connect(fd, (const struct sockaddr *) &server_addr, sizeof(server_addr));
    if(c==-1){
	perror("connect");
	exit(-1);
    }
    return fd;
}


/***************************************************************************
 * clipboard_copy()
 * 
 * This function copies the data pointed by buf to a region on the local 
 * clipboard
 * 
 * @param clipboard_id - value  returned by clipboard_connect().
 * @param region - region the user wants to copy the data to [0 to 9].
 * @param buf- data that is to be copied to the local clipboard.
 * @param count - the length of the data pointed by buf.
 * @return 0 if error; >0 with the bytes copied
 ***************************************************************************/
int clipboard_copy(int clipboard_id, int region, void *buf, size_t count){
    struct Request header;
    ssize_t nbytes;
    char *ptr = malloc(sizeof(struct Request));
    Op operation=COPY;
    
    /*SEND REQUEST FOR COPY*/
    set_msg(&header,operation,region,count);
    if(!valid_request(header)) return 0;
    
    memcpy(ptr,&header,sizeof(struct Request));     
    write(clipboard_id,ptr,sizeof(struct Request));
    free(ptr);
    
    /*SEND DATA*/
    nbytes = write(clipboard_id,buf,count);
          
    return nbytes;
    	
}

/**************************************************************************
 * clipboard_paste()
 * 
 * This function copies from the system to the application the data in a 
 * certain region.
 * 
 * @param clipboard_id - value  returned by clipboard_connect(). 
 * @param region - region the user wants to paste data from [0 to 9].
 * @param buf -   pointer to where the data is to be copied to.
 * @param count - the length that the user wants to paste.
 * @return 0 if error; >0 with the bytes copied.
 **************************************************************************/
int clipboard_paste(int clipboard_id, int region, void *buf, size_t count){
    ssize_t nbytes;
    struct Request header;
    char *ptr = malloc(sizeof(struct Request));
    Op operation = PASTE;
        
    /*SEND REQUEST FOR PASTE*/
    set_msg(&header,operation,region,count);
    if(!valid_request(header)) return 0;
    
    memcpy(ptr,&header,sizeof(struct Request));     //converts the struct to a byte stream to be sent in write()
    write(clipboard_id,ptr,sizeof(struct Request));
    free(ptr);
    
    /*RECEIVE DATASTREAM*/
    nbytes = read(clipboard_id,buf,count);
          
    return nbytes;
    
}

/**************************************************************************
 * clipboard_wait()
 * 
 * This function waits for a change on a certain region, and when it happens
 * the new data in that region is copied to memory pointed by buf up to a 
 * length of count.
 * 
 * @param clipboard_id - value  returned by clipboard_connect(). 
 * @param region - region the user wants to wait on [0 to 9].
 * @param buf -   pointer where the data is to be copied to.
 * @param count - the length of the data to be copied.
 * @return 0 if error; >0 with the bytes copied.
 **************************************************************************/
int clipboard_wait(int clipboard_id, int region, void *buf, size_t count) {
    struct Request request;
    char *ptr = malloc(sizeof(struct Request));
    Op operation = WAIT;
    int nbytes;
    
    /*SEND REQUEST FOR WAIT*/
    set_msg(&request,operation,region,count);
    if(!valid_request(request)) return 0;
    
    memcpy(ptr,&request,sizeof(struct Request));     //converts the struct to a byte stream to be sent in write()
    write(clipboard_id,ptr,sizeof(struct Request));
    free(ptr);
        
    
    /*RECEIVE DATASTREAM*/
    nbytes = read(clipboard_id,buf,count);
    
    return nbytes;
    
    
}


/**************************************************************************
 * clipboard_close()
 * 
 * This function closes the connection between the application and the local
 * clipboard.
 * 
 * @param clipboard_id - value  returned by clipboard_connect(). 
 * @return void
 **************************************************************************/
void clipboard_close(int clipboard_id) {
    close(clipboard_id);
    return;
}

