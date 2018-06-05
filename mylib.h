#ifndef MYLIB_H
#define MYLIB_H

typedef enum {COPY, PASTE, WAIT} Op;
typedef enum {SINGLE, CONNECTED} Clipboard_mode;


struct Request {
    Op operation;              //0 if copy, 1 if paste, 2 if wait
    int region;
    size_t datalen;
};

struct Region {
    char *data;              
    size_t length;
};

typedef struct node {
    int fd;
    struct node * next;
} Node;

typedef struct list {
  Node * head; 
} List;


typedef struct nodeT {
    pthread_t tid;
    struct nodeT * next;
} NodeT;

typedef struct listT {
  NodeT * head; 
} ListT;



char * copy_request(int region, int datalen);
void set_msg(struct Request *msg, int op, int reg, size_t len);
void read_args(int argc, char** argv, Clipboard_mode *mode, char *ip, int *tpt);
bool valid_request(struct Request msg);

//DESCRIPTORS LINKED LIST LIBRARY
List * empty_fdlist();
void add_to_fdlist(int fd, List * list);
void delete_from_fdlist(int fd, List * list);
void destroy_fdlist(List * list);

//ACTIVE THREADS LINKED LIST LIBRARY
ListT * empty_tlist();
void add_to_tlist(pthread_t tid, ListT * list);
void delete_from_tlist(pthread_t tid, ListT * list);
void destroy_tlist(ListT * list);


#endif	// MYLIB_H

