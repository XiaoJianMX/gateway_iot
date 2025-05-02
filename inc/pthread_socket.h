#ifndef PTHREAD_SOCKET_H
#define PTHREAD_SOCKET_H
#include "../inc/main.h"
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include "../inc/thread_pool.h"


extern threadpool_t *pool;

void pthread_socket(void);
void client_info_init(void);
void client_info_distroy(void);

typedef int deviceID_t;

typedef enum {
    REQUEST_ONE_SQL = 0,
    REQUEST_20_SQL,
    CMD_SET,
    CMD_GET,
    CMD_OTHER
}REQ_CMD_E;

typedef struct {
    REQ_CMD_E cmd;
    deviceID_t deviceID;
}ReqPacket;

typedef struct {
    ReqPacket *req;
    volatile int client_fd;
    pthread_t tid;
    bool in_use;
}client_info_t;

typedef struct {
    char buffer[4096];
    int buffer_size;
}send_buffer_t;

#endif