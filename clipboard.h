
/**
 * @file clipboard.h
 * @brief Clipboard API. 
 */
#include <sys/types.h>

/**
 * Socket directory to communicate with the clipboard.
 */
#define SOCK_ADDRESS "./CLIPBOARD_SOCKET"



/** this function is called by the application to interact
 * with the distributed clipboard.
 * 
 * @param clipboard_dir - directory  where  the  local clipboard was launched.
 * 
 * @return -1 if cannot be accessed or >0 in case of success. 
 * It will be used in all other functions as clipboard_id.
 */
int clipboard_connect(char * clipboard_dir);

/** this function copies the data pointed by buf to a region on the local 
 * clipboard
 * 
 * @param clipboard_id - value  returned by clipboard_connect().
 * @param region - region the user wants to copy the data to [0 to 9].
 * @param buf - data that is to be copied to the local clipboard.
 * @param count - the length of the data pointed by buf.
 * 
 * @return 0 if error; >0 with the bytes copied
 */
int clipboard_copy(int clipboard_id, int region, void *buf, size_t count);

/** this function copies from the system to the application the data in a 
 * certain region.
 * 
 * @param clipboard_id - value  returned by clipboard_connect(). 
 * @param region - region the user wants to paste data from [0 to 9].
 * @param buf -   pointer to where the data is to be copied to.
 * @param count - the length that the user wants to paste.
 * 
 * @return 0 if error; >0 with the bytes copied.
 */
int clipboard_paste(int clipboard_id, int region, void *buf, size_t count);

/** this function waits for a change on a certain region, and when it happens
 * the new data in that region is copied to memory pointed by buf up to a 
 * length of count.
 * 
 * @param clipboard_id - value  returned by clipboard_connect(). 
 * @param region - region the user wants to wait on [0 to 9].
 * @param buf -   pointer where the data is to be copied to.
 * @param count - the length of the data to be copied.
 * 
 * @return 0 if error; >0 with the bytes copied.
 */
int clipboard_wait(int clipboard_id, int region, void *buf, size_t count);

/** this function closes the connection between the application and the local
 * clipboard.
 * 
 * @param clipboard_id - value  returned by clipboard_connect(). 
 * 
 * @return void
 */
void clipboard_close(int clipboard_id);

