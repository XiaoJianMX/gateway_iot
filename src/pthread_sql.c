#include "pthread_sql.h"

/*
 * pthread_Sqlite_Write
 * 功能：SQLite数据库操作线程函数
 * 参数：
 *   - arg: 线程参数(未使用)
 * 返回值：
 *   - 线程退出时返回NULL
 * 说明：
 *   - 该线程持续运行，等待条件变量触发后处理数据
 *   - 从共享队列中获取环境数据并存入SQLite数据库
 *   - 使用互斥锁保护共享资源访问
 */
void *pthread_Sqlite_Write(void *arg)
{
    envData_t envdata;
    while (1)
    {
        pthread_mutex_lock(&revdata_mutex);
        pthread_cond_wait(&sql_cont, &revdata_mutex);
        pthread_rwlock_wrlock(&rwlock);
        // 取出数据
        while (!envDataNode_isEmpty(&envDataNode))
        {
            envdata = envDataNode_pop(&envDataNode);
            if (envdata.deviceID != 0)
            {
                insert_data_to_SqliteDB(DBFILENAME, &envdata);
            }
        }
        pthread_rwlock_unlock(&rwlock);
        pthread_mutex_unlock(&revdata_mutex);
    }
}

/* 读取最新数据sqlite3回调函数 */
static int sqlite_read_callback(void *data, int argc, char **argv, char **azColName)
{   
    static uint8_t cnt = 0;
    envData_t *envdataArr = (envData_t *)data;

    for(int i = 0; i < argc; i++){
        if (strcmp(azColName[i], "deviceID") == 0){
            envdataArr[cnt].deviceID = atoi(argv[i]);
        }
        else if (strcmp(azColName[i], "temp") == 0){
            envdataArr[cnt].temprature = atof(argv[i]);
        }
        else if (strcmp(azColName[i], "humi") == 0){
            envdataArr[cnt].humidity = atof(argv[i]);
        }
        else if (strcmp(azColName[i], "gas") == 0){
            envdataArr[cnt].gas = atoi(argv[i]);
        }
        else if (strcmp(azColName[i], "longitude") == 0){
            envdataArr[cnt].longitude = atof(argv[i]);
        }
        else if (strcmp(azColName[i], "latitude") == 0){
            envdataArr[cnt].latitude = atof(argv[i]);
        }
        else if (strcmp(azColName[i], "pitch") == 0){
            envdataArr[cnt].pitch = atof(argv[i]);
        }
        else if (strcmp(azColName[i], "roll") == 0){
            envdataArr[cnt].roll = atof(argv[i]);
        }
        else if (strcmp(azColName[i], "waterlever") == 0){
            envdataArr[cnt].waterLevel = atoi(argv[i]);
        }
        else if (strcmp(azColName[i], "remark") == 0){
            envdataArr[cnt].warningtype = atoi(argv[i]);
        }

    }
    cnt ++;
    if(cnt >= 20) cnt = 0;
    return 0;
}
/*
 * 函数：pthread_Sqlite_Read
 * 功能：读取指定设备ID的最新环境数据
 * 参数：
 *   - arg: 指向设备ID的指针
 * 返回值：
 *   - 返回包含查询结果的envData_t数组指针
 * 说明：
 *   - 查询指定设备ID的最新MAX_SQL_READ条记录
 *   - 返回的数据需要由调用者释放内存
 */
void *pthread_Sqlite_Read(void *arg)
{   
    envData_t *envdata = (envData_t *)malloc(sizeof(envData_t) * MAX_SQL_READ);
    char sql[100];
    int id = *(int *)arg;
    sprintf(sql, "SELECT * FROM envData%d ORDER BY time DESC LIMIT %d;", id, MAX_SQL_READ);
    sqlite3 *db;int rc;
    rc = sqlite3_open(DBFILENAME, &db);
    sql_create_error_handler(rc, NULL, db);
#ifdef DEBUG
    printf("进入pthread_Sqlite_Read\n");
#endif // DEBUG
    sqlite3_exec(db, sql, sqlite_read_callback, envdata, NULL);
    return (void *)envdata;
}

