#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <pthread.h>
#include "libevent_socket.h"


#define MAX_CONNECTIONS   (100000)
#define TIMEOUT_SEC       (30)   /*300 seconds*/

static fd_t g_fd[MAX_CONNECTIONS];
pthread_mutex_t fd_lock = PTHREAD_MUTEX_INITIALIZER;

int set_socket_nonblock(int fd)
{
    int flag;
    flag = fcntl(fd,F_GETFL,NULL);
    if (flag < 0){
        return -1;
    }
    
    flag |= O_NONBLOCK;
    
    if (fcntl(fd,F_SETFL,flag) < 0){
        return -1;
    }
    
    return 0;
}

int set_socket_linger(int fd)
{
    struct linger linger;
    
    linger.l_onoff = 1;
    linger.l_linger = 0;
    
    return setsockopt(fd,SOL_SOCKET,SO_LINGER,&linger,sizeof(struct linger));
}

int set_socket_reusable(int fd)
{
    int reuse_on = 1;
    return setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&reuse_on,sizeof(reuse_on));
}

int socket_setup(int nPort)
{
    int listenfd;
    struct sockaddr_in listen_addr;
    int reuse = 1;
    
    listenfd = socket(AF_INET,SOCK_STREAM,0);
    if (listenfd < 0){
        fprintf(stderr,"Failed to create socket.\n");
        return -1;
    }
    
    /*set socket reuseable*/
    if (set_socket_reusable(listenfd) < 0){
        fprintf(stderr,"Failed to set listening socket re-usable.\n");
        return -1;
    }
    
    /*set socket non-blocking*/
    if (set_socket_nonblock(listenfd) < 0){
        fprintf(stderr,"Failed to set listening socket non-blocking.\n");
        return -1;
    }
    
    memset(&listen_addr,0,sizeof(struct sockaddr_in));
    listen_addr.sin_family      = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port        = htons(nPort);
    if (bind(listenfd,(struct sockaddr*)&listen_addr,sizeof(listen_addr)) < 0){
        fprintf(stderr,"Failed to bind the listening socket fd.\n");
        return -1;
    }
    
    if (listen(listenfd,BACKLOG) < 0){
        fprintf(stderr,"Failed to set the listening socket to listen.\n");
        return -1;
    }
    
    return listenfd;
}

/*check socket fd close_wait timeout*/

void fd_init(void)
{
    int i;
    
    for (i=0; i<MAX_CONNECTIONS; i++){
        g_fd[i].fd = -1;
        g_fd[i].last_time = 0;
    }
}

int fd_insert(int fd)
{
    if (fd < 0){
        return -1;
    }
    
    if (fd > MAX_CONNECTIONS-1){
        return -1;
    }
    
    pthread_mutex_lock(&fd_lock);
    
    g_fd[fd].fd = fd;
    g_fd[fd].last_time = time(NULL);
    printf("fd=%d is inserted, last_time=%u\n",fd,g_fd[fd].last_time);
    
    pthread_mutex_unlock(&fd_lock);
    
    return 0;
}

int fd_del(int fd)
{
    if (fd < 0){
        return -1;
    }
    
    printf("fd_del: fd=%d\n",fd);
    
    close(fd);
    pthread_mutex_lock(&fd_lock);
    g_fd[fd].fd = -1;
    g_fd[fd].last_time = 0;
    pthread_mutex_unlock(&fd_lock);
    return 0;
}

int fd_update_last_time(int fd,time_t last_time)
{
    int ret = -1;
    
    pthread_mutex_lock(&fd_lock);
    
    if (g_fd[fd].fd != -1){
        g_fd[fd].last_time = last_time;
        ret = 0;
    }
    
    pthread_mutex_unlock(&fd_lock);
    
    return ret;
}

int check_closewait_timeout(time_t now)
{
    int i;
    
//    pthread_mutex_lock(&fd_lock);
    for (i=0; i<MAX_CONNECTIONS; i++){
        if (g_fd[i].fd != -1){
            fprintf(stderr,"i=%d,g_fd[%d].fd=%d,last_time=%u\n",i,i,g_fd[i].fd,g_fd[i].last_time);
            
            if (now - g_fd[i].last_time >= TIMEOUT_SEC){
                fprintf(stderr,"fd=%d will be closed.\n",g_fd[i].fd);
                fd_del(g_fd[i].fd);
            }
        }
    }
//    pthread_mutex_unlock(&fd_lock);
}
