#include "clipboard.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <stdbool.h>

int count;      //size of stdin input data


/******************************************************************************
 * read_unlimited_input()
 * 
 * Reads from stdin unlimited input by reallocating memory every time it 
 * maximizes the buffer.
 * 
 * @return char * pointer to stdin input data.
 *****************************************************************************/
char * read_unlimited_input() {
    unsigned int len_max = 128;
    unsigned int current_size = 0;
    
    char *ptr = malloc(len_max);
    current_size = len_max;
    
    if(ptr != NULL) {
	int c = EOF;
	count=0;
        //accept user input until hit enter or end of file
	while((c=getchar())!='\n' && c!=EOF) {
            ptr[count++]=(char)c;

            //if it reached maximize size then realloc size
            if(count == current_size) {
                current_size = count+len_max;
		ptr = realloc(ptr, current_size);
            }
	}
        ptr[count] = '\0';
    }
    
    return ptr;
}

/******************************************************************************
 * reg_matches()
 * 
 * Compares a string to a given regular expression.
 *
 * @param *str - string to compare.
 * @param *pattern - regex to compare string to.
 * @return true if is a match, false otherwise.
 *****************************************************************************/
bool reg_matches(const char *str, const char *pattern) {
    regex_t re;
    int ret;

    if (regcomp(&re, pattern, REG_EXTENDED)!= 0) return false;
    
    ret = regexec(&re, str, (size_t) 0, NULL, 0);
    
    regfree(&re);

    if(ret == 0) return true;

    return false;
}


int main(){
    char cmd[50]={0};
    int fd;
    int region;
    int size;
    
    fd = clipboard_connect(SOCK_ADDRESS);
    if(fd== -1){
        printf("Error in connect.\n");
	exit(-1);
    }
    
    while(1) {
        printf("\nOperation [copy,paste,wait,close]:  ");
        fgets(cmd,50,stdin);

        if(strcmp(cmd,"copy\n")==0) {
            printf("Region [0-9]:  ");
            fgets(cmd,50,stdin);
            if(reg_matches(cmd, "[0-9]+$\n")) {
                region = atoi(cmd);
                if(region<0 || region>9) {
                    printf(">Invalid region. Try again.\n");
                    continue;
                }
            }
            else {
                printf(">Invalid region. Try again.\n");
                continue;
            }
            
            printf("Input:  ");
            char *ptr = read_unlimited_input();
            if(!clipboard_copy(fd,region,ptr,count+1)) {
                printf("Error in clipboard_copy.\n");
                free(ptr);
                exit(-1);
            }
            free(ptr);
        }
        else if(strcmp(cmd,"paste\n")==0) {
            printf("Region [0-9]:  ");
            fgets(cmd,8,stdin);
            region = atoi(cmd);
            if(region<0 || region>9) {
                printf(">Invalid region. Try again.\n");
                continue;
            }
                        
            printf("Size:  ");
            fgets(cmd,8,stdin);
            size=atoi(cmd);
            if(size<=0) {
                printf(">Invalid size. Try again.");
                continue;
            }
            char *ptr = malloc((size+1)*sizeof(char));
            if(!clipboard_paste(fd,region,ptr,size)) {
                printf("Error in clipboard_paste.\n");
                free(ptr);
                exit(-1);
            }
            ptr[size]='\0';
            printf("[paste] %s",ptr);
            free(ptr);
        }
        else if(strcmp(cmd,"wait\n")==0) {
            printf("Region [0-9]:  ");
            fgets(cmd,8,stdin);
            region = atoi(cmd);
            if(region<0 || region>9) {
                printf(">Invalid region. Try again.\n");
                continue;
            }
            
            printf("Size:  ");
            fgets(cmd,8,stdin);
            size=atoi(cmd);
            if(size<=0) {
                printf(">Invalid size. Try again.");
                continue;
            }
            char *ptr = malloc((size+1)*sizeof(char));
            if(!clipboard_wait(fd,region,ptr,size)) {
                printf("Error in clipboard_wait.\n");
                free(ptr);
                exit(-1);
            }
            ptr[size]='\0';
            printf("[wait] %s",ptr);
            free(ptr);
        }
        else if(strcmp(cmd,"close\n")==0) {
            clipboard_close(fd);
            break;
        }
        else printf(">Invalid operation. Try again\n");
    }
      
    
    exit(0);
}