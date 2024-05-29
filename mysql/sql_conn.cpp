#include "sql_conn.h"

MYSQL *connection_pool::GetConnection()
{
    return nullptr;
}

bool connection_pool::ReleaseConnection(MYSQL *conn)
{
    return false;
}

int connection_pool::GetFreeConn()
{
    return 0;
}

void connection_pool::DestroyPool()
{
}

void connection_pool::init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log)
{
}

connection_pool::~connection_pool()
{
}

connectionRAII::connectionRAII(MYSQL **con, connection_pool *connPool)
{
}

connectionRAII::~connectionRAII()
{
}
