#include "pthread_socket.h"
#include "../inc/envDataNode.h"
#include "../inc/pthread_sql.h"
#define MAX_CLIENT              200
#define SERVER_IP               "0.0.0.0"//"192.168.56.128"
#define SERVER_PORT             8900
static fd_set readfds;
static fd_set tempfds;
static client_info_t client_info[MAX_CLIENT];
threadpool_t *pool;
/**
 * @brief 初始化并创建服务器套接字
 * @return 成功返回套接字描述符，失败返回-1
 */
static int socket_init(void)
{   
    pool = threadpool_create(10, 10);
    int sockfd;
    struct sockaddr_in server_addr;
    // 创建套接字
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        MY_LOG_ERROR("socket create failed\n");
        return -1;
    }
    // 设置服务器地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(8900);
    // 绑定套接字到服务器地址
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        MY_LOG_ERROR("socket bind failed");
        return -1;
    }
    // 监听连接
    if (listen(sockfd, 128) == -1) {
        perror("listen");
        MY_LOG_ERROR("socket listen failed");
        return -1;
    }
    return sockfd;
}
/**
 * @brief 初始化客户端信息数组
 * 
 * 遍历整个client_info数组，将每个元素的tid和client_fd初始化为-1，
 * 并清空buffer缓冲区内容，为后续客户端连接做准备
 */
void client_info_init(void)
{
    for (int i = 0; i < MAX_CLIENT; i++){
        client_info[i].tid = -1;
        client_info[i].client_fd = -1;
        client_info[i].in_use = false;
    }
    //为req初始化内存池
    for (int i = 0; i < MAX_CLIENT; i++){
        client_info[i].req = (ReqPacket *)malloc(sizeof(ReqPacket));
        memset(client_info[i].req, 0, sizeof(ReqPacket));
    }
}
/**
 * @brief 查找未使用的客户端信息槽位
 * 
 * 遍历client_info数组，查找第一个tid为-1(未使用)的槽位
 * @return 成功返回可用槽位索引，失败返回-1(表示数组已满)
 */
static client_info_t *client_info_get(void)
{
    for(int i = 0; i < MAX_CLIENT; i++){
        if(client_info[i].in_use == false) {
            client_info[i].in_use = true;
            return &client_info[i];
        }
    }
    MY_LOG_ERROR("client_info full");
    return NULL;
}
static void client_info_put(client_info_t *client)
{
    client->tid = -1;
    client->client_fd = -1;
    client->in_use = false;
    memset(client->req, 0, sizeof(ReqPacket));
}
/**
 * @brief 销毁客户端信息资源
 * 
 * 该函数用于清理客户端信息数组中占用的资源。
 * 遍历客户端信息数组，取消仍在运行的线程，并释放为请求数据包分配的内存。
 * 此函数通常在程序退出或需要释放相关资源时调用。
 */
void client_info_distroy(void)
{
    for (int i = 0; i < MAX_CLIENT; i++){
        if(client_info[i].in_use == true){
            pthread_cancel(client_info[i].tid);
        }
        if(client_info[i].req != NULL){
            free(client_info[i].req);
        }
    }
}
/**
 * @brief 创建一个包含默认环境数据的 JSON 根对象
 * 
 * 此函数用于创建一个 JSON 对象，该对象包含设备 ID、温度、湿度、气体浓度、水位等环境数据，
 * 以及位置和陀螺仪数据。所有数据的初始值都设置为 0。
 * 
 * @return cJSON* 指向新创建的 JSON 根对象的指针。如果创建失败，可能返回 NULL。
 */
static cJSON *create_JSON_Root(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "deviceID", cJSON_CreateNumber(0));
    cJSON_AddItemToObject(root, "temp", cJSON_CreateNumber(0));
    cJSON_AddItemToObject(root, "humi", cJSON_CreateNumber(0));
    cJSON_AddItemToObject(root, "gas", cJSON_CreateNumber(0));
    cJSON_AddItemToObject(root, "waterlevel", cJSON_CreateNumber(0));
    cJSON *location = cJSON_AddObjectToObject(root, "location");
    cJSON_AddItemToObject(location, "longitude", cJSON_CreateNumber(0));
    cJSON_AddItemToObject(location, "latitude", cJSON_CreateNumber(0));
    cJSON *gyro = cJSON_AddObjectToObject(root, "gyro");
    cJSON_AddItemToObject(gyro, "X", cJSON_CreateNumber(0));
    cJSON_AddItemToObject(gyro, "Y", cJSON_CreateNumber(0));
    return root;
}
/**
 * @brief 将环境数据结构体封装成 JSON 格式的字符串
 * 
 * 该函数接收一个指向 envData_t 结构体的指针和一个字符数组指针，
 * 会将 envData_t 结构体中的数据封装成 JSON 格式，并将结果存储在 buffer 中。
 * 
 * @param envData 指向 envData_t 结构体的指针，包含要封装的环境数据
 * @param buffer 用于存储生成的 JSON 字符串的字符数组指针
 */
