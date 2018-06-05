#include <sys/types.h>


#define SOCK_ADDRESS "./CLIPBOARD_SOCKET"
#define SOCK_ADDRESS_TEST1 "./TEST_CONNECTED1"           //don't forget to change this
#define SOCK_ADDRESS_TEST2 "./TEST_CONNECTED2"           //don't forget to change this





int clipboard_connect(char * clipboard_dir);
int clipboard_copy(int clipboard_id, int region, void *buf, size_t count);
int clipboard_paste(int clipboard_id, int region, void *buf, size_t count);
int clipboard_wait(int clipboard_id, int region, void *buf, size_t count);
void clipboard_close(int clipboard_id);

