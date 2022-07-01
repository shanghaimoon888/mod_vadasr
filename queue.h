#include <stdio.h>

typedef struct _node {
	int data;
	void *buffer;
	struct _node *next;
	// struct _node *pre;
} node;

typedef struct {
	node *front;
	node *rear;
} queue;

queue *create_queue(void)
{
	queue *myqueue = (queue *)malloc(sizeof(queue));
	myqueue->front = NULL;
	myqueue->rear = NULL;

	return myqueue;
}

// insert queue
queue *insert_queue(queue *myqueue, int data, void *buffer, int buflen)
{
	if (NULL == myqueue) {
		printf("myqueue is NULL\n");
		return NULL;
	}
	node *new_node = NULL;
	new_node = (node *)malloc(sizeof(node)); // create a new node
	new_node->data = data;
	if (NULL != buffer) {
		new_node->buffer = malloc(buflen);
		memcpy(new_node->buffer, buffer, buflen);
	}
	else
	{
		new_node->buffer = NULL;
	}
	new_node->next = NULL;

	if (myqueue->rear == NULL) { // if the queue is empty
		myqueue->front = myqueue->rear = new_node;
	} else {
		myqueue->rear->next = new_node;
		myqueue->rear = new_node; // move queue rear pointer to new_node
	}

	return myqueue;
}

queue *delete_queue(queue *myqueue)
{
	node *p_node = NULL;
	p_node = myqueue->front;
	if (NULL == p_node) {
		printf("this is empty queue\n");
		return NULL;
	} else {
		myqueue->front = myqueue->front->next;
		if (myqueue->front == NULL) { myqueue->rear = NULL; }
		if (NULL != p_node->buffer) { free(p_node->buffer); }
		free(p_node);
	}

	return myqueue;
}

int get_queue_length(queue *myqueue)
{
	node *p_node = NULL;
	int len = 0;

	p_node = myqueue->front;
	if (NULL != p_node) { len = 1; }
	while (p_node != myqueue->rear) {
		p_node = p_node->next;
		len++;
	}

	return len;
}

void destroy_queue(queue *myqueue)
{
	int len = get_queue_length(myqueue);
	for (int i = 0; i < len; i++) { delete_queue(myqueue); }

	free(myqueue);
}

void queue_print(queue *myqueue)
{
	node *p_node = NULL;
	p_node = myqueue->front;

	if (NULL == p_node) {
		printf("this is empty queue\n");
		return;
	}
	printf("The queue is :");
	while (p_node != myqueue->rear) {
		printf("%2d", p_node->data);
		p_node = p_node->next;
	}
	printf("%2d\n", p_node->data);
}
//typedef enum { false, true } bool;
bool getvadflagcount(queue *myqueue, int length, int state)
{
	int len = get_queue_length(myqueue);
	len = len - length;
	if (len < 0) { return false; }

	node *p_node = NULL;
	p_node = myqueue->front;

	while (p_node != myqueue->rear) {
		if (len == 0) {
			if (p_node->data != state) { return false; }
		} else {
			len--;
		}
		p_node = p_node->next;
	}

	/*int count = 0;
	while (p_node != myqueue->rear) {

		count++;
		if (count > len && p_node->data != state) { return false; }
		p_node = p_node->next;
	}*/

	return true;
}
