#include "../inc/watchdog.h"
#include "../inc/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

#define MAX_COMPONENTS                  10
#define DEFAULT_CHECK_INTERVAL          5
#define DEFAULT_MAX_RESTART             3

static watchdog_config_t watchdog_config = {
    .check_interval = DEFAULT_CHECK_INTERVAL,
    .max_restart_count = DEFAULT_MAX_RESTART,
    .auto_restart = true
};

static component_health_t components[MAX_COMPONENTS];
static int component_count = 0;
static pthread_t watchdog_thread;
static pthread_mutex_t watchdog_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool watchdog_running = false;
static time_t watchdog_start_time;

// 看门狗线程函数
static void *watchdog_thread_func(void *arg) {
    MY_LOG_TRACE("看门狗线程已启动");
    
    while (watchdog_running) {
        pthread_mutex_lock(&watchdog_mutex);
        
        time_t current_time = time(NULL);
        
        // 检查所有注册的组件
        for (int i = 0; i < component_count; i++) {
            component_health_t *comp = &components[i];
            
            // 如果组件超过检查间隔没有心跳，标记为不健康
            if (current_time - comp->last_active_time > watchdog_config.check_interval * 2) {
                if (comp->is_healthy) {
                    comp->is_healthy = false;
                    MY_LOG_WARNING("组件 %s 未响应，已超过 %d 秒", 
                                  comp->component_name, 
                                  (int)(current_time - comp->last_active_time));
                    
                    // 如果配置为自动重启，尝试重启组件
                    if (watchdog_config.auto_restart && 
                        comp->restart_count < watchdog_config.max_restart_count) {
                        pthread_mutex_unlock(&watchdog_mutex);
                        watchdog_restart_component(comp->component_name);
                        pthread_mutex_lock(&watchdog_mutex);
                    }
                }
            }
        }
        
        pthread_mutex_unlock(&watchdog_mutex);
        
        // 等待下一个检查周期
        sleep(watchdog_config.check_interval);
    }
    
    MY_LOG_TRACE("看门狗线程已停止");
    return NULL;
}

bool watchdog_init(watchdog_config_t *config) {
    if (config != NULL) {
        watchdog_config = *config;
    }
    
    // 初始化组件数组
    memset(components, 0, sizeof(components));
    component_count = 0;
    watchdog_running = false;
    
    MY_LOG_TRACE("看门狗已初始化，检查间隔: %d秒，最大重启次数: %d", 
                watchdog_config.check_interval, 
                watchdog_config.max_restart_count);
    
    return true;
}

bool watchdog_start() {
    if (watchdog_running) {
        MY_LOG_WARNING("看门狗已经在运行");
        return false;
    }
    
    watchdog_running = true;
    watchdog_start_time = time(NULL);
    
    // 创建看门狗线程
    if (pthread_create(&watchdog_thread, NULL, watchdog_thread_func, NULL) != 0) {
        MY_LOG_ERROR("创建看门狗线程失败");
        watchdog_running = false;
        return false;
    }
    
    // 设置线程为分离状态
    pthread_detach(watchdog_thread);
    
    return true;
}

void watchdog_stop() {
    if (!watchdog_running) {
        return;
    }
    
    watchdog_running = false;
    
    // 等待看门狗线程退出
    pthread_join(watchdog_thread, NULL);
    
    MY_LOG_TRACE("看门狗已停止");
}

void watchdog_update_heartbeat(const char *component_name) {
    if (component_name == NULL) {
        return;
    }
    
    pthread_mutex_lock(&watchdog_mutex);
    
    // 查找组件并更新心跳时间
    for (int i = 0; i < component_count; i++) {
        if (strcmp(components[i].component_name, component_name) == 0) {
            components[i].last_active_time = time(NULL);
            components[i].is_healthy = true;
            pthread_mutex_unlock(&watchdog_mutex);
            return;
        }
    }
    
    pthread_mutex_unlock(&watchdog_mutex);
    MY_LOG_WARNING("尝试更新未注册组件的心跳: %s", component_name);
}

watchdog_status_t watchdog_check_health(const char *component_name) {
    if (component_name == NULL) {
        return WATCHDOG_MEMORY_ERROR;
    }
    
    pthread_mutex_lock(&watchdog_mutex);
    
    // 查找组件并检查健康状态
    for (int i = 0; i < component_count; i++) {
        if (strcmp(components[i].component_name, component_name) == 0) {
            bool is_healthy = components[i].is_healthy;
            time_t last_active = components[i].last_active_time;
            pthread_mutex_unlock(&watchdog_mutex);
            
            if (!is_healthy) {
                return WATCHDOG_THREAD_STUCK;
            }
            
            time_t current_time = time(NULL);
            if (current_time - last_active > watchdog_config.check_interval * 2) {
                return WATCHDOG_THREAD_DEAD;
            }
            
            return WATCHDOG_OK;
        }
    }
    
    pthread_mutex_unlock(&watchdog_mutex);
    return WATCHDOG_MEMORY_ERROR;
}

