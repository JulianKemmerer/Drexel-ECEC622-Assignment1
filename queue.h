#ifndef _QUEUE_H_
#define _QUEUE_H_

#define MAX_LENGTH 1024
#define TRUE 1
#define FALSE 0

/* Data type for queue element. */
typedef struct queue_element_tag{
	char path_name[MAX_LENGTH]; // Stores the path corresponding to the file/directory
	struct queue_element_tag *next;
} queue_element_t;

typedef struct queue_tag{
		  queue_element_t *head;
		  queue_element_t *tail;
		  unsigned int size;
} queue_t;


/* Function definitions. */
extern queue_t *create_queue(void);
void insert_in_queue(queue_t *, queue_element_t *);
queue_element_t *remove_from_queue(queue_t *);
void print_queue(queue_t *);

#endif
