#include "log.h"
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

int global_out_level;
//static FILE* file=NULL;
static int file = -1;
static pthread_mutex_t mutex;

static char* err_levels[] = { 
	"unknown",
	"emerg",
	"alert",
	"crit",
	"error",
	"warn",
	"notice",
	"info",
	"debug"
};

void initLogInfo()
{
#if (DEBUG)
	global_out_level=LOG_DEBUG;
#else 
	global_out_level=LOG_NOTICE;
#endif
	pthread_mutex_lock (&mutex);
	//file=fopen("error.log","a+");
    file = open("error.log",O_RDWR|O_CREAT|O_APPEND,0644);
    if (file == -1){
        fprintf(stderr,"Open log file error.\n");
    }
    else{
        fprintf(stdout,"file id=%d\n",file);
    }
	pthread_mutex_unlock (&mutex);
}

static struct timeval getTime()
{
	struct timeval tp;
	gettimeofday(&tp,NULL);
	return tp;
}

#if 0
void logInfoForSel(int level,const char *fmt, va_list args)
{
	struct tm localTime;
	time_t t;
	char timeStr[32];
	struct tm* pLocalTime=NULL;
	char* pTimeStr=NULL;
	size_t len=0;
	struct timeval usec=getTime();
	if(global_out_level >= level)
	{
		pthread_mutex_lock (&mutex);
		if (file) {
			t=time(0);
			fprintf(file,"[%s] ",err_levels[level]);
			pLocalTime=localtime_r(&t,&localTime);
			if(NULL == pLocalTime)
			{
				return;
			}
			pTimeStr=asctime_r(pLocalTime,timeStr);
			if(NULL == pTimeStr)
			{
				return;
			}
			len=strlen(pTimeStr);
			pTimeStr[len-1]='\0';
			fprintf(file,"%s usec=%ld ",pTimeStr,usec.tv_usec);
			(void)vfprintf(file, fmt, args);
			fprintf( file, "\n" );
		}
		pthread_mutex_unlock (&mutex);
	}
}
#endif

void logInfo(int level,const char *fmt, ...)
{
	va_list args;
	struct tm localTime;
	time_t t;
    
    char buffer[2048];
    char *p;
    size_t len = 0;
    int n=0;
    
	char timeStr[32];
	struct tm* pLocalTime=NULL;
	char* pTimeStr=NULL;

	struct timeval usec=getTime();
    
	if(global_out_level >= level)
	{
		pthread_mutex_lock (&mutex);
        
		if (file > 0) {
        
            p = buffer;
            
			t=time(0);
            
            len = sprintf(p,"[%s]",err_levels[level]);
            
            p += len;
            *p++ = ' ';

			pLocalTime=localtime_r(&t,&localTime);
			if(NULL == pLocalTime)
			{
				return;
			}
			pTimeStr=asctime_r(pLocalTime,timeStr);
			if(NULL == pTimeStr)
			{
				return;
			}
			n=strlen(pTimeStr);
			pTimeStr[n-1]='\0';
			
            len = sprintf(p,"%s usec=%ld ",pTimeStr,usec.tv_usec);
            p += len;

			va_start(args, fmt);
            
            len = vsprintf(p,fmt,args);
            p += len;
            *p++ = '\n';
            
			va_end(args);
            
            write(file,buffer,p-buffer);
		}
		pthread_mutex_unlock (&mutex);
	}
}

void endLogInfo()
{
	pthread_mutex_lock (&mutex);
	if(file > 0)
	{
		//(void)fclose(file);
        close(file);
		//file=NULL;
        file = -1;
	}	
	pthread_mutex_unlock (&mutex);
}

