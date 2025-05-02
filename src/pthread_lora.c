#include "../inc/pthread_lora.h"

#define LORA_UART_DEV           "/dev/ttymxc2"
#define CRTSCTS                 020000000000

/* 初始化串口读取操作， 返回串口设备操作文件描述符 */
static int lora_init(void)
{
    // 打开串口
    int fd = open(LORA_UART_DEV, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0)
    {
        perror("open");
        MY_LOG_ERROR("uart open %s failed", LORA_UART_DEV);
        return -1;
    }
    struct termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~CRTSCTS;
    options.c_cflag |= CREAD | CLOCAL;
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;
    // 配置 Termios 为阻塞模式
    options.c_cc[VMIN] = 1;  // 至少读取 1 字节
    options.c_cc[VTIME] = 0; // 无限等
    if (tcsetattr(fd, TCSANOW, &options) != 0)
    {   
        MY_LOG_ERROR("uart setattr failed");
        perror("tcsetattr");
        close(fd);
        return -1;
    }
    tcflush(fd, TCIFLUSH);
    return fd;
}

/* 解析JSON字符串。将数据传递出去 */
static void JSON_Parser(const char *jsonData, envData_t *envData)
{
    cJSON *json = cJSON_Parse(jsonData);
    if (json == NULL)
    {   
        MY_LOG_ERROR("lora cJSON_Parse failed");
        printf("Error before: [%s]\n", cJSON_GetErrorPtr());
        return;
    }
#ifdef DEBUG
    printf("进入JSON_Parser\n");
#endif
    if (json->type == cJSON_Object)
    {
        cJSON *deviceID = cJSON_GetObjectItem(json, "deviceID");
        if (deviceID->type == cJSON_Number)
        {
#ifdef DEBUG
            printf("进入deviceID\n");
#endif
            envData->deviceID = deviceID->valueint;
        }
        cJSON *temprature = cJSON_GetObjectItem(json, "temp");
        if (temprature->type == cJSON_Number)
        {
#ifdef DEBUG
            printf("进入temprature\n");
#endif
            envData->temprature = temprature->valuedouble;
        }
        cJSON *humidity = cJSON_GetObjectItem(json, "humi");
        if (humidity->type == cJSON_Number)
        {
#ifdef DEBUG
            printf("进入humidity\n");
#endif
            envData->humidity = humidity->valuedouble;
        }
        cJSON *gas = cJSON_GetObjectItem(json, "gas");
        if (gas->type == cJSON_Number)
        {
#ifdef DEBUG
            printf("进入gas\n");
#endif
            envData->gas = gas->valuedouble;
        }
        cJSON *waterLevel = cJSON_GetObjectItem(json, "waterlevel");
        if (waterLevel->type == cJSON_Number){
            envData->waterLevel = waterLevel->valueint;
        }
        cJSON *location = cJSON_GetObjectItem(json, "location");
        if (location->type == cJSON_Object)
        {
#ifdef DEBUG
            printf("进入location\n");
#endif
            cJSON *longitude = cJSON_GetObjectItem(location, "longitude");
            if (longitude->type == cJSON_Number)
            {
#ifdef DEBUG
                printf("进入longitude\n");
#endif
                envData->longitude = longitude->valuedouble;
            }
            cJSON *latitude = cJSON_GetObjectItem(location, "latitude");
            if (latitude->type == cJSON_Number)
            {
#ifdef DEBUG
                printf("进入latitude\n");
#endif
                envData->latitude = latitude->valuedouble;
            }
        }
        cJSON *gyro = cJSON_GetObjectItem(json, "gyro");
        if (gyro->type == cJSON_Object)
        {
#ifdef DEBUG
            printf("进入gyro\n");
#endif
            cJSON *pitch = cJSON_GetObjectItem(gyro, "X");
            if (pitch->type == cJSON_Number)
            {
#ifdef DEBUG
                printf("进入pitch\n");
#endif
                envData->pitch = pitch->valuedouble;
            }
            cJSON *roll = cJSON_GetObjectItem(gyro, "Y");
            if (roll->type == cJSON_Number)
            {
#ifdef DEBUG
                printf("进入roll\n");
#endif
                envData->roll = roll->valuedouble;
            }
        }
        cJSON *warningtype = cJSON_GetObjectItem(json, "warningtype");
        if (warningtype->type == cJSON_Number){
            envData->warningtype = warningtype->valueint;
        }
    }
#ifdef DEBUG
        printf("deviceID: %d\n", envData->deviceID);
        printf("temprature: %.2f\n", envData->temprature);
        printf("humidity: %.2f\n", envData->humidity);
        printf("gas: %.2f\n", envData->gas);
        printf("longitude: %.6f\n", envData->longitude);
        printf("latitude: %.6f\n", envData->latitude);
        printf("pitch: %.2f\n", envData->pitch);
        printf("roll: %.2f\n", envData->roll);
#endif
    cJSON_Delete(json);
}

void *pthread_Lora_Recive(void *arg)
{
    int fd = lora_init();
    int flags = fcntl(fd, F_GETFL, 0);
    flags &= ~O_NONBLOCK;//阻塞模式
    fcntl(fd, F_SETFL, flags);
    char buffer[200];
    envData_t tempEnvData;
    while (1)
    {
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);

        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0'; // 添加字符串结束符
#ifdef DEBUG
            printf("收到 %zd 字节: %s\n", bytes_read, buffer);
#endif
            JSON_Parser(buffer, &tempEnvData);
            pthread_mutex_lock(&revdata_mutex);
            if(envDataNode_push(&envDataNode, tempEnvData) && envDataNode_isFull(&envDataNode)){
                pthread_cond_signal(&sql_cont);
#ifdef DEBUG
            printf("数据写入队列成功\n");
#endif
            }
            pthread_mutex_unlock(&revdata_mutex);
        }
    }
    close(fd);
    return NULL;
}