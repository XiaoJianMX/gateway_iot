#ifndef LOGGER_H
#define LOGGER_H

#include <syslog.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdarg.h>
//  * @brief 日志级别
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
    LOG_LEVEL_TRACE,
    LOG_LEVEL_WARN
} log_level;
//  * @brief 日志目标
typedef enum {
    LOG_TARGET_CONSOLE = 0x01,
    LOG_TARGET_FILE = 0x02,
    LOG_TARGET_SYSLOG = 0x04
}log_target;
//  * @brief 日志配置结构体
typedef struct {
    log_level level;
    uint32_t target;
    char file_path[64];
    uint32_t max_file_size;//一个文件的最大大小，单位为字节
    uint32_t max_file_count;//最多保留的文件数量
    pthread_mutex_t mutex;
}log_config_t;

int logger_init(log_level level, uint32_t target, char *file_path);
void logger_log(log_level level, const char *file_name, int line, const char *format, ...);
void logger_set_level(log_level level);
void logger_set_target(uint32_t target);
void logger_set_file_path(const char *file_path);
void logger_set_max_file_size(uint32_t max_file_size);
void logger_close(void);

// 日志宏定义，方便使用
#define MY_LOG_TRACE(fmt, ...)     logger_log(LOG_LEVEL_TRACE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define MY_LOG_DEBUG(fmt, ...)     logger_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define MY_LOG_INFO(fmt, ...)      logger_log(LOG_LEVEL_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define MY_LOG_WARN(fmt, ...)      logger_log(LOG_LEVEL_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define MY_LOG_ERROR(fmt, ...)     logger_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define MY_LOG_FATAL(fmt, ...)     logger_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif