#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <time.h>

#include "log.h"

FILE *access_log;
FILE *error_log;
time_t timer;

int logger_init()
{
    char fileName[32];
    memset(fileName, 0, 32);
    time(&timer);
    struct tm *now_tm = localtime(&timer);
    sprintf(fileName, "logs/access_%d_%d_%d.log", now_tm->tm_year + 1900, now_tm->tm_mon, now_tm->tm_mday);
    access_log = fopen(fileName, "a+");
    if (access_log == NULL)
    {
        fprintf(stderr, "Cannot R/W AccessLogFile\n");
        return 1;
    }
    sprintf(fileName, "logs/error_%d_%d_%d.log", now_tm->tm_year + 1900, now_tm->tm_mon, now_tm->tm_mday);
    error_log = fopen(fileName, "a+");
    if (error_log == NULL)
    {
        fprintf(stderr, "Cannot R/W ErrorLogFile\n");
        return 1;
    }
    return 0;
}

int log_acc(Request *request, int response_code, int response_size)
{
    char log[1024];
    memset(log, 0, 1024);
    struct tm *now_tm = localtime(&timer);
    if (request == NULL)
    {
        sprintf(log, "%s -- [%d/%d/%d %d:%d:%d] \"%s\" %d %d\n", get_client_ip(), now_tm->tm_mon, now_tm->tm_mday,
                now_tm->tm_year + 1900, now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec,
                "BAD REQUEST", response_code, response_size);
    }
    else
    {
        sprintf(log, "%s -- [%d/%d/%d %d:%d:%d] \"%s %s %s\" %d %d\n", get_client_ip(), now_tm->tm_mon, now_tm->tm_mday,
                now_tm->tm_year + 1900, now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec,
                request->http_version, request->http_uri, request->http_method, response_code, response_size);
    }
    if (fwrite(log, strlen(log), 1, access_log) == 1)
    {
        fflush(access_log);
        return 0;
    }
    else
    {
        return 1;
    }
    fclose(access_log);
}

int log_err(char *err_level, char *err_message)
{
    char log[1024];
    memset(log, 0, 1024);
    struct tm *now_tm = localtime(&timer);
    sprintf(log, "%s -- [%d/%d/%d %d:%d:%d] [%s] \n%s\n", get_client_ip(), now_tm->tm_mon, now_tm->tm_mday,
            now_tm->tm_year + 1900, now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec,
            err_level, err_message);

    if (fwrite(log, strlen(log), 1, error_log) == 1)
    {
        fflush(error_log);
        return 0;
    }
    else
    {
        return 1;
    }
    fclose(error_log);
}

char *get_client_ip()
{
    return "www.cs.cmu.edu";
}