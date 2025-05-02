#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
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
    char buffer[4096];
    int buffer_size;
}send_buffer_t;

int main(int argc, char *argv[])
{   
    send_buffer_t send_buffer;

    ReqPacket *req = (ReqPacket *)malloc(sizeof(ReqPacket));
    req->cmd = REQUEST_20_SQL;
    req->deviceID = 1;


    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8900);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    while (1) {
        char buffer[20];
        int bytes_received = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
        if (bytes_received == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        if (send(sockfd, req, sizeof(*req), 0) == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }
        //读取服务器返回的数据
        memset(buffer, 0, sizeof(buffer));
        bytes_received = read(sockfd, &send_buffer, sizeof(send_buffer) -1);
        if (bytes_received == -1) {
            perror("recv");
            exit(EXIT_FAILURE);
        }
        buffer[bytes_received] = '\0';
        printf("Received: %s\n", send_buffer.buffer);
        printf("Received buffer size: %d\n", bytes_received);
        printf("buffer size: %ld\n", sizeof(send_buffer.buffer));
    }
    close(sockfd);
    free(req);

    return 0;
}