/*
 * 函数：createOrOpen_SqliteDB
 * 功能：创建或打开SQLite数据库，并在其中创建指定名称的表
 * 参数：
 *   - dbfilename: 数据库文件名
 *   - tableName: 要创建的表名
 * 说明：
 *   - 如果数据库文件不存在会自动创建
 *   - 表结构包含设备ID、温湿度、气体浓度等环境数据字段
 */
void createOrOpen_SqliteDB(const char *dbfilename, const char *tableName)
{
    sqlite3 *db;
    char *errMsg = 0;
    int rc;
    // 打开数据库
    rc = sqlite3_open(dbfilename, &db);
    sql_create_error_handler(rc, errMsg, db);
    // 创建表
    char sql[256];
    sprintf(sql, "CREATE TABLE IF NOT EXISTS %s ("
                 "deviceID INTEGER,"
                 "temp REAL,"
                 "humi REAL,"
                 "gas INTEGER,"
                 "longitude REAL,"
                 "latitude REAL,"
                 "pitch REAL,"
                 "roll REAL,"
                 "waterlever INTEGER,"
                 "time TEXT,"
                 "remark INTEGER);",
                  tableName);
#ifdef DEBUG
    printf("%s\n", sql);
#endif
    rc = sqlite3_exec(db, sql, NULL, NULL, &errMsg);
    sql_error_handler(rc, errMsg);
    sqlite3_close(db);
}

/*
 * 函数：insert_data_to_SqliteDB
 * 功能：向SQLite数据库插入环境数据
 * 参数：
 *   - dbFileName: 数据库文件名
 *   - envData: 包含环境数据的结构体指针
 * 说明：
 *   - 根据设备ID自动生成表名(格式: envData+设备ID)
 *   - 自动获取当前系统时间作为时间戳
 *   - 插入数据包括温度、湿度、气体浓度等环境参数
 */
void insert_data_to_SqliteDB(const char *dbFileName, envData_t *envData)
{
    // 检查设备ID是否在队列中
    if (!is_deviceID_in_queue(envData->deviceID))
    {
#ifdef DEBUG
        printf("deviceID %d is not in queue\n", envData->deviceID);
#endif // DEBUG
        add_newDevice_to_SqliteDB(dbFileName, envData->deviceID);
    }
    sqlite3 *db;
    char *errMsg = 0;
    int rc;
    // 获取当前时间
    time_t now;
    time(&now);
    // 打开数据库
    rc = sqlite3_open(dbFileName, &db);
    sql_create_error_handler(rc, errMsg, db);
    // 插入数据
    char sql[256];
    char tableName[20];
    sprintf(tableName, "envData%d", envData->deviceID);
    sprintf(sql, "INSERT INTO %s (deviceID, temp, humi, gas, longitude, latitude, pitch, roll, waterlever, time, remark) "
                 "VALUES (%d, %.2f, %.2f, %.2f ,%.6f, %.6f, %.2f, %.2f, %d, '%s', %d);",
            tableName,
            envData->deviceID,
            envData->temprature,
            envData->humidity,
            envData->gas,
            envData->longitude,
            envData->latitude,
            envData->pitch,
            envData->roll,
            envData->waterLevel,
            ctime(&now),
            envData->warningtype);
    rc = sqlite3_exec(db, sql, NULL, NULL, &errMsg);
    sql_error_handler(rc, errMsg);
    sqlite3_close(db);
}
/*
 * 函数：find_data_callback
 * 功能：SQLite查询结果的回调处理函数
 * 参数：
 *   - data: 用户提供的指针，用于存储查询结果(此处为envData_t结构体指针)
 *   - argc: 返回的字段数量
 *   - argv: 字段值数组(字符串形式)
 *   - azColName: 字段名数组
 * 返回值：
 *   - 始终返回0(SQLITE_OK)
 * 说明：
 *   - 将SQLite返回的字符串格式数据转换为envData_t结构体对应类型
 *   - 自动匹配字段名并填充到结构体对应成员
 */