bool watchdog_restart_component(const char *component_name) {
    if (component_name == NULL) {
        return false;
    }
    
    pthread_mutex_lock(&watchdog_mutex);
    
    // 查找组件
    int comp_index = -1;
    for (int i = 0; i < component_count; i++) {
        if (strcmp(components[i].component_name, component_name) == 0) {
            comp_index = i;
            break;
        }
    }
    
    if (comp_index == -1) {
        pthread_mutex_unlock(&watchdog_mutex);
        MY_LOG_ERROR("尝试重启未注册的组件: %s", component_name);
        return false;
    }
    
    // 增加重启计数
    components[comp_index].restart_count++;
    pthread_t thread_id = components[comp_index].thread_id;
    
    MY_LOG_WARNING("正在重启组件: %s (重启次数: %d)", 
                  component_name, 
                  components[comp_index].restart_count);
    
    pthread_mutex_unlock(&watchdog_mutex);
    
    // 尝试终止旧线程
    if (pthread_kill(thread_id, 0) == 0) {
        // 线程存在，尝试终止
        pthread_cancel(thread_id);
    }
    
    // 根据组件名称重启相应的线程
    if (strcmp(component_name, "thread_pool") == 0) {
        // 重启线程池逻辑
        // 注意：这里需要根据实际情况实现
        MY_LOG_WARNING("重启线程池");
        // 这里应该调用相关函数重新初始化线程池
        
        // 更新组件状态
        pthread_mutex_lock(&watchdog_mutex);
        components[comp_index].last_active_time = time(NULL);
        components[comp_index].is_healthy = true;
        pthread_mutex_unlock(&watchdog_mutex);
        
        return true;
    } 
    else if (strcmp(component_name, "socket_thread") == 0) {
        // 重启套接字线程逻辑
        MY_LOG_WARNING("重启套接字线程");
        // 这里应该调用相关函数重新初始化套接字线程
        
        // 更新组件状态
        pthread_mutex_lock(&watchdog_mutex);
        components[comp_index].last_active_time = time(NULL);
        components[comp_index].is_healthy = true;
        pthread_mutex_unlock(&watchdog_mutex);
        
        return true;
    }
    
    MY_LOG_ERROR("不支持重启组件类型: %s", component_name);
    return false;
}

bool watchdog_register_component(const char *component_name, pthread_t thread_id) {
    if (component_name == NULL) {
        return false;
    }
    
    pthread_mutex_lock(&watchdog_mutex);
    
    // 检查是否已经注册
    for (int i = 0; i < component_count; i++) {
        if (strcmp(components[i].component_name, component_name) == 0) {
            // 更新现有组件
            components[i].thread_id = thread_id;
            components[i].last_active_time = time(NULL);
            components[i].is_healthy = true;
            components[i].restart_count = 0;
            pthread_mutex_unlock(&watchdog_mutex);
            MY_LOG_TRACE("更新已注册组件: %s", component_name);
            return true;
        }
    }
    
    // 检查是否达到最大组件数
    if (component_count >= MAX_COMPONENTS) {
        pthread_mutex_unlock(&watchdog_mutex);
        MY_LOG_ERROR("已达到最大组件数量，无法注册: %s", component_name);
        return false;
    }
    
    // 注册新组件
    strncpy(components[component_count].component_name, component_name, sizeof(components[component_count].component_name) - 1);
    components[component_count].thread_id = thread_id;
    components[component_count].last_active_time = time(NULL);
    components[component_count].is_healthy = true;
    components[component_count].restart_count = 0;
    
    component_count++;
    
    pthread_mutex_unlock(&watchdog_mutex);
    MY_LOG_TRACE("已注册新组件: %s", component_name);
    
    return true;
}

const char* watchdog_status_to_string(watchdog_status_t status) {
    switch (status) {
        case WATCHDOG_OK:
            return "正常";
        case WATCHDOG_THREAD_DEAD:
            return "线程死亡";
        case WATCHDOG_THREAD_STUCK:
            return "线程卡住";
        case WATCHDOG_QUEUE_FULL:
            return "队列满";
        case WATCHDOG_NETWORK_ERROR:
            return "网络错误";
        case WATCHDOG_MEMORY_ERROR:
            return "内存错误";
        default:
            return "未知状态";
    }
}