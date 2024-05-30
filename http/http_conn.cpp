#include "http_conn.h"



void http_conn::init(int sockfd, const sockaddr_in &addr)
{
    
}

void http_conn::close_conn(bool real_close)
{

}

//从内核时间表删除描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
    //请求不完整
    if(read_ret == NO_REQUEST){
        //注册并监听读事件
        modfd(m_epollfd,m_sockfd,EPOLLIN,1);
        return;
    }

    //调用process_write完成报文响应
    bool write_ret = process_write(read_ret);
    if(!write_ret){
        close_conn();
    }

    //注册并监听写事件
    modfd(m_epollfd,m_sockfd,EPOLLOUT,1);
}

bool http_conn::read_once()
{
    //为什么会出现这种情况呢？
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;
    while (true)
    {
        // recv(sockfd, buffer, length, flags)：从 sockfd 读取数据，存储到 buffer，最多读取 length 个字节。flags 参数通常为 0。
        bytes_read = recv(m_sockfd,m_read_buf+m_read_idx , READ_BUFFER_SIZE-m_read_idx,0);
        // 当 recv 返回 -1 时检查 errno：
        if (bytes_read == -1)
        {
            // EAGAIN 或 EWOULDBLOCK 表示没有数据可读，读取操作会阻塞，这种情况不是错误，只需稍后再尝试读取。
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                //ET模式 需要把数据读完
                break;
            }
            return false;

        }else if (bytes_read == 0) {
            // 对方关闭连接
            
            logger->log(LogLevel::DEBUG, "Debug message");
            return false;
        }
        
        m_read_idx += bytes_read;
    }
    return true;
}

bool http_conn::write()
{

    return false;
}

void http_conn::initmysql_result()
{

}

void http_conn::initresultFile(connection_pool *connPool)
{

}

HTTP_CODE http_conn::process_read()
{
    
    return HTTP_CODE::NO_RESOURCE;
}

bool http_conn::process_write(HTTP_CODE ret)
{

    return false;
}
