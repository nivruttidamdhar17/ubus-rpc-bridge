#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <time.h>
#include <stdarg.h>

// ============== LOG MACROS ==============
#define LOG_INFO    0
#define LOG_DEBUG   1
#define LOG_WARN    2
#define LOG_ERROR   3

static inline void log_msg(int level, const char *fmt, ...)
{
    const char *level_str[] = {"[INFO]", "[DEBUG]", "[WARN]", "[ERROR]"};
    time_t t = time(NULL);  // Get current timestamp
    struct tm *tm_info = localtime(&t);     // Convert to local time
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);     // Format time string

    printf("%s %s ", time_buf, level_str[level]);

    // Handle variable arguments
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");

    fflush(stdout); // Force immediate output
}

#define log_info(fmt, ...) log_msg(LOG_INFO, fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...) log_msg(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...) log_msg(LOG_WARN, fmt, ##__VA_ARGS__)
#define log_error(fmt, ...) log_msg(LOG_ERROR, fmt, ##__VA_ARGS__)

#endif
