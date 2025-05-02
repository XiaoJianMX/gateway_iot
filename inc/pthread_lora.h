#ifndef PTHREAD_LORA_H
#define PTHREAD_LORA_H


#include "../inc/cJSON.h"
#include "../inc/main.h"
#include "../inc/envDataNode.h"

extern envDataNode_t envDataNode;

void *pthread_Lora_Recive(void *arg);

#endif // !1