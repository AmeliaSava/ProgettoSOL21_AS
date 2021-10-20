#if !defined(_MESSAGE_H)
#define _MESSAGE_H

/** 
 * tipo del messaggio
 */
typedef struct MSG {
	int namelenght;
	long size;
	op op_type; 
	char filename[MAX_SIZE];
	char filecontents[MAX_SIZE];
} msg;

#endif 
  