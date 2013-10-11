#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <string.h>
#include "libevent_socket.h"
#include "libevent_multi.h"


static LIBEVENT_THREAD *threads;

struct event_base *main_base;
struct event       main_event;
struct event       clock_event;

static last_thread = -1;
static int g_threads_num;

static int free_item_total;
static int free_item_curr;
static CQ_ITEM **freeitems;

static void (*g_readcb)(struct bufferevent *bev, void *arg);
static void (*g_writecb)(struct bufferevent *bev, void *arg);
static void (*g_eventcb)(struct bufferevent *bev,short events, void *arg);

void on_accept(int fd, short which, void *args);
void on_read(struct bufferevent *bev, void *arg);
void on_write(struct bufferevent *bev,void *arg);
void check_timeout(int fd, short which, void *args);

static void cq_init(CQ *cq){
    pthread_mutex_init(&cq->lock,NULL);
    
    free_item_total = 1000;
    free_item_curr  = 1000;
    
    freeitems = malloc(sizeof(CQ_ITEM*)*free_item_total);
    if (freeitems == NULL){
        fprintf(stderr,"failed to allocate.\n");
        exit(1);
    }
    
    cq->header = NULL;
    cq->tail   = NULL;
}

static CQ_ITEM *item_from_freelist()
{
    CQ_ITEM *item;
    
//    pthread_mutex_lock(&cq->lock);
    
    if (free_item_curr > 0){
        item = freeitems[--free_item_curr];
    }
    else{
        item = NULL;
    }
   
//    pthread_mutex_unlock(&cq->lock);
    
    return item;
}

static int add_to_freelist(CQ_ITEM *item)
{
    int ret = -1;
    
    if (free_item_curr < free_item_total){
        freeitems[free_item_curr++] = item;
        ret = 0;
    }
    else{
        int new_size = free_item_total*2;
        CQ_ITEM **new_freeitems = realloc(freeitems,new_size*sizeof(CQ_ITEM*));
        if (new_freeitems != NULL){
            free_item_total = new_size;
            freeitems = new_freeitems;
            freeitems[free_item_curr++] = item;
            ret = 0;
        }
    }
    
    return ret;
}

static void cq_free(CQ *cq,CQ_ITEM *item)
{
    
    pthread_mutex_lock(&cq->lock);
    
    if (item != NULL){
        if (add_to_freelist(item) == -1){
            free(item);
        }
    }
    
    pthread_mutex_unlock(&cq->lock);
}

static void cq_push(CQ *cq, CQ_ITEM *item){
    item->next = NULL;
    
    pthread_mutex_lock(&cq->lock);
    
    if (NULL== cq->tail){
        cq->header = item;
    }
    else{
        cq->tail->next = item;
    }
    cq->tail = item;
    
    pthread_mutex_unlock(&cq->lock);
}

static CQ_ITEM *cq_pop(CQ *cq){
    CQ_ITEM *item;
    
    pthread_mutex_lock(&cq->lock);
    
    item = cq->header;
    if (item != NULL){
        cq->header = item->next;
        if (NULL == cq->header){
            cq->tail = NULL;
        }
    }
    
    pthread_mutex_unlock(&cq->lock);
    
    return item;
}

void dispatch_conn(int sfd,int ev_flag){
    char buf[1];
    /*fixme: maybe memory can be pre-allocated.*/
//    CQ_ITEM *item = malloc(sizeof(CQ_ITEM));
    CQ_ITEM *item = item_from_freelist();
    if (item == NULL){
        item = malloc(sizeof(CQ_ITEM));
        if (item == NULL){
            fprintf(stderr,"malloc error.\n");
            exit(1);
        }
    }
    
    item->fd = sfd;
    item->ev_flag = ev_flag;
 
    int tid = (last_thread + 1)%g_threads_num;
    last_thread = tid;
    
    LIBEVENT_THREAD *thread = threads + tid;

    cq_push(&thread->new_conn_queue,item);
    buf[0] = 'c';
    write(thread->notify_send_fd,buf,1);
}


static void create_worker(void *(*func)(void*),void *arg)
{
    LIBEVENT_THREAD *thread = (LIBEVENT_THREAD*)arg;
   
    pthread_attr_t attr;
    int ret ;
    
    pthread_attr_init(&attr);
    
    ret = pthread_create(&thread->thread_id,&attr,func,arg);
    if (ret != 0){
        perror("Failed to create thread.");
        exit(1);
    }
    
}

