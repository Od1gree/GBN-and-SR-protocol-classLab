//
// Created by wangnaizheng on 2018/5/2.
//

#include<stdlib.h>
#include"queue.h"

int QueueInit(QUEUE *queue, int maxSize){
    if(queue == NULL)
        return -1;

    queue->size = 0;
    queue->maxSize = maxSize;
    queue->head = NULL;
    queue->tail = NULL;
    return 1;
}

int push(QUEUE *queue, char *item){
    if(item == NULL || queue == NULL)
        return -1;
    if(queue->size >= queue->maxSize)
        return 0;

    NODE *newnode = (NODE*)malloc(sizeof(NODE));
    strcpy(newnode->item, item);
    newnode->next = NULL;

    if(queue->size == 0){
        queue->head = newnode;
        queue->tail = newnode;
        queue->size++;
    }
    else{
        queue->tail->next = newnode;
        queue->tail = newnode;
        queue->size ++;
    }
    return 1;
}

int pop(QUEUE *queue, char *ret){
    if(queue == NULL)
        return -1;
    if(queue->size <= 0)
        return 0;

    NODE *tempNode = queue->head;
    queue->head = queue->head->next;
    queue->size--;
    strcat(ret, tempNode->item);
    //free(tempNode);

    return 1;
}

int insertFirst(QUEUE *queue, char *item){
    if(queue->size == 0){
        return push(queue, item);
    }

    NODE *newnode = (NODE*)malloc(sizeof(NODE));
    strcpy(newnode->item, item);
    newnode->next = queue->head;
    queue->head = newnode;
    queue->size ++;
    return 1;
}
