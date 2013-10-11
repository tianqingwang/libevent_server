#ifndef  _LOG_H_INC
#define  _LOG_H_INC

#include <stdio.h>
#include <string.h>
#include <unistd.h> 
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" 
{
#endif

#define LOG_STDERR            0
#define LOG_EMERG             1
#define LOG_ALERT             2
#define LOG_CRIT              3
#define LOG_ERR               4
#define LOG_WARN              5
#define LOG_NOTICE            6
#define LOG_INFO              7
#define LOG_DEBUG             8 

#define DEBUG		          1

	void initLogInfo();
#if 0
	void logInfoForSel(int level,const char *fmt, va_list args);
#endif
	void logInfo(int level,const char *fmt, ...);
	void endLogInfo();

#ifdef __cplusplus
}
#endif


#endif  

