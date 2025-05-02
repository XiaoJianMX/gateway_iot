#include "../inc/envDataNode.h"

#define QUEUE_SIZE          20

/*
 * 函数：envDataNode_init
 * 功能：初始化环境数据节点队列
 * 参数：
 *   - envDataNode: 指向要初始化的队列结构体指针
 * 说明：
 *   - 为队列分配QUEUE_SIZE大小的存储空间
 *   - 设置队列容量为QUEUE_SIZE
 *   - 初始化队首和队尾指针为0
 */
void envDataNode_init(envDataNode_t *envDataNode)
{
    envDataNode->envData = malloc(sizeof(envData_t) * QUEUE_SIZE);
    if(envDataNode->envData == NULL){
        printf("envData malloc error\n");
        return;
    }
    envDataNode->capacity = QUEUE_SIZE;
    envDataNode->rear = 0;
    envDataNode->front = 0;
}
/*
 * 函数：envDataNode_push
 * 功能：向环境数据队列尾部添加新数据
 * 参数：
 *   - envDataNode: 队列结构体指针
 *   - envData: 要添加的环境数据指针
 * 返回值：
 *   - true: 数据添加成功
 *   - false: 队列已满，添加失败
 * 说明：
 *   - 使用循环队列实现，避免空间浪费
 */
bool envDataNode_push(envDataNode_t *envDataNode, envData_t envData)
{
    if ((envDataNode->rear + 1) % envDataNode->capacity == envDataNode->front) return false;
    envDataNode->rear = (envDataNode->rear + 1) % envDataNode->capacity;
    envDataNode->envData[envDataNode->rear] = envData;
    return true;
}
/*
 * 函数：envDataNode_isEmpty
 * 功能：检查环境数据队列是否为空
 * 参数：
 *   - envDataNode: 队列结构体指针
 * 返回值：
 *   - true: 队列为空
 *   - false: 队列不为空
 * 说明：
 *   - 通过比较队首和队尾指针判断队列是否为空
 *   - 循环队列中front等于rear时表示队列为空
 */
bool envDataNode_isEmpty(envDataNode_t *envDataNode)
{
    return envDataNode->front == envDataNode->rear;
}
/*
 * 函数：envDataNode_isFull
 * 功能：检查环境数据队列是否已满
 * 参数：
 *   - envDataNode: 队列结构体指针
 * 返回值：
 *   - true: 队列已满
 *   - false: 队列未满
 * 说明：
 *   - 使用循环队列实现
 *   - 当(rear+1)%capacity == front时表示队列已满
 */
bool envDataNode_isFull(envDataNode_t *envDataNode)
{
    return (envDataNode->rear + 1) % envDataNode->capacity == envDataNode->front;
}
/*
 * 函数：envDataNode_pop  
 * 功能：从环境数据队列头部取出数据
 * 参数：
 *   - envDataNode: 队列结构体指针
 * 返回值：
 *   - 返回取出的环境数据
 * 说明：
 *   - 使用循环队列实现
 *   - 先移动队首指针再取出数据
 *   - 调用者需确保队列不为空
 */
envData_t envDataNode_pop(envDataNode_t *envDataNode)
{
    envDataNode->front = (envDataNode->front + 1) % envDataNode->capacity;
    return envDataNode->envData[envDataNode->front];
}
/*
 * 函数：envDataNode_destroy
 * 功能：销毁环境数据队列并释放内存
 * 参数：
 *   - envDataNode: 指向要销毁的队列结构体指针
 * 说明：
 *   - 安全释放队列数据存储空间
 *   - 将数据指针置为NULL避免野指针
 *   - 不释放队列结构体本身，仅释放内部数据
 */
void envDataNode_destroy(envDataNode_t* envDataNode)
{
    if(envDataNode->envData != NULL){
        free(envDataNode->envData);
        envDataNode->envData = NULL;
    }
}





/*
 * 函数：create_deviceNumFile
 * 功能：创建设备数量记录文件（如果文件不存在）
 * 说明：
 *   - 使用access()检查文件是否存在
 *   - 文件不存在时创建新文件并设置读写权限(0666)
 *   - 文件创建失败时打印错误信息
 *   - 文件已存在时直接返回
 */
void create_deviceNumFile(void)
{
    // 检查文件是否存在
    if (access(NUMFILENAME, F_OK) == -1) {
        // 创建文件并设置读写权限
        int fd = open(NUMFILENAME, O_CREAT | O_RDWR, 0666);
        // 检查文件是否成功创建
        if (fd == -1) {
            perror("open");  // 打印错误信息
            return;
        }
        close(fd);  // 关闭文件描述符
    }else{
        return;  // 文件已存在，直接返回
    }
}

/*
 * 函数：write_deviceNumFile
 * 功能：将设备数量写入文件
 * 参数：
 *   - num: 指向要写入的设备数量值的指针
 * 说明：
 *   - 以读写模式打开文件，并清空文件内容
 *   - 如果文件打开失败，打印错误信息并返回
 *   - 将设备数量值写入文件
 *   - 关闭文件描述符
 */
void write_deviceNumFile(int *num)
{   
    int fd = open(NUMFILENAME, O_RDWR | O_TRUNC, 0666);
    if (fd == -1) {
        perror("open");
        return;
    }
    char buf[2] = {0};
    sprintf(buf, "%d", *num);
    write(fd, buf, sizeof(buf));
    close(fd);
}

/*
 * 函数：read_deviceNumFile
 * 功能：从文件中读取设备数量
 * 返回值：
 *   - 成功：返回读取到的设备数量
 *   - 失败：返回-1
 * 说明：
 *   - 以只读模式打开文件
 *   - 如果文件打开失败，打印错误信息并返回-1
 *   - 从文件中读取设备数量值
 *   - 关闭文件描述符
 *   - 返回读取到的设备数量
 */
int read_deviceNumFile(void)
{
    int fd = open(NUMFILENAME, O_RDONLY, 0666);
    if (fd == -1) {
        perror("open");
        return 0;
    }
    char buf[10];
    read(fd, buf,2);
    close(fd);
    return atoi(buf);
}
/*
 * 函数：is_deviceID_in_queue
 * 功能：检查指定设备ID是否存在于设备队列中
 * 参数：
 *   - deviceID: 要检查的设备ID
 * 返回值：
 *   - true(1): 设备ID存在于队列中
 *   - false(0): 设备ID不存在于队列中
 * 说明：
 *   - 在DEBUG模式下会打印调试信息
 *   - 遍历全局设备ID数组queueOfDevice进行查找
 */
bool is_deviceID_in_queue(const int deviceID)
{
#ifdef DEBUG
    printf("is_deviceID_in_queue--deviceID: %d\n", deviceID);
    printf("is_deviceID_in_queue--queueOfDevice size: %d\n", read_deviceNumFile());
#endif 
    int num = read_deviceNumFile();
#ifdef DEBUG
    printf("is_deviceID_in_queue--file num : %d\n", num);
#endif 
    for(int i = 0; i < num ; i ++){
        if(queueOfDevice[i] == deviceID){
            return 1;
        }
    }
    int size = deviceID + 1;
    queueOfDevice = realloc(queueOfDevice, (size_t)size * sizeof(uint8_t));
    queueOfDevice[size - 1] = deviceID;
    write_deviceNumFile(&size);
    return 0;
}
