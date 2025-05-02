#include "../inc/thread_pool.h"
/**
 * @brief 线程池工作线程的入口函数，负责从任务队列中取出任务并执行。
 * 
 * 该函数会在一个无限循环中等待任务队列中有新任务，若有则取出任务执行；
 * 若线程池关闭且任务队列为空，则退出循环。
 * 
 * @param arg 指向线程池结构体 thread_pool_t 的指针，通过该指针访问线程池的相关信息。
 * @return void* 该函数不会返回有效指针，最终会通过 pthread_exit 退出线程。
 */
static void *thread_pool_worker(void *arg)
{
    threadpool_t *pool = (threadpool_t *)arg;
    threadpool_task_t task;
    while (1)
    {
        pthread_mutex_lock(&(pool->lock));
        while(pool->count == 0 && (!pool->shutdown)){
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }
        if(pool->shutdown == THREADPOOL_SHUTDOWN && pool->count == 0){
            break;
        }
        //取出任务
        task.func = pool->queue[pool->head].func;
        task.arg = pool->queue[pool->head].arg;
        pool->head = (pool->head + 1) % pool->queue_size;
        pool->count --;
        pthread_mutex_unlock(&(pool->lock));
        //执行任务
        (*(task.func))(task.arg);
    }
    pool->started --;
    pthread_mutex_unlock(&(pool->lock));
    pthread_exit(NULL);
    return NULL;
}

/**
 * @brief 创建一个新的线程池。
 * 
 * 该函数会分配内存来创建线程池结构体，初始化线程池的各项参数，
 * 分配线程数组和任务队列的内存，初始化互斥锁和条件变量，
 * 并创建指定数量的工作线程。
 * 
 * @param thread_count 线程池中的线程数量，必须大于 0。
 * @param queue_size 任务队列的大小，必须大于 0。
 * @return thread_pool_t* 若线程池创建成功，返回指向新创建线程池的指针；否则返回 NULL。
 */
threadpool_t *threadpool_create(uint16_t thread_count, uint16_t queue_size)
{   
    if(thread_count <= 0 || queue_size <= 0) return NULL;
    threadpool_t *pool = (threadpool_t *)malloc(sizeof(threadpool_t));
    if (pool == NULL) return NULL;
    memset(pool, 0, sizeof(threadpool_t));
    pool->thread_count = thread_count;
    pool->queue_size = queue_size;
    pool->shutdown = THREADPOOL_RUNNING;

    pool->threads  = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    pool->queue = (threadpool_task_t *)malloc(sizeof(threadpool_task_t) * queue_size);
    if(pool->threads == NULL || pool->queue == NULL){
        if(pool->threads != NULL) free(pool->threads);
        if(pool->queue!= NULL) free(pool->queue);
        free(pool);
        MY_LOG_ERROR("thread_pool_create malloc error");
        return NULL;
    }
    //初始化互斥锁和条件变量
    if(pthread_mutex_init(&(pool->lock), NULL) != 0 || pthread_cond_init(&(pool->notify), NULL) != 0){
        free(pool->threads);
        free(pool->queue);
        free(pool);
        MY_LOG_ERROR("thread_pool_create mutex or cond init error");
        return NULL;
    }
    //创建线程
    for(int i = 0; i < thread_count; i++){
        if(pthread_create(&(pool->threads[i]), NULL, thread_pool_worker, (void *)pool) != 0){
            threadpool_destroy(pool, 0);
            MY_LOG_ERROR("thread_pool_create pthread_create error");
            return NULL;
        }
        pool->started ++;
    }
    return pool;
}
/**
 * @brief 向线程池的任务队列中添加一个新任务。
 * 
 * 该函数会尝试将指定的任务函数及其参数添加到线程池的任务队列中。
 * 在添加任务前，会检查线程池状态和任务队列容量，若条件不满足则添加失败。
 * 
 * @param pool 指向线程池结构体的指针，代表要添加任务的线程池。
 * @param func 指向要执行的任务函数的指针，该函数接受一个 void* 类型的参数。
 * @param arg 传递给任务函数的参数。
 * @return bool 若任务成功添加到队列中，返回 true；否则返回 false。
 */