static void package_One_JSON(const ReqPacket *pak, char *sendbuffer)
{   
    cJSON *root = create_JSON_Root();
    envData_t tempenvdata;
    find_latest_from_SqliteDB(DBFILENAME, pak->deviceID, &tempenvdata);
    cJSON_SetNumberValue(cJSON_GetObjectItem(root, "deviceID"), tempenvdata.deviceID);
    cJSON_SetNumberValue(cJSON_GetObjectItem(root, "temp"), tempenvdata.temprature);
    cJSON_SetNumberValue(cJSON_GetObjectItem(root, "humi"), tempenvdata.humidity);
    cJSON_SetNumberValue(cJSON_GetObjectItem(root, "gas"), tempenvdata.gas);
    cJSON_SetNumberValue(cJSON_GetObjectItem(root, "waterlevel"), tempenvdata.waterLevel);
    cJSON *location = cJSON_GetObjectItem(root, "location");
    cJSON_SetNumberValue(cJSON_GetObjectItem(location, "longitude"), tempenvdata.longitude);
    cJSON_SetNumberValue(cJSON_GetObjectItem(location, "latitude"), tempenvdata.latitude);
    cJSON *gyro = cJSON_GetObjectItem(root, "gyro");
    cJSON_SetNumberValue(cJSON_GetObjectItem(gyro, "X"), tempenvdata.pitch);
    cJSON_SetNumberValue(cJSON_GetObjectItem(gyro, "Y"), tempenvdata.roll);
    char *str = cJSON_Print(root);
    ssize_t len = strlen(str);
    strcpy(sendbuffer, str);
    cJSON_free(str);
    cJSON_Delete(root);
#ifdef DEBUG
    printf("封装后的JSON数据为buffer: %s\n",sendbuffer);
    printf("封装后的JSON数据为buffer长度: %d\n",len);
#endif // DEBUG
    
}

static void package_20_JSON(const ReqPacket *pak, char *sendbuffer)
{   
    pthread_t tid;
    envData_t *data;
    int id = pak->deviceID;
    pthread_create(&tid, NULL, pthread_Sqlite_Read, &id);
    pthread_join(tid, (void *)&data);
    cJSON *arr_root = cJSON_CreateArray();
    for(int i = 0; i < MAX_SQL_READ; i++){
        cJSON *arr_item = create_JSON_Root();
        cJSON_SetNumberValue(cJSON_GetObjectItem(arr_item, "deviceID"), ((envData_t)data[i]).deviceID);
        cJSON_SetNumberValue(cJSON_GetObjectItem(arr_item, "temp"), ((envData_t)data[i]).temprature);
        cJSON_SetNumberValue(cJSON_GetObjectItem(arr_item, "humi"), ((envData_t)data[i]).humidity);
        cJSON_SetNumberValue(cJSON_GetObjectItem(arr_item, "gas"), ((envData_t)data[i]).gas);
        cJSON_SetNumberValue(cJSON_GetObjectItem(arr_item, "waterlevel"), ((envData_t)data[i]).waterLevel);
        cJSON *location = cJSON_GetObjectItem(arr_item, "location");
        if(location == NULL){
            MY_LOG_ERROR("location is NULL");
        }
        cJSON_SetNumberValue(cJSON_GetObjectItem(location, "longitude"), ((envData_t)data[i]).longitude);
        cJSON_SetNumberValue(cJSON_GetObjectItem(location, "latitude"), ((envData_t)data[i]).latitude);
        cJSON *gyro = cJSON_GetObjectItem(arr_item, "gyro");
        if(gyro == NULL){
            MY_LOG_ERROR("gyro is NULL");
        }
        cJSON_SetNumberValue(cJSON_GetObjectItem(gyro, "X"), ((envData_t)data[i]).pitch);
        cJSON_SetNumberValue(cJSON_GetObjectItem(gyro, "Y"), ((envData_t)data[i]).roll);
        if(!cJSON_AddItemToArray(arr_root, arr_item)){
            MY_LOG_ERROR("cJSON_AddItemToArray failed");
        }
        
    }
    printf("封装后的数组的大小是：%d\n",cJSON_GetArraySize(arr_root));
    char *str = cJSON_Print(arr_root);
    ssize_t len = strlen(str);
    printf("封装后的JSON数据为buffer长度: %d\n",len);
    strcpy(sendbuffer, str);
    cJSON_free(str);
    cJSON_Delete(arr_root);
    free(data);
}
static void parse_revData(const ReqPacket *pak, char *sendbuffer)
{   
    
    switch (pak->cmd)
    {
    case REQUEST_ONE_SQL:
        package_One_JSON(pak, sendbuffer);
        break;
    case REQUEST_20_SQL:
        printf("进入REQUEST_20_SQL\n");
        package_20_JSON(pak, sendbuffer);
        break;
    default:
        break;
    }

}
/**
 * @brief 客户端数据接收线程函数
 * 
 * 处理客户端发送的数据，并在处理完成后清理相关资源
 * 
 * @param arg 传入的客户端信息结构体指针
 * @return void* 总是返回NULL
 */
