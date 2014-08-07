#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>

#define LOG_ERROR(format, args...) \
	do { \
		fprintf(errfd, __FILE__ ":%u:Error:" format, __LINE__, ##args); \
	} while(0)

#define LOG(format, args...) \
	do { \
		if (logfd != NULL) { \
			fprintf(logfd, format, ##args); \
		} \
	} while(0)

/** File handle for error output messages. */
extern FILE *errfd;
/** File handle for log output messages. */
extern FILE *logfd;

#endif
