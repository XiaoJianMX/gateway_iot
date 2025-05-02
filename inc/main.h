#ifndef MAIN_H
#define MAIN_H
#define _XOPEN_SOURCE 500         
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include "../inc/cJSON.h"
#include <semaphore.h>
#include "../inc/logger.h"
#include "../inc/thread_pool.h"
#include <mcheck.h>

#define DBFILENAME "./saveData.db"


extern uint8_t *queueOfDevice;
extern pthread_cond_t sql_cont;
extern pthread_mutex_t revdata_mutex;
extern pthread_rwlock_t rwlock;
#endif