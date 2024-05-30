#include "http_conn.h"

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

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

//将事件重置为EPOLLONESHOT 就绪一次后就被移出了需要手动再次添加
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



http_conn::HTTP_CODE http_conn::process_read()
{
    //初始化从状态机状态、HTTP请求解析结果
    LINE_STATUS line_status = LINE_OK;
    http_conn::HTTP_CODE ret = NO_REQUEST;
    char* text = 0;
    
    //逻辑应该是从状态读取一行数据后给出LINE_OK状态
    //根据当前的主状态机的状态纷纷对这一行数据进行不同的操作

    //parse_line为从状态机的具体实现
    while((m_check_state==CHECK_STATE_CONTENT && line_status==LINE_OK)||((line_status=parse_line())==LINE_OK))
    {
        text = get_line();
        switch (m_check_state)
        {
        //解析请求行
        case CHECK_STATE_REQUESTLINE:
            ret = parse_request_line(text);
            if(ret = BAD_REQUEST)
                return BAD_REQUEST;
            break;
        //解析请求头
        case CHECK_STATE_HEADER:
            ret = parse_headers(text);
            if(ret = BAD_REQUEST)
            return BAD_REQUEST;
            //完整解析GET请求后,因为没有消息体的内容，直接跳转到报文响应函数
            else if(ret == GET_REQUEST){
            return do_request();
            }
            break;
        //解析消息体
        case CHECK_STATE_CONTENT:
            ret = parse_content(text);
            if(ret == GET_REQUEST){
            return do_request();
            }

            //解析完消息体即完成报文解析，避免再次进入循环，更新line_status
            line_status=LINE_OPEN;
            break;    
        default:
        //默认错误
            return INTERNAL_ERROR;
        }
    }
    return ret;
}

bool http_conn::process_write(HTTP_CODE ret)
{

    return false;
}


// GET / HTTP/1.1
// Host: 192.168.61.128:8989
// Connection: keep-alive
// Upgrade-Insecure-Requests: 1
// User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36 Edg/125.0.0.0
// Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7
// Accept-Encoding: gzip, deflate
// Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6

http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    //该函数返回 str1 中第一个匹配字符串 str2 中字符的字符数，如果未找到字符则返回 NULL
    m_url = strpbrk(text," \t");

    if(!m_url){
        return BAD_REQUEST;
    }

    *m_url++='\0';


    char *method = text;
    if (strcasecmp(method,"GET") == 0)
    {
        m_method = GET;
    }
    else if(strcasecmp(method,"POST")==0)
    {
        m_method = POST;
        cgi = 1 ;
    }else{
        return BAD_REQUEST;
    }

    //继续跳过空格和\t字符，指向请求资源的第一个字符
    m_url+=strspn(m_url," \t");

    //使用与判断请求方式的相同逻辑，判断HTTP版本号
    m_version = strpbrk(m_url," \t");
    if(!m_version){
        return BAD_REQUEST;
    }
    
    //隔断了m_url
    *m_version++ = '\0';
    m_version += strspn(m_version," \t");
    
    //仅支持HTTP/1.1
    if(strcasecmp(m_version,"HTTP/1.1")!=0){
        return BAD_REQUEST;
    }

    //对请求资源前7个字符进行判断
    //这里主要是有些报文的请求资源中会带有http://，这里需要对这种情况进行单独处理

    if(strncasecmp(m_url , "http://" , 7) == 0){
        m_url += 7;
        //转到资源的位置
        m_url = strchr(m_url, '/');
    }

    if(strncasecmp(m_url , "https://" , 8) == 0){
        m_url += 8;
        //转到资源的位置
        m_url = strchr(m_url, '/');
    }

    if(!m_url || m_url[0]!='/'){
        return BAD_REQUEST;
    }

    //当url为/时，显示欢迎界面
    if(strlen(m_url)==1)
        strcat(m_url,"welcome.html");

    //请求行处理完毕，将主状态机转移处理请求头
    m_check_state = CHECK_STATE_HEADER;

    // NO_REQUEST
    // 请求不完整，需要继续读取请求报文数据
    // GET_REQUEST
    // 获得了完整的HTTP请求

    return NO_REQUEST;
}
