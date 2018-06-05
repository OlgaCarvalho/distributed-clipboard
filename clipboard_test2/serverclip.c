#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <linux/unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "clipboard.h"
#include "mylib.h"

#define max(A,B) ((A)>=(B)?(A):(B))


struct Region regions[10];

Clipboard_mode mode;    //SINGLE or CONNECTED

int tofd;               //parent descriptor
List * fdlist;          //list with children clipboards descriptors
ListT * threadlist;     //list with active threads

pthread_rwlock_t lock_rw[10];
pthread_rwlock_t lock_list = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lock_threadlist = PTHREAD_RWLOCK_INITIALIZER;

pthread_cond_t change[10];
pthread_mutex_t mux[10];

pthread_t threadapp, threadclip, threadremote;


void ctrl_c_callback_handler(int signum){
    printf("\nCaught signal Ctr-C\n");
    unlink(SOCK_ADDRESS);       //UNLINK FROM THE FILESYSTEM FOR REUSE

    
    /*CANCEL ACTIVE THREADS*/
    pthread_cancel(threadapp);
    pthread_cancel(threadclip);
    if(mode==CONNECTED) pthread_cancel(threadremote);
    pthread_join(threadapp, NULL); 
    pthread_join(threadclip, NULL); 
    if(mode==CONNECTED) pthread_join(threadremote, NULL);
    
    NodeT * current = threadlist->head;
    if(threadlist->head != NULL) {
        pthread_rwlock_rdlock(&lock_threadlist);
            //go trough the linked list
            while(current->next != NULL){
                pthread_cancel(current->tid);
                pthread_join(current->tid, NULL);
                current = current->next;
            }
            //last node
            pthread_cancel(current->tid);
            pthread_join(current->tid, NULL);
        pthread_rwlock_unlock(&lock_threadlist);
    }
    
    /*Free regions pointer*/
    for(int i=0; i<10; i++) {
        free(regions[i].data);
    }
    destroy_fdlist(fdlist);
    destroy_tlist(threadlist);
        
    exit(0);
}

