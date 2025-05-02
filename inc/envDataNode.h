#ifndef ENVDATA_H
#define ENVDATA_H

#include "../inc/main.h"
#include <stdbool.h>
#define NUMFILENAME "./exist device num.txt"

typedef int deviceID_t;

typedef struct __envData_t{
    deviceID_t deviceID;  // 设备唯一标识符
    float temprature;     // 温度值
    float humidity;       // 湿度值
    float gas;            // 气体浓度值
    float longitude ;     // 经度，表示设备的地理位置
    float latitude ;      // 纬度，表示设备的地理位置
    float pitch;          // 俯仰角，表示设备的倾斜角度
    float roll;           // 横滚角，表示设备的旋转角度
    int waterLevel;       // 水位高度，表示设备的水位状态
    int warningtype;      // 警告类型，表示设备的警告状态
}envData_t;
struct __envDataNode_t{
    envData_t *envData;
    int capacity;
    int rear, front;
};
// 前向声明
typedef struct __envDataNode_t envDataNode_t;
// typedef struct __envData_t envData_t;

void envDataNode_init(envDataNode_t *envDataNode);
bool envDataNode_push(envDataNode_t *envDataNode, envData_t envData);
bool envDataNode_isEmpty(envDataNode_t *envDataNode);
bool envDataNode_isFull(envDataNode_t *envDataNode);
envData_t envDataNode_pop(envDataNode_t *envDataNode);
void envDataNode_destroy(envDataNode_t* envDataNode);

void create_deviceNumFile(void);
void write_deviceNumFile(int *num);
int read_deviceNumFile(void);
bool is_deviceID_in_queue(const int deviceID);
#endif