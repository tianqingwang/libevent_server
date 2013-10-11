#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "libevent_socket.h"
#include "libevent_multi.h"

#define NPORT      5000
#define BACKLOG    5
#define MAXTHREADS 3

void read_callback(struct bufferevent *bev,void *arg);
void write_callback(struct bufferevent *bev,void *arg);
void event_callback(struct bufferevent *bev, short events, void *arg);

int main(int argc, char *argv[])
{
    int sockfd = socket_setup(NPORT);
    
    pthread_t test_id; 

    if (sockfd == -1){
        fprintf(stderr,"socket_setup failed.\n");
        return 1;
    }
    /*set worker threads.*/
    
    worker_thread_init(MAXTHREADS);
    set_read_callback(read_callback);
    set_write_callback(write_callback);
    master_thread_loop(sockfd);
    
    return 0;
}

/*sample for read/write/event callback.*/
void read_callback(struct bufferevent *bev,void *arg)
{
    char buf[1024];
    int n;
    /*read buffer in bufferevent*/
    struct evbuffer *input = bufferevent_get_input(bev);
    /*write buffer in bufferevent*/
    struct evbuffer *output = bufferevent_get_output(bev);
    /*connected socket fd in bufferevent.*/
    int fd = bufferevent_getfd(bev);
    
    time_t now = time(NULL);
    fd_update_last_time(fd,now);

    while((n=evbuffer_remove(input,buf,sizeof(buf))) > 0){
        printf("fd=%d,received:%s at thread=0x%x\n",fd,buf,pthread_self());
        evbuffer_add(output,buf,n);
    }

}

void write_callback(struct bufferevent *bev,void *arg)
{
    /*how to use write callback?*/
}

void event_callback(struct bufferevent *bev, short events, void *arg)
{
    /*events from callback*/
    if (events & EV_TIMEOUT){
        fprintf(stderr,"events timeout.\n");
    }
    if (events & BEV_EVENT_ERROR){
        perror("Error from bufferevent.");
    }
    if (events &(BEV_EVENT_ERROR|BEV_EVENT_EOF)){
        bufferevent_free(bev);
    }
    
    if (events &(BEV_EVENT_TIMEOUT|BEV_EVENT_READING)){
        fprintf(stderr,"read timeout.\n");
    }
    if (events & (BEV_EVENT_TIMEOUT | BEV_EVENT_WRITING)){
        fprintf(stderr,"write timeout.\n");
    }
}

