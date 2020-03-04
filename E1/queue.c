#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "queue.h"

// circular array
typedef struct _queue {
    int size;
    int used;
    int first;
    void **data;
    pthread_cond_t condinsert;
    pthread_cond_t condremove;
    pthread_mutex_t mutex;
} _queue;



queue q_create(int size) {
    queue q = malloc(sizeof(_queue));

    q->size  = size;
    q->used  = 0;
    q->first = 0;
    q->data  = malloc(size*sizeof(void *));

    pthread_mutex_init(&q->mutex,NULL);
    pthread_cond_init(&q->condinsert,NULL);
    pthread_cond_init(&q->condremove,NULL);

    return q;
}

int q_elements(queue q) {
    return q->used;
}

int q_insert(queue q, void *elem) {

    int is_empty;
    pthread_mutex_lock(&q->mutex);
    while(q->size==q->used) {
        pthread_cond_wait(&q->condinsert, &q->mutex);
    }
    is_empty = (q->used==0);
    q->data[(q->first+q->used) % q->size] = elem;
    q->used++;
    if(is_empty){
        pthread_cond_broadcast(&q->condremove);
    }
    pthread_mutex_unlock(&q->mutex);
    return 1; 
}

void *q_remove(queue q) {

    void *res;
    int is_full;

    pthread_mutex_lock(&q->mutex);
    while(q->used==0){
        pthread_cond_wait(&q->condremove, &q->mutex);
    }
    is_full = (q->used==q->size);
    res=q->data[q->first];
    q->first=(q->first+1) % q->size;
    q->used--;
    if(is_full){
        pthread_cond_broadcast(&q->condinsert);
    }
    pthread_mutex_unlock(&q->mutex);

    return res;
}

void q_destroy(queue q) {
    free(q->data);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->condinsert);
    pthread_cond_destroy(&q->condremove);
    free(q);
}
