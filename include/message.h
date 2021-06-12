#if !defined(_MESSAGE_H)
#define _MESSAGE_H

/** 
 * tipo del messaggio
 */
typedef struct MSG {
	int lenN;
	long size;
	op op_type; 
	char* filename;
	char* filecontents;
} msg;

#endif 