void * thread_app_handler(void * fd){
    int clientfd=*(int*)fd;
    struct Request rcvmsg;
    char data[sizeof(struct Request)];
    size_t regionlen;
    
       
    
    /*ADD THREAD TO ACTIVE THREADS LIST*/
    pthread_t tid = pthread_self();
    pthread_rwlock_wrlock(&lock_threadlist);
    add_to_tlist(tid, threadlist);
    pthread_rwlock_unlock(&lock_threadlist);

    

    /*READ REQUESTS FROM CLIENT*/
    while(1) {
        if(read(clientfd, data, sizeof(struct Request))>0) {
            memcpy(&rcvmsg,data,sizeof(struct Request));
 
            /*COPY FROM CLIENT*/
            if(rcvmsg.operation==COPY) {
                /*RECEIVE DATA*/
                char *buffer_c = malloc((rcvmsg.datalen*sizeof(char)));
                if(read(clientfd, buffer_c, rcvmsg.datalen)<0){
                    perror("error reading request from client:");
                    free(buffer_c);
                    continue;
                }

                /*SAVE DATA TO CLIPBOARD*/
                pthread_rwlock_wrlock(&lock_rw[rcvmsg.region]);
                free(regions[rcvmsg.region].data);
                regions[rcvmsg.region].data=buffer_c;
                regions[rcvmsg.region].length=rcvmsg.datalen;
                pthread_rwlock_unlock(&lock_rw[rcvmsg.region]);


                /*SIGNAL APPS WAITING FOR A CHANGE*/
                pthread_mutex_lock(&mux[rcvmsg.region]);
                pthread_cond_broadcast(&change[rcvmsg.region]);
                pthread_mutex_unlock(&mux[rcvmsg.region]);

		
		/*PRINT REGIONS*/
		printf("\n\n>Regions:\n");
                for(int i=0; i<10; i++) {
                    pthread_rwlock_rdlock(&lock_rw[i]);
                    printf("   Region[%d]: %s\n",i,regions[i].data);
                    pthread_rwlock_unlock(&lock_rw[i]);
                }


                /*REPLICATE COPY TO CHILDREN*/
                Node * current = fdlist->head;
                if(fdlist->head != NULL) {
			printf("\n>replicate copy() to children\n");
                    char *ptr = copy_request(rcvmsg.region,rcvmsg.datalen);
                   
                    pthread_rwlock_rdlock(&lock_list);

                        while(current->next != NULL){
                            if(write(current->fd,ptr,sizeof(struct Request))<0) {            //send copy request
                                perror("error replicating copy to child:");
                            }             
                            else if(write(current->fd,buffer_c,rcvmsg.datalen)<0){               //send the data
                                perror("error replicating copy to child:");
                            }                
                            current = current->next;
                        }

                        
                        //last node
                        if(write(current->fd,ptr,sizeof(struct Request))<0) {            //send copy request
                                perror("error replicating copy to child:");
                        }             
                        else if(write(current->fd,buffer_c,rcvmsg.datalen)<0){               //send the data
                                perror("error replicating copy to child:");
                        }     
                       

                    pthread_rwlock_unlock(&lock_list);
                    free(ptr);
                }
                

                /*REPLICATE COPY TO PARENT*/
                if(mode==CONNECTED) {
		    printf("\n>replicate copy() to parent\n");
                    char *ptr = copy_request(rcvmsg.region,rcvmsg.datalen);
                    if(write(tofd,ptr,sizeof(struct Request))<0){       //send copy request
                        perror("error replicating copy to parent:");
                    }
                    else if(write(tofd,buffer_c,rcvmsg.datalen)<0) {
                        perror("error replicating copy to parent:");
                    }
                    free(ptr);
                }
            }

            /*PASTE TO CLIENT*/
            else if(rcvmsg.operation==PASTE){

                pthread_rwlock_rdlock(&lock_rw[rcvmsg.region]);
                regionlen=regions[rcvmsg.region].length;
                char *buffer_p = malloc((regionlen*sizeof(char)));
                memcpy(buffer_p,regions[rcvmsg.region].data,regionlen);
                pthread_rwlock_unlock(&lock_rw[rcvmsg.region]);
                
                if(regionlen!=0) {
                    if(rcvmsg.datalen<regionlen) {   //if the client wants fewer bytes than the region has
                        if(write(clientfd,buffer_p,rcvmsg.datalen)<0) {
                            perror("error paste to client:");
                            free(buffer_p);
                            continue;
                        }
                    }
                    else {                          //if the client wants the same or more bytes than the region has
                        if(write(clientfd,buffer_p,regionlen)<0) {
                            perror("error paste to client:");
                            free(buffer_p);
                            continue;
                        }
                    }
                    
                }
                else {
                    if(write(clientfd,"\0",rcvmsg.datalen)<0) {
                        perror("error paste to client:");
                        free(buffer_p);
                        continue;
                    }
                }
                free(buffer_p);
            }

            /*WAIT FOR CLIENT*/
            else if(rcvmsg.operation==WAIT) {

                pthread_mutex_lock(&mux[rcvmsg.region]);
                pthread_cond_wait(&change[rcvmsg.region], &mux[rcvmsg.region]);
                regionlen=regions[rcvmsg.region].length;
                char *buffer_w = malloc((regionlen*sizeof(char)));
                memcpy(buffer_w,regions[rcvmsg.region].data,regionlen);
                pthread_mutex_unlock(&mux[rcvmsg.region]);

                if(regionlen!=0) {
                    if(rcvmsg.datalen<regionlen) {                           //if the client wants fewer bytes than the region has
                        if(write(clientfd,buffer_w,rcvmsg.datalen)<0){
                            perror("error wait to client:");
                            free(buffer_w);
                            continue;
                        }
                    }
                    else {                         
                        if(write(clientfd,buffer_w,regionlen)<0){            //if the client wants the same or more bytes than the region has
                            perror("error wait to client:");
                            free(buffer_w);
                            continue;
                        }
                    }
                }
                else {
                    if(write(clientfd,"\0",rcvmsg.datalen)<0) {
                        perror("error paste to client:");
                        free(buffer_w);
                        continue;
                    }   
                }
                free(buffer_w);
            }
        }
        else {
            printf(">client disconnected\n");
            pthread_detach(pthread_self()); 
            break;
        }
    }
    
    /*CLOSE CONNECTION*/
    close(clientfd);
    
    
    /*REMOVE THREAD FROM ACTIVE THREADS LIST*/
    pthread_rwlock_wrlock(&lock_threadlist);
    delete_from_tlist(tid, threadlist);
    pthread_rwlock_unlock(&lock_threadlist);
    
    /*EXIT*/
    pthread_exit(NULL);
}

