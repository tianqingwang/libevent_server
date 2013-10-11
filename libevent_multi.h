#ifndef __LIBEVENT_MULTI_H__
#define __LIBEVENT_MULTI_H__
#include <event.h>
#include <pthread.h>

#define DEFAULT_MAXTHREADS 2

typedef struct cq_item{
    int         fd;
    int         ev_flag;
    struct cq_item *next;
}CQ_ITEM;

typedef struct cq{
    CQ_ITEM         *header;
    CQ_ITEM         *tail;
    pthread_mutex_t lock;
}CQ;

typedef struct _LIBEVENT_THREAD{
    pthread_t          thread_id;
    int                notify_receive_fd;
    int                notify_send_fd;
    struct event_base  *base;
    struct event       notify_event;
    CQ                 new_conn_queue;
}LIBEVENT_THREAD;


int worker_thread_init(int nthreads);
void master_thread_loop(int sockfd);
void set_read_callback(void (*callback)(struct bufferevent *, void *));
void set_write_callback(void (*callback)(struct bufferevent *,void *));
#endif
