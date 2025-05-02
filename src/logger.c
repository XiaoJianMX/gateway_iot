#include "logger.h"
#define             LOG_FILE_PATH           "./log/"

const char *log_level_strings[] = {
    "DEBUG",
    "INFO",
    "ERROR",
    "FATAL",
    "TRACE",
    "WARN",
};

//  * @brief 日志配置结构体
static log_config_t log_config = {
    .level = LOG_LEVEL_DEBUG,
    .target = LOG_TARGET_CONSOLE | LOG_TARGET_FILE,
    .file_path = LOG_FILE_PATH,
    .max_file_size = 1024,
    .max_file_count = 9
};
//  * @brief 日志文件指针
static FILE *log_file = NULL;
/**
 * @brief 初始化日志系统
 * 
 * 该函数用于初始化日志系统的配置，包括日志级别、日志输出目标和日志文件路径。
 * 同时会检查日志文件所在目录是否存在，若不存在则尝试创建，最后打开日志文件。
 * 
 * @param level 日志级别，用于过滤低于该级别的日志输出
 * @param target 日志输出目标，可通过位运算组合不同的输出目标
 * @param file_path 日志文件的路径，若为 NULL 则使用默认路径
 * @return int 初始化成功返回 0，失败返回 -1
 */
int logger_init(log_level level, uint32_t target, char *file_path) {
    if (pthread_mutex_init(&log_config.mutex, NULL) != 0) {
        perror("pthread_mutex_init");
        return -1;
    }
    if(level != LOG_LEVEL_DEBUG){
        log_config.level = level;
    }
    if(target!= (LOG_TARGET_CONSOLE|LOG_TARGET_FILE)){
        log_config.target = target;
    }
    if(file_path != NULL){
        strncpy(log_config.file_path, file_path, strlen(file_path));
        log_config.file_path[ strlen(file_path) + 1] = '\0';
    }
    char dir_path[1024];
    struct stat st;
    char *last_slash = strrchr(log_config.file_path, '/');
    strncpy(dir_path, log_config.file_path, last_slash - log_config.file_path );
    dir_path[last_slash - log_config.file_path] = '\0';
#ifdef DEBUG
    printf("dir_path: %s\n", dir_path);
#endif // DEBUG
    if (stat(log_config.file_path, &st) == -1) {
        if(mkdir(dir_path, 0755) == -1){
            perror("mkdir");
            return -1;
        }
    }
    char file_name[64];
    FILE *temp_file;
    for(int i = 0; i < log_config.max_file_count; i++){
        snprintf(file_name, sizeof(file_name), "%s/%d.gateway.log",log_config.file_path,i + 1);
        if (stat(file_name, &st) == -1) {
            temp_file = fopen(file_name, "w");
            if (temp_file == NULL) {
                perror("fopen");
                return -1;
            }
            fclose(temp_file);
        }
    }
    snprintf(file_name, sizeof(file_name), "%s/%d.gateway.log",log_config.file_path,1);
    log_file = fopen(file_name, "a");
    if (log_file == NULL) {
        perror("fopen");
        return -1;
    }
    MY_LOG_INFO("logger initialized, level: %s, targets: 0x%x", log_level_strings[log_config.level], log_config.target);
    return 0;
}
/**
 * @brief 设置日志系统的日志级别
 * 
 * 该函数用于动态设置日志系统的日志级别。在设置之前，会先检查传入的日志级别是否有效。
 * 如果有效，则使用互斥锁保证线程安全，更新日志配置中的日志级别，并记录一条信息日志。
 * 
 * @param level 要设置的日志级别，类型为 log_level 枚举
 */
void logger_set_level(log_level level) {
    if (level < LOG_LEVEL_DEBUG || level > LOG_LEVEL_FATAL) {
        return;
    }
    pthread_mutex_lock(&log_config.mutex);
    log_config.level = level;
    pthread_mutex_unlock(&log_config.mutex);
    MY_LOG_INFO("logger level set to %s", log_level_strings[level]);
}
/**
 * @brief 设置日志系统的输出目标
 * 
 * 该函数用于动态设置日志系统的输出目标，可通过位运算组合不同的输出目标。
 * 使用互斥锁保证线程安全，更新日志配置中的输出目标，并记录一条信息日志。
 * 
 * @param target 要设置的日志输出目标，为无符号 32 位整数，可通过位运算组合
 */