static void thread_libevent_process(int fd, short which,void *arg)
{
    LIBEVENT_THREAD *this = arg;
    CQ_ITEM *item;
    char buf[1];
    struct timeval tv={5,0};
    
    if (read(fd,buf,1) != 1){
        perror("can't read from libevent pipe.");
    }
    printf("thread_libevent_process\n");
    item = cq_pop(&this->new_conn_queue);

    struct bufferevent *bev = bufferevent_socket_new(this->base,item->fd,BEV_OPT_CLOSE_ON_FREE);
    /*fixme: need callback argument*/
    bufferevent_setcb(bev,g_readcb,g_writecb,g_eventcb,NULL);
    
    bufferevent_set_timeouts(bev,&tv,&tv);
    
    bufferevent_enable(bev,EV_READ|EV_WRITE|EV_PERSIST);
    
//    free(item);
    cq_free(&this->new_conn_queue,item);
}

static void *worker_libevent(void *arg)
{
    LIBEVENT_THREAD *me = arg;
    printf("worker_libevent\n");
    
    event_base_loop(me->base,0);
    
    return NULL;
}


int worker_thread_init(int nthreads)
{
    int i;
    
    if (nthreads <= 0){
        g_threads_num = DEFAULT_MAXTHREADS;
    }
    else{
        g_threads_num = nthreads;
    }
    
    threads =(LIBEVENT_THREAD*) malloc(sizeof(LIBEVENT_THREAD)*nthreads);
    if (threads == NULL){
        return -1;
    }
    
    for (i=0; i<nthreads; i++){
        int fds[2];
        if (pipe(fds)){
            perror("Failed to create pipe.");
            return -1;
        }
        threads[i].notify_receive_fd = fds[0];
        threads[i].notify_send_fd    = fds[1];
        
        setup_thread(&threads[i]);
    }
    
    /*create worker threads.*/
    for (i=0; i<nthreads; i++){
        create_worker(worker_libevent,&threads[i]);
    }
}


int setup_thread(LIBEVENT_THREAD *thread)
{
    thread->base = event_init();
    if (thread->base == NULL){
        perror("Failed to allocate event base.");
        return -1;
    }
    
    event_set(&thread->notify_event,thread->notify_receive_fd,EV_READ|EV_PERSIST,thread_libevent_process,thread);
    event_base_set(thread->base,&thread->notify_event);
    if (event_add(&thread->notify_event,0) == -1){
        perror("Failed to add event.");
        return -1;
    }
    
    cq_init(&thread->new_conn_queue);
}

void master_thread_loop(int sockfd)
{
    fd_init();
  
    main_base = event_init();
    
    struct timeval tv={10,0};

    event_set(&main_event,sockfd,EV_READ|EV_PERSIST,on_accept,NULL);    
    event_base_set(main_base,&main_event);
    event_add(&main_event,0);
    
    /*add timer*/
    event_set(&clock_event,-1,EV_TIMEOUT|EV_PERSIST,check_timeout,NULL);
    event_base_set(main_base,&clock_event);
    event_add(&clock_event,&tv);
 
    event_base_loop(main_base,0);
}

void set_read_callback(void (*callback)(struct bufferevent *, void *))
{
    g_readcb = callback;
}

void set_write_callback(void (*callback)(struct bufferevent *,void *))
{
    g_writecb = callback;
}

/*main_base is in charge of accept.*/
void on_accept(int fd, short which, void *args)
{
    int connfd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    connfd = accept(fd,(struct sockaddr*)&client_addr,&client_len);
    if (connfd == -1){
        if (errno == EAGAIN || errno == EWOULDBLOCK){
            /*fixme: to do something for EAGAIN*/
        }
        else{
            fprintf(stderr,"Too many open connections.\n");
            return;
        }
    }
    
    /*libevent needs nonblocking socket fd.*/
    if (set_socket_nonblock(connfd) == -1){
        fprintf(stderr,"failed to set NONBLOCK.\n");
        close(connfd);
    }
    
    fd_insert(connfd);
    
    dispatch_conn(connfd,EV_READ|EV_PERSIST);
    
}


/*CLOSE_WAIT check*/
void check_timeout(int fd, short which, void *args)
{

    if (which & EV_TIMEOUT){
        check_closewait_timeout(time(NULL));
    }
}