void * thread_fromclip(void * fd){
    int fromfd=*(int*)fd;
    int i;
    struct Request rcvmsg;
    char data[sizeof(struct Request)];
    size_t regionlen;
    
    
    /*ADD THREAD TO ACTIVE THREADS LIST*/
    pthread_t tid = pthread_self();
    pthread_rwlock_wrlock(&lock_threadlist);
    add_to_tlist(tid, threadlist);
    pthread_rwlock_unlock(&lock_threadlist);
    
    /*FIRST UDDATE*/
    for(i=0; i<10; i++) {
        pthread_rwlock_rdlock(&lock_rw[i]);
        regionlen=regions[i].length;
        pthread_rwlock_unlock(&lock_rw[i]);
                
        /*SEND COPY'S FLAG WITH DATALEN OF THE i-TH REGION*/
        char *ptr = copy_request(i,regionlen);
        if(write(fromfd,ptr,sizeof(struct Request))<0){
            perror("error first update to child:");
            free(ptr);
            break;
        }
        free(ptr);
                
        /*WAIT FOR CONFIRMATION*/
        if(read(fromfd, data, sizeof(struct Request))<0){
           perror("error first update to child:");
           break;
        }
        memcpy(&rcvmsg,data,sizeof(struct Request));
                
        if(regionlen!=0) {
            /*SEND THE DATA*/
            char *buffer = malloc((regionlen*sizeof(char)));
                    
            pthread_rwlock_rdlock(&lock_rw[i]);
            memcpy(buffer,regions[i].data,regionlen);
            pthread_rwlock_unlock(&lock_rw[i]);
            if(write(fromfd,buffer,regionlen)<0){
                perror("error in first update to child:");
                free(buffer);
                break;
            }
            free(buffer);
                    
            /*WAIT FOR CONFIRMATION*/
            if(read(fromfd, data, sizeof(struct Request))<0){
                perror("error first update to child:");
                break;
            }
            memcpy(&rcvmsg,data,sizeof(struct Request));
        }
    }
    
    /*RECEIVE REQUESTS FROM CLIPBOARD CHILDREN*/
    while(1) {
        if(read(fromfd, data, sizeof(struct Request))>0){
            memcpy(&rcvmsg,data,sizeof(struct Request));

            if(valid_request(rcvmsg)) {
                //RECEIVE COPY REPLICATION FROM CHILD
                if(rcvmsg.operation==COPY) {
                    /*RECEIVE THE DATA*/
                    char *buffer = malloc((rcvmsg.datalen*sizeof(char)));
                    if(read(fromfd, buffer, rcvmsg.datalen)<0){
                        perror("error replicating copy from child:");
                        free(buffer);
                        continue;
                    }

                    /*SAVE THE DATA*/
                    pthread_rwlock_wrlock(&lock_rw[rcvmsg.region]);
                    free(regions[rcvmsg.region].data);
                    regions[rcvmsg.region].data=buffer;
                    regions[rcvmsg.region].length=rcvmsg.datalen;
                    pthread_rwlock_unlock(&lock_rw[rcvmsg.region]);

                    /*SIGNAL APPS WAITING FOR A CHANGE*/
                    pthread_mutex_lock(&mux[rcvmsg.region]);
                    pthread_cond_broadcast(&change[rcvmsg.region]);
                    pthread_mutex_unlock(&mux[rcvmsg.region]);

		    /*PRINT REGIONS*/
		    printf("\n\n>Regions:\n");
		    for(int i=0; i<10; i++) {
		         pthread_rwlock_rdlock(&lock_rw[i]);
		         printf("   Region[%d]: %s\n",i,regions[i].data);
		         pthread_rwlock_unlock(&lock_rw[i]);
		    }


                    /*REPLICATE COPY TO PARENT*/
                    if(mode==CONNECTED) {
                        char *ptr = copy_request(rcvmsg.region,rcvmsg.datalen);
                        if(write(tofd,ptr,sizeof(struct Request))<0){
                            perror("error replicating copy to parent:");
                        }
                        else if(write(tofd,buffer,rcvmsg.datalen)<0){
                            perror("error replicating copy to parent:");
                        }
                        free(ptr);
                        printf("\n>replicate copy() to parent\n");
                    }

                    /*REPLICATE TO THE OTHER CHILDREN*/
                    Node * current = fdlist->head;
                    if(fdlist->head != NULL) {
                        printf("\n>replicate copy() to children\n");
                        char *ptr = copy_request(rcvmsg.region,rcvmsg.datalen);

                        pthread_rwlock_rdlock(&lock_list);

                            //REPLICATE COPY() TO ALL CHILDREN 
                            while(current->next != NULL){

                                //EXCEPT THE CHILD THAT SENT THE ORIGINAL COPY REQUEST
                                if(current->fd!=fromfd){
                                    if(write(current->fd,ptr,sizeof(struct Request))<0) {            //send copy request
                                        perror("error replicating copy to child:");
                                    }             
                                    else if(write(current->fd,buffer,rcvmsg.datalen)<0){               //send the data
                                        perror("error replicating copy to child:");
                                    }
                                    current = current->next;
                                }
                                else {                                          //skip the child that sent me the original copy() request
                                    current = current->next;               
                                }
                            }

                            //LAST NODE
                            if(current->fd!=fromfd){
                                if(write(current->fd,ptr,sizeof(struct Request))<0){
                                    perror("error replicating copy to child:");
                                }                         //send copy request
                                else if(write(current->fd,buffer,rcvmsg.datalen)<0){
                                    perror("error replicating copy to child:");
                                }
                            }
                           

                        pthread_rwlock_unlock(&lock_list);
                        free(ptr);
                    }
                }
            }
            else {
                printf(">invalid request\n");
            }
        
        
        }
        else {
            printf(">clipboard disconnected\n");
            pthread_detach(pthread_self()); 
            break;
        }
    }
    
    
    
    /*DELETE THE CLIPBOARD DESCRIPTOR FROM ACTIVE CLIPBOARDS LIST*/
    pthread_rwlock_wrlock(&lock_list);
    delete_from_fdlist(fromfd,fdlist);
    pthread_rwlock_unlock(&lock_list);
    
    /*CLOSE CONNECTION TO CLIPBOARD*/
    close(fromfd);
    
    /*REMOVE THREAD FROM ACTIVE THREADS LIST*/
    pthread_rwlock_wrlock(&lock_threadlist);
    delete_from_tlist(tid, threadlist);
    pthread_rwlock_unlock(&lock_threadlist);
    
    /*EXIT*/
    pthread_exit(NULL);
} 

void * thread_toclip(void * fd) {
    int tofd=*(int*)fd;
    int i;
    char data[sizeof(struct Request)];
    struct Request rcvmsg;
       
        
    /*FIRST UPDATE*/
    printf(">First update\n"); 
    for(i=0; i<10; i++) {
            /*RECEIVE COPY'S FLAG WITH DATALEN FOR THE i-th REGION*/
            if(read(tofd, data, sizeof(struct Request))<0){
                perror("error in first update from parent:");
                break;
            }
            memcpy(&rcvmsg, data, sizeof(struct Request));
            
            if(valid_request(rcvmsg)) {
                /*SEND CONFIRMATION*/
                char *ptr = copy_request(i,1);
                if(write(tofd,ptr,sizeof(struct Request))<0){
                    perror("error in first update:");
                    free(ptr);
                    break;
                }
                free(ptr);

                if(rcvmsg.region==i && rcvmsg.datalen>0) {
                    /*RECEIVE DATA*/
                    char *buffer = malloc((rcvmsg.datalen*sizeof(char)));
                    if(read(tofd, buffer, rcvmsg.datalen)<0){
                        perror("error in first update from parent:");
                        break;
                    }

                    /*SAVE THE DATA*/
                    if(pthread_rwlock_wrlock(&lock_rw[i])!=0) perror("error acquiring the wrlock:");
                    else {    
                        free(regions[i].data);
                        regions[i].data=buffer;
                        regions[i].length=rcvmsg.datalen;
                        if(pthread_rwlock_unlock(&lock_rw[i])!=0) perror("error unlocking rwlock:");
                    }
                    
                    /*SEND CONFIRMATION*/
                    char *ptr = copy_request(i,1);
                    if(write(tofd,ptr,sizeof(struct Request))<0){
                        perror("error in first update:");
                        free(ptr);
                        break;
                    }
                    free(ptr);
                }
            }
            else {
                //ERROR HANDLING
                printf("Invalid request\n");
            }
        }
    
            
    /*RECEIVE REQUESTS FROM PARENT CLIPBOARD*/    
    while(1) {
        if(read(tofd, data, sizeof(struct Request))>0) {
            memcpy(&rcvmsg,data,sizeof(struct Request));

            if(valid_request(rcvmsg)) {
		
                /*RECEIVE COPY REPLICATION FROM PARENT*/
                if(rcvmsg.operation==COPY) {
                    /*RECEIVE THE DATA*/
                    char *buffer = malloc((rcvmsg.datalen*sizeof(char)));
                    if(read(tofd, buffer, rcvmsg.datalen)<0){
                        perror("error replicating from parent:");
                        free(buffer);
                        continue;
                    }


                    /*SAVE DATA*/
                    pthread_rwlock_wrlock(&lock_rw[rcvmsg.region]);
                        free(regions[rcvmsg.region].data);
                        regions[rcvmsg.region].data=buffer;
                        regions[rcvmsg.region].length=rcvmsg.datalen;
                    pthread_rwlock_unlock(&lock_rw[rcvmsg.region]);

                    /*SIGNAL APPS WAITING FOR A CHANGE*/
                    pthread_mutex_lock(&mux[rcvmsg.region]);
                        pthread_cond_broadcast(&change[rcvmsg.region]);
                    pthread_mutex_unlock(&mux[rcvmsg.region]);

		    /*PRINT REGIONS*/
		    printf("\n\n>Regions:\n");
		    for(int i=0; i<10; i++) {
		        pthread_rwlock_rdlock(&lock_rw[i]);
		        printf("   Region[%d]: %s\n",i,regions[i].data);
		        pthread_rwlock_unlock(&lock_rw[i]);
		    }

                    /*REPLICATE COPY TO CHILDREN*/
                    Node * current = fdlist->head;
                    if(fdlist->head != NULL) {
			printf("\n>replicate copy() to children\n");
                        char *ptr = copy_request(rcvmsg.region,rcvmsg.datalen);

                        pthread_rwlock_rdlock(&lock_list);
                            //go trough the linked list
                            while(current->next != NULL){
                                if(write(current->fd,ptr,sizeof(struct Request))<0){        //send copy request
                                    perror("error replicating copy to child:");
                                }                         
                                else if(write(current->fd,buffer,rcvmsg.datalen)<0){        //send the data
                                    perror("error replicating copy to child:");
                                }
                                current = current->next;
                            }
                            //last node
                            if(write(current->fd,ptr,sizeof(struct Request))<0){
                                perror("error replicating copy to child:");
                            }                        
                            else if(write(current->fd,buffer,rcvmsg.datalen)<0){
                                perror("error replicating copy to child:");
                            }
                        
                        pthread_rwlock_unlock(&lock_list);
                        free(ptr);
                    }
                }
            }
            else printf(">invalid request\n");
        }
        else {
            printf(">remote clipboard disconnected\n");
            pthread_detach(pthread_self()); 
            break;
        }
    }
    
    
    
    /*CLOSE CONNECTION TO PARENT CLIPBOARD*/
    close(tofd);
    
    /*UPDATE MODE*/
    mode = SINGLE;
    
    /*EXIT*/
    pthread_exit(NULL);           
}


