#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include "mylib.h"



/**************************************************************************
 * set_msg()
 * 
 * Updates the values of the variables of the message struct given.
 * 
 * @param msg - message struct to be updated
 * @param op - requested operation [0-copy, 1-paste]
 * @param reg - operation region
 * @param len - data's length to be sent afterwards
 *************************************************************************/
void set_msg(struct Request *msg, int op, int reg, size_t len) {
    msg->operation = op;
    msg->region=reg;
    msg->datalen=len;
    
    return;
}


/******************************************************************************
 * read_args()
 * 
 * Assigns the values to the respective variables by reference.
 * 
 * @param mode - [0-single, 1-connected]  
 * @param ip - target clipboard's ip
 * @param tpt - target clipboard's tcp port
 * @return void
 *****************************************************************************/
void read_args(int argc, char** argv, Clipboard_mode *mode, char *ip, int *tpt) {
        
    if(argc==1) {
        *mode=SINGLE;
        return;
    }
    
    if(argc<4) {
        printf("Too few arguments. Usage: clipboard –c [ip] [tpt]\n");
        exit(-1);
    }
    
    if(argc>4) {
        printf("Too many arguments. Usage: clipboard –c [ip] [tpt]\n");
        exit(-1);
    }
    
    if(strcmp(argv[1],"-c")==0) {
        *mode=CONNECTED;
        strcpy(ip,argv[2]);
        *tpt=atoi(argv[3]);
        return;
    }
        
    printf("Wrong arguments. Usage: clipboard –c [ip] [tpt]\n");
    exit(-1);   
}



/******************************************************************************
 * copy_request()
 * 
 * Creates a pointer to a byte stream with a request for a copy operation.
 * 
 * @param region - operation region
 * @param datalen - data's length to be sent afterwards
 * @return char * ptr
 *****************************************************************************/
char * copy_request(int region, int datalen) {
    struct Request sdmsg;
    char *ptr = malloc(sizeof(struct Request));
    Op operation=COPY;
    
    set_msg(&sdmsg,operation,region,datalen);
    memcpy(ptr,&sdmsg,sizeof(struct Request));
    return ptr;
}

/******************************************************************************
 * check_request()
 * 
 * Check the values of the struct Request.
 * 
 * @param msg - Request struct
 * @return bool - true if it checks out, false if it's not formated correctly.
 *****************************************************************************/
bool valid_request(struct Request msg) {
    if(msg.operation==0 || msg.operation==1 || msg.operation==2) {
        if(msg.region>=0 && msg.region<10) {
            if((int)msg.datalen>=0) {
                return true;
            }
            else return false;
        }
        else return false;
    }
    else return false;    
}



/******************************************************************************
 * create_node()
 * 
 * Creates a node and updates its values.
 * 
 * @param fd - file descriptor to be saved as the node's data
 * @return Node pointer to the created node
 *****************************************************************************/
Node * create_node(int fd);

Node * create_node(int fd){
    Node * newNode = malloc(sizeof(Node));
    newNode->fd = fd;
    newNode->next = NULL;
    return newNode;
}

/******************************************************************************
 * empty_fdlist()
 * 
 * Initializes a Linked List.
 * 
 * @return List pointer to the created List.
 *****************************************************************************/
List * empty_fdlist(){
    List * list = malloc(sizeof(List));
    list->head = NULL;
    return list;
}

/******************************************************************************
 * add_to_fdlist()
 * 
 * Adds a node at the end of the Linked List.
 * 
 * @param fd - file descriptor to be saved as the node's data to add
 * @param list - pointer to the Linked List to add the node to
 * @return void
 *****************************************************************************/
void add_to_fdlist(int fd, List * list){
    Node * current = NULL;
    
    if(list->head == NULL){
        list->head = create_node(fd);
    }
    else {
        current = list->head; 
        while (current->next!=NULL){
            current = current->next;
        }
        current->next = create_node(fd);
    }
}


/******************************************************************************
 * delete_from_fdlist()
 * 
 * Deletes a node by its value from the Linked List, restoring the links.
 * 
 * @param fd - data of the node to be deleted
 * @param list - pointer to the Linked List
 * @return void
 *****************************************************************************/
void delete_from_fdlist(int fd, List * list){
    Node * current = list->head;            
    Node * previous = current;           
  
    while(current != NULL){           
    
        if(current->fd == fd){      
            previous->next = current->next;
      
            if(current == list->head) list->head = current->next;
            free(current);
        return;
    }
        
    previous = current;             
    current = current->next;        
  }                                 
}           


/******************************************************************************
 * destroy_fdlist()
 * 
 * Destroys the entire list by freeing all the memory.
 *
 * @param list - pointer to the Linked List
 * @return void
 *****************************************************************************/
void destroy_fdlist(List * list){
    Node * current = list->head;
    Node * next = current;
  
    while(current != NULL){
        next = current->next;
        free(current);
        current = next;
    }
    free(list);
}



/******************************************************************************
 * create_tnode()
 * 
 * Creates a node and updates its values.
 * 
 * @param tid- thread id to be saved as the node's data
 * @return Node pointer to the created node
 *****************************************************************************/
NodeT * create_tnode(pthread_t tid);

NodeT * create_tnode(pthread_t tid){
    NodeT * newNode = malloc(sizeof(NodeT));
    newNode->tid = tid;
    newNode->next = NULL;
    return newNode;
}

/******************************************************************************
 * empty_tist()
 * 
 * Initializes a Linked List.
 * 
 * @return List pointer to the created List.
 *****************************************************************************/
ListT * empty_tlist(){
    ListT * list = malloc(sizeof(ListT));
    list->head = NULL;
    return list;
}

/******************************************************************************
 * add_to_tlist()
 * 
 * Adds a node at the end of the Linked List
 * 
 * @param tid - thread id to be saved as the node's data to add
 * @param list - pointer to the Linked List to add the node to
 * @return void
 *****************************************************************************/
void add_to_tlist(pthread_t tid, ListT * list){
    NodeT * current = NULL;
    
    if(list->head == NULL){
        list->head = create_tnode(tid);
    }
    else {
        current = list->head; 
        while (current->next!=NULL){
            current = current->next;
        }
        current->next = create_tnode(tid);
    }
}


/******************************************************************************
 * delete_from_tlist()
 * 
 * Deletes a node by its value from the Linked List, restoring the links.
 * 
 * @param tid - data of the node to be deleted
 * @param list - pointer to the Linked List
 * @return void
 *****************************************************************************/
void delete_from_tlist(pthread_t tid, ListT * list){
    NodeT * current = list->head;            
    NodeT * previous = current;           
  
    while(current != NULL){           
    
        if(current->tid == tid){      
            previous->next = current->next;
      
            if(current == list->head) list->head = current->next;
            free(current);
        return;
    }
        
    previous = current;             
    current = current->next;        
  }                                 
}           


/******************************************************************************
 * destroy_tlist()
 * 
 * Destroys the entire list by freeing all the memory.
 *
 * @param list - pointer to the Linked List
 * @return void
 *****************************************************************************/
void destroy_tlist(ListT * list){
    NodeT * current = list->head;
    NodeT * next = current;
  
    while(current != NULL){
        next = current->next;
        free(current);
        current = next;
    }
    free(list);
}