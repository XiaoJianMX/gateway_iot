#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include "../inc/thread_pool.h"

// 看门狗状态码
typedef enum {
    WATCHDOG_OK = 0,           // 系统正常
    WATCHDOG_THREAD_DEAD,      // 线程死亡
    WATCHDOG_THREAD_STUCK,     // 线程卡住
    WATCHDOG_QUEUE_FULL,       // 队列满
    WATCHDOG_NETWORK_ERROR,    // 网络错误
    WATCHDOG_MEMORY_ERROR      // 内存错误
} watchdog_status_t;

// 看门狗配置
typedef struct {
    unsigned int check_interval;    // 检查间隔(秒)
    unsigned int max_restart_count; // 最大重启次数
    bool auto_restart;              // 是否自动重启
} watchdog_config_t;

// 组件健康状态
typedef struct {
    time_t last_active_time;        // 最后活动时间
    unsigned int restart_count;     // 重启次数
    bool is_healthy;                // 是否健康
    pthread_t thread_id;            // 线程ID
    char component_name[32];        // 组件名称
} component_health_t;

// 初始化看门狗
bool watchdog_init(watchdog_config_t *config);

// 启动看门狗
bool watchdog_start();

// 停止看门狗
void watchdog_stop();

// 组件心跳更新
void watchdog_update_heartbeat(const char *component_name);

// 检查组件健康状态
watchdog_status_t watchdog_check_health(const char *component_name);

// 重启组件
bool watchdog_restart_component(const char *component_name);

// 注册组件到看门狗
bool watchdog_register_component(const char *component_name, pthread_t thread_id);

// 获取看门狗状态
const char* watchdog_status_to_string(watchdog_status_t status);

#endif /* __WATCHDOG_H__ */