void logger_set_target(uint32_t target) {
    pthread_mutex_lock(&log_config.mutex);
    log_config.target = target;
    pthread_mutex_unlock(&log_config.mutex);
    MY_LOG_INFO("logger targets set to 0x%x", target);
}
/**
 * @brief 设置日志系统的日志文件路径
 * 
 * 该函数用于动态设置日志系统的日志文件路径。
 * 使用互斥锁保证线程安全，避免多个线程同时修改日志文件路径。
 * 复制传入的文件路径到日志配置中，并确保字符串以 '\0' 结尾。
 * 最后记录一条信息日志，表明日志文件路径已被设置为指定值。
 * 
 * @param file_path 要设置的日志文件路径，为字符串指针
 */
void logger_set_file_path(const char *file_path) {
    pthread_mutex_lock(&log_config.mutex);
    strncpy(log_config.file_path, file_path, sizeof(log_config.file_path) - 1);
    log_config.file_path[sizeof(log_config.file_path) - 1] = '\0';
    pthread_mutex_unlock(&log_config.mutex);
    MY_LOG_INFO("logger file path set to %s", log_config.file_path);
}
/**
 * @brief 设置日志系统的最大日志文件大小
 * 
 * 该函数用于动态设置日志系统单个日志文件的最大允许大小。
 * 使用互斥锁保证线程安全，避免多个线程同时修改最大文件大小配置。
 * 设置完成后，会记录一条信息日志，表明最大文件大小已被设置为指定值。
 * 
 * @param max_file_size 要设置的最大日志文件大小，单位由具体实现决定，通常为字节
 */
void logger_set_max_file_size(uint32_t max_file_size) {
    pthread_mutex_lock(&log_config.mutex);
    log_config.max_file_size = max_file_size;
    pthread_mutex_unlock(&log_config.mutex);
    MY_LOG_INFO("logger max file size set to %u", max_file_size);
}
/**
 * @brief 设置日志系统的最大日志文件数量
 * 
 * 该函数用于动态设置日志系统允许保留的最大日志文件数量。
 * 使用互斥锁保证线程安全，避免多个线程同时修改最大文件数量配置。
 * 设置完成后，会记录一条信息日志，表明最大文件数量已被设置为指定值。
 * 
 * @param max_file_count 要设置的最大日志文件数量，为无符号 32 位整数
 */
void logger_set_max_file_count(uint32_t max_file_count) {
    pthread_mutex_lock(&log_config.mutex);
    log_config.max_file_count = max_file_count;
    pthread_mutex_unlock(&log_config.mutex);
    MY_LOG_INFO("logger max file count set to %u", max_file_count);
}
/**
 * @brief 执行日志文件轮转操作
 * 
 * 该函数用于检查当前日志文件是否达到最大文件大小限制，
 * 若达到限制则尝试切换到新的日志文件。若所有日志文件都已满，
 * 则删除最早的日志文件，对其余文件进行重命名，并创建新的日志文件。
 */
static void log_rotate(void) 
{
    if(log_file == NULL) return;
    long file_size = ftell(log_file);
    if(file_size < log_config.max_file_size) return;
    //获取文件大小
    for(int i = 0; i < log_config.max_file_count; i++){
        char file_name[64];
        snprintf(file_name, sizeof(file_name), "%s/%d.gateway.log",log_config.file_path,i + 1);
        FILE *temp_file = fopen(file_name, "r");
        if (temp_file == NULL) {
            MY_LOG_ERROR("log_rotate fopen");
            perror("fopen");
            return;
        }
        file_size = ftell(temp_file);
        if(file_size < log_config.max_file_size){
            log_file = fopen(file_name, "a");
            if (log_file == NULL) {
                MY_LOG_ERROR("log_rotate fopen");
                perror("fopen");
                fclose(temp_file);
                return;
            }
            fclose(temp_file);
            return;
        }else{
            fclose(temp_file);
        }
    }
    //如果程序走到这里，说明所有文件都已满，删除第一个文件，重命名所有文件
    //删除第一个文件
    char rm_file_name[64];
    snprintf(rm_file_name, sizeof(rm_file_name), "%s/%d.gateway.log",log_config.file_path,1);
    remove(rm_file_name);
    //创建新文件
    char old_name[64];
    char new_name[64];
    for(int i = 0; i < log_config.max_file_count - 1; i++){
        //将现有文件重命名
        snprintf(old_name, sizeof(old_name), "%s/%d.gateway.log",log_config.file_path,i + 2);
        snprintf(new_name, sizeof(new_name), "%s/%d.gateway.log",log_config.file_path,i + 1);
        rename(old_name, new_name);
    }
    //创建最新的文件
    char file_name[64];
    snprintf(file_name, sizeof(file_name), "%s/%d.gateway.log",log_config.file_path,log_config.max_file_count + 1);
    log_file = fopen(file_name, "w");
    if (log_file == NULL) {
        perror("fopen");
        return;
    }
}
/**
 * @brief 记录日志信息
 * 
 * 该函数根据指定的日志级别、文件名、行号和格式化字符串记录日志信息。
 * 日志信息会根据配置输出到控制台、文件或系统日志。
 * 
 * @param level 日志级别，用于过滤低于该级别的日志输出
 * @param file_name 调用该日志函数的源文件名
 * @param line 调用该日志函数的代码行号
 * @param format 格式化字符串，用于指定日志信息的格式
 * @param ... 可变参数列表，对应格式化字符串中的占位符
 */
