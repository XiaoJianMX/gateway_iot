#ifndef PTHREAD_SQL_H
#define PTHREAD_SQL_H
#include "../sqlite3/include/sqlite3.h"
#include <stdbool.h>
#include "../inc/envDataNode.h"
#define MAX_DEVICES              100
#define MAX_SQL_READ             20
extern uint8_t *queueOfDevice;
extern envDataNode_t envDataNode;

void *pthread_Sqlite_Write(void *arg);
void *pthread_Sqlite_Read(void *arg);

void createOrOpen_SqliteDB(const char *dbfilename, const char *tableName);
void insert_data_to_SqliteDB(const char *dbfilename, envData_t *envData);
void find_latest_from_SqliteDB(const char *dbfilename, const int deviceID, envData_t *envData);
void add_newDevice_to_SqliteDB(const char *dbfilename, const int deviceID);

void sql_error_handler(int rc, char* errMsg);
void sql_create_error_handler(int rc, char* errMsg, sqlite3* db);

#endif