static void* pthread_recvData(void *arg)
{   
    char buffer[4096 * 2];
    memset(buffer, 0, sizeof(buffer));
#ifdef DEBUG
    printf("进入pthread_recvData线程\n");
#endif // DEBUG
    pthread_rwlock_rdlock(&rwlock);
    parse_revData(((client_info_t *)arg)->req, buffer);//处理数据
    MY_LOG_TRACE("The client's(%d) request is %d", ((client_info_t *)arg)->client_fd, ((client_info_t *)arg)->req->cmd);
    if(((client_info_t *)arg)->client_fd > 0){
        if(send(((client_info_t *)arg)->client_fd,buffer,strlen(buffer),0) <= 0){
            perror("send");
            close(((client_info_t *)arg)->client_fd);
            FD_CLR(((client_info_t *)arg)->client_fd, &readfds);
            MY_LOG_ERROR("send to client(%d) failed", ((client_info_t *)arg)->client_fd);
        }
    }
    pthread_rwlock_unlock(&rwlock);
    client_info_put((client_info_t *)arg);
    return NULL;
}
// const char * sendbuffer = "hello";
// static int cut = 0;
// void *test_thread(void *arg)
// {
//     client_info_t *client = (client_info_t *)arg;
//     write(client->client_fd, sendbuffer, 5);
//     printf("进入到了test_thread\n");
//     MY_LOG_TRACE("第%d次进入", cut++);
//     client_info_put(client);
// }
/**
 * @brief 套接字监听线程主函数
 * 
 * 负责初始化服务器套接字，监听客户端连接，处理客户端数据通信
 * 
 * @param arg 线程参数(未使用)
 * @return void* 线程退出时返回NULL
 */
void pthread_socket(void)
{
    int serv_sockfd = socket_init();
    FD_ZERO(&readfds);
    FD_SET(serv_sockfd, &readfds);
    int maxfd = serv_sockfd;
    int sret = 0;
    while (1) {
        tempfds = readfds;
        sret = select(maxfd + 1, &tempfds, NULL, NULL, NULL);
        if (sret == -1) {
            perror("select");
            MY_LOG_ERROR("select failed\n");
            return;
        }
        if(FD_ISSET(serv_sockfd, &tempfds)){//有新连接到
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            int client_sockfd = accept(serv_sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
            MY_LOG_TRACE("有新连接到 client_sockfd : %d client_addr : %s client_port : %d", 
                        client_sockfd,
                        inet_ntoa(client_addr.sin_addr),
                        ntohs(client_addr.sin_port));//日志打印
            FD_SET(client_sockfd,&readfds);
            maxfd =  client_sockfd > maxfd ? client_sockfd : maxfd;
            if(sret == 1) continue;
        }
        //处理已连接的客户端
#ifdef DEBUG
        printf("maxfd: %d\n", maxfd);
        printf("serv_sockfd: %d\n", serv_sockfd);
        printf("sret: %d\n",sret);
#endif
        for(int i = 0; i <= maxfd; i++){
            if( i != serv_sockfd && FD_ISSET(i,&tempfds)){
                client_info_t *tmp_info = client_info_get();
#ifdef DEBUG
                printf("进入到了client_info_get\n");
#endif
                if(tmp_info == NULL){
                    char buffer[100];
                    recv(i,buffer, sizeof(buffer), 0);
                    MY_LOG_ERROR("client_info full");
                    continue;
                }
                int bytes_read = recv(i, tmp_info->req, sizeof(ReqPacket), 0);
                if (bytes_read <= 0) {//断开连接处理
                    MY_LOG_TRACE("client disconnected: %d", i);//日志打印
                    client_info_put(tmp_info);
                    close(i);
                    FD_CLR(i, &readfds);
                    continue;
                }else if(bytes_read > 0){
                    MY_LOG_TRACE("recv from client(%d) : %d", i, bytes_read);
                    tmp_info->client_fd = i;
                    MY_LOG_TRACE("Adding task to thread pool, current queue size: %d", threadpool_queue_size(pool));
                    if(!threadpool_add(pool, pthread_recvData, (void *)tmp_info)){
                        MY_LOG_ERROR("threadpool_add_task failed");
                        client_info_put(tmp_info);
                        close(i);
                        FD_CLR(i, &readfds);
                    }else{
                        MY_LOG_TRACE("threadpool_add_task success");
                    }

                }
            }
        }
    }
}