void logger_log(log_level level, const char *file_name, int line, const char *format, ...) {
    if (level < log_config.level) {
        return;
    }
    pthread_mutex_lock(&log_config.mutex);
    //当前系统时间
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local_time);
    //当前线程ID
    pthread_t tid = pthread_self();
    //获取文件名
    const char* tempfile_name = file_name;
    const char* lastSlash = strrchr(file_name, '/');
    if (lastSlash) {
        tempfile_name = lastSlash + 1;
    } else {
        lastSlash = strrchr(file_name, '\\');
        if (lastSlash) {
            tempfile_name = lastSlash + 1;
        }
    }
    //格式化日志消息
    char log_message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(log_message, sizeof(log_message), format, args);
    va_end(args);
    //格式化日志
    char formatted_log[1024];
    snprintf(formatted_log, sizeof(formatted_log), "[%s]    [%s]    [%lu]   [%s:%d]     [%s]\n",
            time_str, log_level_strings[level], (unsigned long)tid, tempfile_name, line,log_message);
    //输出控制台
    if (log_config.target & LOG_TARGET_CONSOLE) {
        const char *color_code = "";
        const char *color_end = "\033[0m";
        switch (level) {
            case LOG_LEVEL_DEBUG:   color_code = "\033[34m";break;//蓝色
            case LOG_LEVEL_INFO:    color_code = "\033[32m";break;//绿色
            case LOG_LEVEL_WARN:    color_code = "\033[33m";break;//黄色
            case LOG_LEVEL_ERROR:   color_code = "\033[31m";break;//红色
            case LOG_LEVEL_FATAL:   color_code = "\033[35m";break;//紫色
            case LOG_LEVEL_TRACE:   color_code = "\033[36m";break;//青色
            default:break;
        }
        fprintf(stdout, "%s%s%s", color_code, formatted_log, color_end);
        fflush(stdout);
    }
    //输出文件
    if ((log_config.target & LOG_TARGET_FILE) && log_file != NULL) {
        log_rotate();
        fputs(formatted_log, log_file);
        fflush(log_file);
    }
    //系统输出
    if (log_config.target & LOG_TARGET_SYSLOG) {
        openlog("gateway", LOG_PID,LOG_USER);
        syslog(LOG_LEVEL_INFO, "%s", formatted_log);
        closelog();
    }
    pthread_mutex_unlock(&log_config.mutex);
}
/**
 * @brief 设置日志系统的最大日志文件大小和最大日志文件数量
 * 
 * 该函数用于同时动态设置日志系统单个日志文件的最大允许大小，
 * 以及日志系统允许保留的最大日志文件数量。
 * 使用互斥锁保证线程安全，避免多个线程同时修改相关配置。
 * 设置完成后，会分别记录一条信息日志，表明最大文件大小和最大文件数量已被设置为指定值。
 * 
 * @param file_size 要设置的最大日志文件大小，单位由具体实现决定，通常为字节
 * @param file_count 要设置的最大日志文件数量，为整数
 */
void logger_set_file_info(int file_size, int file_count) 
{
    pthread_mutex_lock(&log_config.mutex);
    log_config.max_file_size = file_size;
    log_config.max_file_count = file_count;
    pthread_mutex_unlock(&log_config.mutex);
    MY_LOG_INFO("logger max file size set to %u", log_config.max_file_size);
    MY_LOG_INFO("logger max file count set to %u", log_config.max_file_count);    
}
/**
 * @brief 关闭日志系统
 * 
 * 该函数用于关闭日志系统，释放相关资源。具体操作包括关闭日志文件、销毁互斥锁，
 * 并在控制台输出关闭信息。使用互斥锁保证线程安全，避免在关闭过程中其他线程访问日志资源。
 */
void logger_close(void)
{
    pthread_mutex_lock(&log_config.mutex);
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_config.mutex);
    pthread_mutex_destroy(&log_config.mutex);
    // 最后一条日志无法写入文件，因为已经关闭
    fprintf(stdout, "logger closed\n");
}