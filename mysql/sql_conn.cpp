#include "sql_conn.h"

//获得空闲连接
MYSQL *connection_pool::GetConnection()
{
    MYSQL *conn = NULL;
    if(0 == connList.size()){
        return NULL;
    }

    reserve.wait();

    lock.lock();

    conn = connList.front();
    connList.pop_front();

    --m_FreeConn;
    ++m_CurConn;
    
    lock.unlock();

    return conn;
}

//释放当前使用
bool connection_pool::ReleaseConnection(MYSQL *conn)
{
    if(NULL == conn){
        return false;
    }
    lock.lock();

    connList.push_back(conn);
    ++m_FreeConn;
    --m_CurConn;

    lock.unlock();

    reserve.post();
    return true;
}

//当前空闲的连接数
int connection_pool::GetFreeConn()
{
    return this->m_FreeConn;
}

//销毁连接池
void connection_pool::DestroyPool()
{
    lock.lock();
    if(connList.size() > 0){
        for(auto it = connList.begin() ; it != connList.end() ; it++){
            MYSQL *conn = *it;
            mysql_close(conn);
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();
    }
    lock.unlock();
}

connection_pool *connection_pool::GetInstance()
{
    static connection_pool connPool;
    return &connPool;
}

void connection_pool::init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log)
{
    m_url = url;
    m_Port = Port;
    m_User = User;
    m_PassWord = PassWord;
    m_DatabaseName = DataBaseName;
    m_close_log = close_log;

    Logger logger("mysql.log", LogLevel::DEBUG);

    //创建MaxConn个数据库连接放进连接池
    for(int i = 0 ; i < MaxConn ; i++){
        MYSQL *conn = NULL;
        conn = mysql_init(conn);
        if(conn == NULL){
            logger.log(LogLevel::DEBUG, "MYSQL Error");
            exit(1);
        }
        conn = mysql_real_connect(conn, url.c_str() , User.c_str(), PassWord.c_str(), DataBaseName.c_str() , Port, NULL , 0);
        
        if(conn == NULL){
            logger.log(LogLevel::DEBUG, "MYSQL Error");
            exit(1);
        }
        connList.push_back(conn);
        ++m_FreeConn;
    }

    reserve = sem(m_FreeConn);
    m_MaxConn = m_FreeConn;
}

connection_pool::connection_pool(){
    //初始化
    m_CurConn = 0;
    m_FreeConn = 0;
}

connection_pool::~connection_pool()
{

}


connectionRAII::connectionRAII(MYSQL **con, connection_pool *connPool)
{
    *con = connPool->GetConnection();
    
    conRAII = *con;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII()
{
    poolRAII->ReleaseConnection(conRAII);
}