void * thread_app_listener(void * appfd) {
    int fd=*(int*)appfd;
    int clientfd;
    struct sockaddr_un client_addr;
    int addrlen = sizeof(client_addr);
    
    while(1) {
        //ACCEPT APP CLIENT
        clientfd=accept(fd, (struct sockaddr *)&client_addr, &addrlen);
        if(clientfd==-1) {
            printf("Error accepting app connection\n");
            perror("accept");
        }
        else { 
            printf("\n>accept client\n");
            pthread_t thread;
            if(pthread_create(&thread, NULL, thread_app_handler, (void*) &clientfd) < 0) {
                perror("Error creating app handler thread\n");
            }
        }
    }
    
    pthread_exit(NULL);
}

void * thread_clip_listener(void * clipfd) {
    int tcpfd=*(int*)clipfd;
    int fromfd;
    struct sockaddr_in tcpaddr;
    int addrlen = sizeof(tcpaddr); 
    
    while(1) {
    
        //ACCEPT CLIPBOARD
        fromfd=accept(tcpfd, (struct sockaddr*)&tcpaddr, &addrlen);
        if(fromfd==-1){
            printf("Error accepting clipboard connection\n");
            perror ("accept");
        }
        else {
            printf("\n>accept clipboard\n");

            pthread_t thread;
            if(pthread_create(&thread, NULL, thread_fromclip, (void*) &fromfd) < 0) {
                perror("Error creating clipboard handler thread\n");
            }
            else {
                if(pthread_rwlock_wrlock(&lock_list)==0) {
                    add_to_fdlist(fromfd, fdlist);
                    if(pthread_rwlock_unlock(&lock_list)!=0) {
                        printf("Error unlocking wr_lock");
                    }
                }
                else printf("Error acquiring wr_lock");
            }
        }
    }
    
    pthread_exit(NULL);
}


