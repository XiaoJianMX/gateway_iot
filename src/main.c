#include "../inc/main.h"
#include <stdint.h>
#include "../inc/pthread_sql.h"
#include "../inc/pthread_lora.h"
#include "../inc/pthread_socket.h"
#include <signal.h>

//如果是ctrl + c 信号，退出程序
static void sigint_handler(int signum)
{
    if(signum == SIGINT){
        printf("SIGINT received, exiting...\n");
        // 释放资源
        envDataNode_destroy(&envDataNode);
        // 释放线程池
        threadpool_destroy(pool,THREADPOOL_SHUTDOWN);
        // 释放队列
        free(queueOfDevice);
        //释放客户端连接内存池
        client_info_distroy();
        //日志打印
        MY_LOG_FATAL("SIGINT received, exiting...");
        // 关闭日志文件
        logger_close();
        // 退出程序
        exit(0);
    }
}

//存储设备ID队列
uint8_t *queueOfDevice;
// 环境数据队列结构体
envDataNode_t envDataNode;
// 初始化条件变量和互斥锁
pthread_cond_t sql_cont = PTHREAD_COND_INITIALIZER;
pthread_mutex_t revdata_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;


int main(int argc, char *argv[])
{   
    mtrace();
    //初始化日志系统
    logger_init(LOG_LEVEL_INFO, LOG_TARGET_CONSOLE | LOG_TARGET_FILE,NULL);
#ifdef COMPILE_TIME
    time_t now = time(NULL);

    if(now < 946684800){
        MY_LOG_FATAL("time error");
        char command[40];
        snprintf(command, sizeof(command), "date -s \"%s\"", COMPILE_TIME);
        system(command);
    }
#endif
    //初始化clent——info池子
    client_info_init();
    //线程ID
    pthread_t lora_pthread , sql_pthread;
    // 初始化环境数据队列
    envDataNode_init(&envDataNode);
    // 创建文件：存储设备数量
    create_deviceNumFile();
    int deviceNum = read_deviceNumFile();
    // 初始化设备数量队列
    queueOfDevice = (uint8_t *)malloc(sizeof(uint8_t) * deviceNum);
    if (queueOfDevice == NULL)
    {
        printf("queueOfDevice malloc error\n");
        MY_LOG_ERROR("queueOfDevice malloc error");
        return -1;
    }
    char buffer[20];

    for(int i = 0; i < deviceNum; i++){
        queueOfDevice[i] = i + 1;
        //创建数据库
        sprintf(buffer, "envData%d", i + 1);
        createOrOpen_SqliteDB(DBFILENAME,buffer);
    }
    //绑定 ctrl + c 信号
    struct sigaction sigact;
    sigact.sa_handler = sigint_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    if (sigaction(SIGINT, &sigact, NULL) == -1) {
        perror("sigaction");
        return -1;
    }
    //创建线程
    pthread_create(&lora_pthread, NULL, pthread_Lora_Recive, NULL);
    pthread_create(&sql_pthread, NULL, pthread_Sqlite_Write, NULL);
    pthread_detach(lora_pthread);
    pthread_detach(sql_pthread);
while(1){
    pthread_socket();
}
    // 等待线程退出
    printf("main thread exit\n");
    return 0;

}