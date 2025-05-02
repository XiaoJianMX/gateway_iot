#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../inc/logger.h"
// 线程池关闭标志
#define THREADPOOL_GRACEFUL 1   // 优雅关闭：等待所有任务完成
#define THREADPOOL_IMMEDIATE 2  // 立即关闭：不等待任务完成

// 线程池状态
#define THREADPOOL_RUNNING 0    // 运行中
#define THREADPOOL_SHUTDOWN 1   // 关闭中
// 任务结构体
typedef struct {
    void *(*func)(void *);
    void *arg;
}threadpool_task_t;

// 线程池结构体
typedef struct {
    pthread_mutex_t lock;       // 互斥锁
    pthread_cond_t notify;      // 条件变量
    pthread_t *threads;         // 工作线程数组
    threadpool_task_t *queue;   // 任务队列
    int thread_count;           // 线程数量
    int queue_size;             // 队列大小
    int head;                   // 队列头
    int tail;                   // 队列尾
    int count;                  // 当前任务数
    int shutdown;               // 关闭标志
    int started;                // 已启动的线程数
} threadpool_t;

threadpool_t *threadpool_create(uint16_t thread_count, uint16_t queue_size);
bool threadpool_add(threadpool_t *pool, void *(*func)(void *arg), void *arg);
bool threadpool_destroy(threadpool_t *pool, int shutdown);
int threadpool_thread_count(threadpool_t *pool) ;
int threadpool_queue_size(threadpool_t *pool);
#endif