int main(int argc, char** argv){
    signal(SIGPIPE, SIG_IGN);    
    signal(SIGINT, ctrl_c_callback_handler);
    int n, i;
    char cmd[50]={0};
    fd_set rfds;
    int maxfd, counter;
    char hostbuffer[256];
    char *IPbuffer;
    struct hostent *host_entry;
    int hostname;
    
    
    //CONNECTED MODE VARIABLES
    int tpt;
    char ip[17]={0};
        
       
    //UNIX SOCKET VARIABLES
    struct sockaddr_un local_addr;
    int fd;

    //TCP SOCKET VARIABLES
    int tcpfd;
    struct sockaddr_in tcpaddr, toaddr;
    int myport=50100;
    
    
    //INITIALIZE REGION POINTER
    for(i=0; i<10; i++) {
        regions[i].data=calloc(1,sizeof(char));     
    }
    
    //INITIALIZE RW_LOCKS
    for(int i=0;i<10;i++) {
        lock_rw[i] = (pthread_rwlock_t)PTHREAD_RWLOCK_INITIALIZER;
    }
    
    //INITIALIZE MUTEXs
    for(i=0;i<10;i++) {
        mux[i] = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    }
    
    //INITIALIZE CONDITION VARIABLES
    for(i=0;i<10;i++) {
        change[i] = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    }
    
    //INITIALIZE LINKED LIST
    fdlist = empty_fdlist();
    threadlist = empty_tlist();
    
    //READ ARGS
    read_args(argc, argv, &mode, ip, &tpt);
    if(mode==SINGLE) printf(">Run as single mode.\n");
    else if(mode==CONNECTED) printf(">Run as connected mode to ip:%s tpt:%d.\n", ip, tpt);
    
    
    //GET HOST IP
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    if (hostname == -1) {
        perror("gethostname");
        exit(1);
    }
    host_entry = gethostbyname(hostbuffer);
    if (host_entry == NULL) {
        perror("gethostbyname");
        exit(1);
    }
    IPbuffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
    if (NULL == IPbuffer) {
        perror("inet_ntoa");
        exit(1);
    }
    printf(">Host IP: %s\n", IPbuffer);
    printf(">TCP port: %d\n",myport);
    
   
    /*OPEN UNIX SOCKET*/
    unlink(SOCK_ADDRESS);
    fd=socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd==-1){
	perror("socket: ");
	exit(-1);	
    }
    local_addr.sun_family = AF_UNIX;
    strcpy(local_addr.sun_path, SOCK_ADDRESS);
    
    
    if(bind(fd, (struct sockaddr *)&local_addr, sizeof(local_addr))==-1) {
        printf("Error binding unix socket\n");
        perror("bind");
        exit(-1);
    }
    if(listen(fd, 10) == -1) {
	perror("listen");
	exit(-1);
    }
    
    //CREATE THREAD TO LISTENING AT SOCK_ADDRESS
    if(pthread_create(&threadapp, NULL, thread_app_listener, (void*) &fd) < 0) {
        perror("Error creating app listener thread");
        exit(-1);
    }
    
    //OPEN TCP SOCKET
    tcpfd=socket(AF_INET, SOCK_STREAM,0);
    if(tcpfd==-1){
	printf("Error creating tcp socket\n");
	exit(-1);	
    }
    memset((void*)&tcpaddr,(int)'\0',sizeof(tcpaddr));
    tcpaddr.sin_family=AF_INET;
    tcpaddr.sin_addr.s_addr= htonl(INADDR_ANY);
    tcpaddr.sin_port=htons(myport);             //don't forget to change this to 50000
    
    if(bind(tcpfd,(struct sockaddr*)&tcpaddr,sizeof(tcpaddr))==-1) {
        printf("Error binding tcp socket\n");
        perror("bind");
        exit(-1);
    }
    if(listen(tcpfd,5)==-1){
        perror("listen");
	exit(-1);
    }
    
    //CREATE THREAD TO LISTENING AT PORT 50000 FOR TCP CONNECTIONS
    if(pthread_create(&threadclip, NULL, thread_clip_listener, (void*) &tcpfd) < 0) {
        perror("Error creating tcp listener thread");
        exit(-1);
    }
    
    //CONNECT TO REMOTE CLIPBOARD
    if(mode==CONNECTED) {
        /*CONNECT TO REMOTE CLIPBOARD'S TCP SOCKET*/
        tofd=socket(AF_INET, SOCK_STREAM,0);
        if(tofd==-1){
            printf("Error creating tcp socket\n");
            exit(-1);	
        }
        memset((void*)&toaddr,(int)'\0',sizeof(toaddr));
        toaddr.sin_family=AF_INET;
        toaddr.sin_addr.s_addr= inet_addr(ip);
        toaddr.sin_port=htons(tpt);
        
        n=connect(tofd,(struct sockaddr*)&toaddr,sizeof(toaddr));
        if(n==-1) {
            printf("Error connecting to tcp socket\n");
            exit(-1);//error
        }
        else printf(">connected to remote clipboard\n");
                
        if(pthread_create(&threadremote, NULL, thread_toclip, (void*) &tofd) < 0) {
            perror("Error creating thread");
            exit(-1);
        }
  
    }
        
       
    while(1){
        FD_ZERO(&rfds);
        FD_SET(0,&rfds); maxfd = 0;

        counter = select(maxfd+1, &rfds, (fd_set *) NULL, (fd_set *) NULL,(struct timeval *)NULL);
        if(counter<=0) {
          printf("Error in counter\n");
          exit(-1);
        } 
        
        //KEYBOARD DESCRIPTOR
	if (FD_ISSET(0, &rfds)){
            fgets(cmd,50,stdin);
            if(strcmp(cmd,"exit\n")==0) {
                break;
            }
            if(strcmp(cmd,"show\n")==0) {
                /*PRINT REGIONS*/
		printf("\n\n>Regions:\n");
                for(int i=0; i<10; i++) {
                    pthread_rwlock_rdlock(&lock_rw[i]);
                    printf("   Region[%d]: %s\n",i,regions[i].data);
                    pthread_rwlock_unlock(&lock_rw[i]);
                }
            }
        }   
    }
    
    /*CANCEL ACTIVE THREADS*/
    pthread_cancel(threadapp);
    pthread_cancel(threadclip);
    if(mode==CONNECTED) pthread_cancel(threadremote);
    pthread_join(threadapp, NULL); 
    pthread_join(threadclip, NULL); 
    if(mode==CONNECTED) pthread_join(threadremote, NULL);
    
    NodeT * current = threadlist->head;
    if(threadlist->head != NULL) {
        pthread_rwlock_rdlock(&lock_threadlist);
        //go trough the linked list
        while(current->next != NULL){
            pthread_cancel(current->tid);
            pthread_join(current->tid, NULL);
            current = current->next;
        }
        //last node
        pthread_cancel(current->tid);
        pthread_join(current->tid, NULL);
        pthread_rwlock_unlock(&lock_threadlist);
    }
    
    
    
    /*Free regions pointer*/
    for(int i=0; i<10; i++) {
        free(regions[i].data);
    }
        
    /*Free Linked List*/
    destroy_fdlist(fdlist);
    destroy_tlist(threadlist);
    
    /*Unlink from filesystem for reuse*/
    unlink(SOCK_ADDRESS);
    
    exit(0);
	
}