static int find_data_callback(void *data, int argc, char **argv, char **azColName)
{
    envData_t *envData = (envData_t *)data;
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(azColName[i], "deviceID") == 0)
        {
            envData->deviceID = atoi(argv[i]);
        }
        else if (strcmp(azColName[i], "temp") == 0)
        {
            envData->temprature = atof(argv[i]);
        }
        else if (strcmp(azColName[i], "humi") == 0)
        {
            envData->humidity = atof(argv[i]);
        }
        else if (strcmp(azColName[i], "gas") == 0)
        {
            envData->gas = atoi(argv[i]);
        }
        else if (strcmp(azColName[i], "longitude") == 0)
        {
            envData->longitude = atof(argv[i]);
        }
        else if (strcmp(azColName[i], "latitude") == 0)
        {
            envData->latitude = atof(argv[i]);
        }
        else if (strcmp(azColName[i], "pitch") == 0)
        {
            envData->pitch = atof(argv[i]);
        }
        else if (strcmp(azColName[i], "roll") == 0)
        {
            envData->roll = atof(argv[i]);
        }
        else if (strcmp(azColName[i], "waterlever") == 0)
        {
            envData->waterLevel = atoi(argv[i]);
        }
        else if (strcmp(azColName[i], "remark") == 0)
        {
            envData->warningtype = atoi(argv[i]);
        }
    }
    return 0;
}
/*
 * 函数：find_latest_from_SqliteDB
 * 功能：从SQLite数据库查询指定设备的最新一条环境数据
 * 参数：
 *   - dbfilename: 数据库文件名
 *   - deviceID: 要查询的设备ID
 *   - envData: 用于存储查询结果的环境数据结构体指针
 * 说明：
 *   - 按时间降序排序后获取第一条记录(即最新记录)
 *   - 查询结果通过回调函数填充到envData结构体
 *   - 自动处理数据库连接和错误
 */
void find_latest_from_SqliteDB(const char *dbfilename, const int deviceID, envData_t *envData)
{
    sqlite3 *db;
    char *errMsg = 0;
    int rc;

    rc = sqlite3_open(dbfilename, &db);
    sql_create_error_handler(rc, errMsg, db);

    char sql[256];
    sprintf(sql, "SELECT * FROM envData%d ORDER BY time DESC LIMIT 1;", deviceID);

    rc = sqlite3_exec(db, sql, find_data_callback, envData, &errMsg);
    sql_error_handler(rc, errMsg);

    sqlite3_close(db);
}
/*
 * 函数：add_newDevice_to_SqliteDB
 * 功能：为新增设备创建对应的数据库表
 * 参数：
 *   - dbfilename: 数据库文件名
 *   - deviceID: 要添加的新设备ID
 * 说明：
 *   - 根据设备ID自动生成表名(格式: envData+设备ID)
 *   - 调用createOrOpen_SqliteDB函数创建新表
 *   - 表结构与现有环境数据表结构一致
 */
void add_newDevice_to_SqliteDB(const char *dbfilename, const int deviceID)
{
    char tbname[30];
    sprintf(tbname, "envData%d", deviceID);
    createOrOpen_SqliteDB(dbfilename, tbname);
}

/*
 * 函数：sql_error_handler
 * 功能：处理 SQLite 操作中的错误
 * 参数：
 *   - rc: SQLite 操作的返回码
 *   - errMsg: SQLite 返回的错误信息
 * 说明：
 *   - 如果操作失败（rc != SQLITE_OK），打印错误信息并释放错误消息内存
 *   - 该函数用于统一处理 SQLite 操作中的错误，简化代码结构
 */
void sql_error_handler(int rc, char *errMsg)
{
    if (rc != SQLITE_OK)
    {   
        MY_LOG_ERROR("SQL Failed: %s", errMsg);
        printf("Failed: %s\n", errMsg);
        if(errMsg != NULL)
            sqlite3_free(errMsg);
        return;
    }
}

/*
 * 函数：sql_create_error_handler
 * 功能：处理创建表时的 SQLite 错误
 * 参数：
 *   - rc: SQLite 操作的返回码
 *   - errMsg: SQLite 返回的错误信息
 *   - db: SQLite 数据库句柄
 * 说明：
 *   - 如果操作失败（rc != SQLITE_OK），打印错误信息，释放错误消息内存，并关闭数据库
 *   - 该函数专门用于处理创建表时的错误，确保资源被正确释放
 */
void sql_create_error_handler(int rc, char *errMsg, sqlite3 *db)
{
    if (rc != SQLITE_OK)
    {   
        MY_LOG_ERROR("Failed to create table: %s", errMsg);
        printf("Failed to create table: %s\n", errMsg);
        if(errMsg != NULL)
            sqlite3_free(errMsg);
        sqlite3_close(db);
        return;
    }
}