bool threadpool_add(threadpool_t *pool, void *(*func)(void *arg), void *arg)
{
    if(pool == NULL || func == NULL) return false;
    pthread_mutex_lock(&(pool->lock));
    while(pool->count == pool->queue_size){
        pthread_mutex_unlock(&(pool->lock));
        MY_LOG_ERROR("thread_pool_add queue is full");
        return false;
    }
    if(pool->shutdown){
        pthread_mutex_unlock(&(pool->lock));
        MY_LOG_ERROR("thread_pool_add thread pool is shutdown");
        return false;
    }
    //添加任务
    pool->queue[pool->tail].func = func;
    pool->queue[pool->tail].arg = arg;
    pool->tail = (pool->tail + 1) % pool->queue_size;
    pool->count ++;
    
    if(pthread_cond_signal(&(pool->notify))!= 0){
        pthread_mutex_unlock(&(pool->lock));
        MY_LOG_ERROR("thread_pool_add pthread_cond_broadcast error");
        return false;
    }
    pthread_mutex_unlock(&(pool->lock));
    return true;
}
/**
 * @brief 销毁线程池，释放相关资源并等待所有线程结束。
 * 
 * 该函数会标记线程池为关闭状态，唤醒所有等待的线程，
 * 等待所有工作线程结束，然后销毁互斥锁和条件变量，
 * 最后释放线程池占用的内存资源。
 * 
 * @param pool 指向要销毁的线程池结构体的指针。
 * @param shutdown 关闭标志，用于指定线程池的关闭方式。
 * @return bool 若线程池成功销毁，返回 true；否则返回 false。
 */
bool threadpool_destroy(threadpool_t *pool, int shutdown)
{
    if(pool == NULL)  return false;
    if(pool->shutdown) return false;
    pool->shutdown = shutdown;
    // *** 问题点：在这里解锁可能太早 ***
    if(pthread_cond_broadcast(&(pool->notify)) != 0 || pthread_mutex_unlock(&(pool->lock))!= 0){
        MY_LOG_ERROR("thread_pool_destroy pthread_cond_broadcast or pthread_mutex_unlock error");
        // 注意：如果 broadcast 成功但 unlock 失败，锁仍然被持有，后续 join 会死锁
        // 更好的错误处理可能是尝试解锁然后返回错误，或者记录错误并继续尝试清理
        return false;
    }

    // 主线程在这里等待所有工作线程结束，但此时锁已释放
    for(int i = 0; i < pool->thread_count; i++){
        if(pthread_join(pool->threads[i], NULL)!= 0){
            MY_LOG_ERROR("thread_pool_destroy pthread_join error");
            // 如果 join 失败，可能需要更复杂的错误处理
            // return false; // 或者继续尝试 join 其他线程
        }
    }
    if(pthread_mutex_destroy(&(pool->lock))!= 0 || pthread_cond_destroy(&(pool->notify))!= 0){
        MY_LOG_ERROR("thread_pool_destroy pthread_mutex_destroy or pthread_cond_destroy error");
        return false;
    }
    free(pool->threads);
    free(pool->queue);
    free(pool);
    return true;
}

/**
 * @brief 获取线程池中的线程数量
 * @param pool 线程池指针
 * @return 线程数量
 */
int threadpool_thread_count(threadpool_t *pool) 
{
    if (pool == NULL) {
        return -1;
    }
    
    pthread_mutex_lock(&(pool->lock));
    int count = pool->thread_count;
    pthread_mutex_unlock(&(pool->lock));
    
    return count;
}
/**
 * @brief 获取线程池中的任务数量
 * @param pool 线程池指针
 * @return 任务数量
 */
int threadpool_queue_size(threadpool_t *pool) 
{
    if (pool == NULL) {
        return -1;
    }
    
    pthread_mutex_lock(&(pool->lock));
    int count = pool->count;
    pthread_mutex_unlock(&(pool->lock));
    
    return count;
}
