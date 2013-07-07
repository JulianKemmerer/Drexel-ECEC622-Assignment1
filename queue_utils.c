#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"


/* Creates an empty queue and return a pointer to the queue data structure. */
queue_t *create_queue(void)
{
	queue_t *this_queue = (queue_t *)malloc(sizeof(queue_t)); 
	if(this_queue == NULL) return NULL;
	this_queue->head = this_queue->tail = NULL;
	this_queue->size=0;
	return this_queue;
}

/* Insert an element in the queue. */
void insert_in_queue(queue_t *queue, queue_element_t *element)
{
	element->next = NULL;

	if(queue->head == NULL){ // Queue is empty
		queue->head = element;
		queue->tail = element;
	} else{ // Add element to the tail of the queue
		(queue->tail)->next = element;
		queue->tail = (queue->tail)->next;
	}
	(queue->size)++;
}

/* Remove element from the head of the queue. */
queue_element_t *remove_from_queue(queue_t *queue)
{
	queue_element_t *element = NULL; 
	if(queue->head != NULL){
		element = queue->head;
		queue->head = (queue->head)->next;
	}
	(queue->size)--;
	return element;
}

/* Print the elements in the queue. */
void print_queue(queue_t *queue)
{
	queue_element_t *current = queue->head;
	while(current != NULL){
		printf("%s \n", current->path_name);
		current = current->next;
	}
}
