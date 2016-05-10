#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "local.h"

#define MAXSIZE		1000

static struct node
{
	GstBuffer *buf;
	struct node *next;
} *head = NULL, *tail = NULL;
int size = 0;

int push(GstBuffer *buf)
{
	GstBuffer *buffer;
	if (size >= MAXSIZE){printf("size >= MAXSIZE(%d)\n",MAXSIZE); return -1;}
	struct node *temp = (struct node *)malloc(sizeof(struct node));
	if (temp == NULL) return -1;
	buffer = gst_buffer_copy(buf);
	temp->buf = buffer;
	temp->next = NULL;
	if (tail)
	{
		tail->next = temp;
		tail = temp;
	}
	else
	{
		head = tail = temp;
	}
	size++;
	return 0;
}

int pop(GstBuffer *buf)
{
	if (head == NULL) return -1;
	struct node *temp = head;
	head = head->next;
	if (head == NULL) tail = NULL;
	if (buf)
	{
		gst_buffer_copy_metadata(buf, temp->buf, GST_BUFFER_COPY_ALL);
	    GST_BUFFER_DATA (buf) = temp->buf->data;
        GST_BUFFER_SIZE (buf) = temp->buf->size;
	}
	free(temp);
	size--;
	return 0;
}

int count()
{
/*	int count = 0;
	if (head)
	{
		struct node *temp = head;
		do
		{
			temp = temp->next;
			count++;
		}
		while (temp);
	}
	return count;*/
	return size;
}

int clear()
{
	while (head)
	{
		struct node *temp = head;
		head = head->next;
		if (temp->buf) free(temp->buf);
		free(temp);
	}
	head = tail = NULL;
	size = 0;
	return 0;
}

