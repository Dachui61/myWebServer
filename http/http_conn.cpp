#include "http_conn.h"

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form =
    "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form =
    "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form =
    "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form =
    "There was an unusual problem serving the request file.\n";

void http_conn::init(int sockfd, const sockaddr_in &addr) {

}

void http_conn::close_conn(bool real_close) {}

//从内核时间表删除描述符
void removefd(int epollfd, int fd) {
  epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
  close(fd);
}

//将事件重置为EPOLLONESHOT 就绪一次后就被移出了需要手动再次添加
void modfd(int epollfd, int fd, int ev, int TRIGMode) {
  epoll_event event;
  event.data.fd = fd;

  if (1 == TRIGMode)
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
  else
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

  epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void http_conn::process() {
  HTTP_CODE read_ret = process_read();
  //请求不完整
  if (read_ret == NO_REQUEST) {
    //注册并监听读事件
    modfd(m_epollfd, m_sockfd, EPOLLIN, 1);
    return;
  }

  //调用process_write完成报文响应
  bool write_ret = process_write(read_ret);
  if (!write_ret) {
    close_conn();
  }

  //注册并监听写事件
  modfd(m_epollfd, m_sockfd, EPOLLOUT, 1);
}

bool http_conn::read_once() {
  //为什么会出现这种情况呢？
  if (m_read_idx >= READ_BUFFER_SIZE) {
    return false;
  }
  int bytes_read = 0;
  while (true) {
    // recv(sockfd, buffer, length, flags)：从 sockfd 读取数据，存储到
    // buffer，最多读取 length 个字节。flags 参数通常为 0。
    bytes_read = recv(m_sockfd, m_read_buf + m_read_idx,
                      READ_BUFFER_SIZE - m_read_idx, 0);
    // 当 recv 返回 -1 时检查 errno：
    if (bytes_read == -1) {
      // EAGAIN 或 EWOULDBLOCK
      // 表示没有数据可读，读取操作会阻塞，这种情况不是错误，只需稍后再尝试读取。
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // ET模式 需要把数据读完
        break;
      }
      return false;

    } else if (bytes_read == 0) {
      // 对方关闭连接

      logger->log(LogLevel::DEBUG, "Debug message");
      return false;
    }

    m_read_idx += bytes_read;
  }
  return true;
}

bool http_conn::write() {
  int temp = 0;

  int newadd = 0;

  //若要发送的数据长度为0
  //表示响应报文为空，一般不会出现这种情况
  if (bytes_to_send == 0) {
    modfd(m_epollfd, m_sockfd, EPOLLIN, 1);
    init();
    return true;
  }

  while (1) {
    //将响应报文的状态行、消息头、空行和响应正文发送给浏览器端
    temp = writev(m_sockfd, m_iv, m_iv_count);

    //正常发送，temp为发送的字节数
    if (temp > 0) {
      //更新已发送字节
      bytes_have_send += temp;
      //偏移 文件iovec 的指针
      newadd = bytes_have_send - m_write_idx;
    }
    if (temp <= -1) {
      //判断缓冲区是否满了
      if (errno == EAGAIN) {
        //第一个iovec头部信息的数据已发送完，发送第二个iovec数据
        //因为m_iv是顺序发送的
        if (bytes_have_send >= m_iv[0].iov_len) {
          //不再继续发送头部信息
          m_iv[0].iov_len = 0;
          m_iv[1].iov_base = m_file_address + newadd;
          m_iv[1].iov_len = bytes_to_send;
        }
        //继续发送第一个iovec头部信息的数据
        else {
          m_iv[0].iov_base = m_write_buf + bytes_have_send;
          m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }
        //重新注册写事件
        modfd(m_epollfd, m_sockfd, EPOLLOUT, 1);
        return true;
      }
      //如果发送失败，但不是缓冲区问题，取消映射
      unmap();
      return false;
    }

    bytes_to_send -= temp;
    if (bytes_to_send <= 0) {
      //发送完了断开映射
      unmap();

      if (m_linger) {
        //检查是否为长连接 若是长连接则保持
        //重新初始化对象
        init();
        return true;
      } else {
        //不是长连接则断开
        return false;
      }
    }
  }
}

void http_conn::initmysql_result() {}
//用户名和密码
map<string, string> users;
void http_conn::initresultFile(connection_pool *connPool) {
  MYSQL *mysql = NULL;
  connectionRAII mysqlcon(&mysql, connPool);

  //在user表中查找username和passwd数据
  if (mysql_query(mysql, "SELECT username,passwd FROM user")) {
    string info = "SELECT ERROR:";
    info.append(mysql_error(mysql)).append("\n");
    logger->log(LogLevel::ERROR, info);
    return;
  }

  //从表中检索完整的结果集
  MYSQL_RES *result = mysql_store_result(mysql);

  //返回结果集中的列数
  int num_fields = mysql_num_fields(result);

  //返回所有字段结构的数组
  MYSQL_FIELD *fields = mysql_fetch_fields(result);

  //从结果集中获取下一行，将对应的用户名和密码，存入map中
  while (MYSQL_ROW row = mysql_fetch_row(result)) {
    string temp1(row[0]);
    string temp2(row[1]);
    users[temp1] = temp2;
  }
}

void http_conn::init() {
  mysql = NULL;
  bytes_to_send = 0;
  bytes_have_send = 0;
  m_check_state = CHECK_STATE_REQUESTLINE;
  m_linger = false;
  m_method = GET;
  m_url = 0;
  m_version = 0;
  m_content_length = 0;
  m_host = 0;
  m_start_line = 0;
  m_checked_idx = 0;
  m_read_idx = 0;
  m_write_idx = 0;
  cgi = 0;

  memset(m_read_buf, '\0', READ_BUFFER_SIZE);
  memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
  memset(m_real_file, '\0', FILENAME_LEN);
}

http_conn::HTTP_CODE http_conn::process_read() {
  // 初始化从状态机状态、HTTP请求解析结果
  LINE_STATUS line_status = LINE_OK;
  http_conn::HTTP_CODE ret = NO_REQUEST;
  char *text = 0;

  // 逻辑应该是从状态读取一行数据后给出LINE_OK状态
  // 根据当前的主状态机的状态纷纷对这一行数据进行不同的操作

  // parse_line为从状态机的具体实现
  // 在POST请求报文中，消息体的末尾没有任何字符，所以不能使用从状态机的状态，这里转而使用主状态机的状态作为循环入口条件
  while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) ||
         ((line_status = parse_line()) == LINE_OK)) {
    text = get_line();

    // m_start_line是一个索引，用于记录下次开始的位置
    // 在parse_line()中m_checked_idx为当前解析到的位置，以可以理解为下一行的起始位置
    // 因为parse_line()是解析到一行就停止了
    m_start_line = m_checked_idx;

    switch (m_check_state) {
    // 解析请求行
    case CHECK_STATE_REQUESTLINE:
      ret = parse_request_line(text);
      if (ret = BAD_REQUEST)
        return BAD_REQUEST;
      break;
    // 解析请求头
    case CHECK_STATE_HEADER:
      ret = parse_headers(text);
      if (ret = BAD_REQUEST)
        return BAD_REQUEST;
      // 完整解析GET请求后,因为没有消息体的内容，直接跳转到报文响应函数
      else if (ret == GET_REQUEST) {
        return do_request();
      }
      break;
    // 解析消息体
    case CHECK_STATE_CONTENT:
      ret = parse_content(text);
      if (ret == GET_REQUEST) {
        return do_request();
      }

      // 解析完消息体即完成报文解析，避免再次进入循环，更新line_status
      line_status = LINE_OPEN;
      break;
    default:
      // 默认错误
      return INTERNAL_ERROR;
    }
  }
  return ret;
}

bool http_conn::process_write(HTTP_CODE ret) {

  switch (ret) {
  //内部错误，500
  case INTERNAL_ERROR: {
    //状态行
    add_status_line(500, error_500_title);
    //消息报头
    add_headers(strlen(error_500_form));
    if (!add_content(error_500_form))
      return false;
    break;
  }
  //报文语法有误，404
  case BAD_REQUEST: {
    add_status_line(404, error_404_title);
    add_headers(strlen(error_404_form));
    if (!add_content(error_404_form))
      return false;
    break;
  }
  //资源没有访问权限，403
  case FORBIDDEN_REQUEST: {
    add_status_line(403, error_403_title);
    add_headers(strlen(error_403_form));
    if (!add_content(error_403_form))
      return false;
    break;
  }
  //文件存在，200
  case FILE_REQUEST: {
    add_status_line(200, ok_200_title);
    //如果请求的资源存在
    if (m_file_stat.st_size != 0) {
      add_headers(m_file_stat.st_size);
      //第一个iovec指针指向响应报文缓冲区，长度指向m_write_idx
      m_iv[0].iov_base = m_write_buf;
      m_iv[0].iov_len = m_write_idx;
      //第二个iovec指针指向mmap返回的文件指针，长度指向文件大小
      m_iv[1].iov_base = m_file_address;
      m_iv[1].iov_len = m_file_stat.st_size;
      m_iv_count = 2;
      //发送的全部数据为响应报文头部信息和文件大小
      bytes_to_send = m_write_idx + m_file_stat.st_size;
      return true;
    } else {
      //如果请求的资源大小为0，则返回空白html文件
      const char *ok_string = "<html><body></body></html>";
      add_headers(strlen(ok_string));
      if (!add_content(ok_string))
        return false;
    }
  }
  default:
    return false;
  }
  //除FILE_REQUEST状态外，其余状态只申请一个iovec，指向响应报文缓冲区
  m_iv[0].iov_base = m_write_buf;
  m_iv[0].iov_len = m_write_idx;
  m_iv_count = 1;
  return true;
}

// GET / HTTP/1.1
// Host: 192.168.61.128:8989
// Connection: keep-alive
// Upgrade-Insecure-Requests: 1
// User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36
// (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36 Edg/125.0.0.0 Accept:
// text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7
// Accept-Encoding: gzip, deflate
// Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6

http_conn::HTTP_CODE http_conn::parse_request_line(char *text) {
  //该函数返回 str1 中第一个匹配字符串 str2 中字符的字符数，如果未找到字符则返回
  // NULL
  m_url = strpbrk(text, " \t");

  if (!m_url) {
    return BAD_REQUEST;
  }

  *m_url++ = '\0';

  char *method = text;
  if (strcasecmp(method, "GET") == 0) {
    m_method = GET;
  } else if (strcasecmp(method, "POST") == 0) {
    m_method = POST;
    cgi = 1;
  } else {
    return BAD_REQUEST;
  }

  //继续跳过空格和\t字符，指向请求资源的第一个字符
  m_url += strspn(m_url, " \t");

  //使用与判断请求方式的相同逻辑，判断HTTP版本号
  m_version = strpbrk(m_url, " \t");
  if (!m_version) {
    return BAD_REQUEST;
  }

  //隔断了m_url
  *m_version++ = '\0';
  m_version += strspn(m_version, " \t");

  //仅支持HTTP/1.1
  if (strcasecmp(m_version, "HTTP/1.1") != 0) {
    return BAD_REQUEST;
  }

  //对请求资源前7个字符进行判断
  //这里主要是有些报文的请求资源中会带有http://，这里需要对这种情况进行单独处理

  if (strncasecmp(m_url, "http://", 7) == 0) {
    m_url += 7;
    //转到资源的位置
    m_url = strchr(m_url, '/');
  }

  if (strncasecmp(m_url, "https://", 8) == 0) {
    m_url += 8;
    //转到资源的位置
    m_url = strchr(m_url, '/');
  }

  if (!m_url || m_url[0] != '/') {
    return BAD_REQUEST;
  }

  //当url为/时，显示欢迎界面
  if (strlen(m_url) == 1)
    strcat(m_url, "welcome.html");

  //请求行处理完毕，将主状态机转移处理请求头
  m_check_state = CHECK_STATE_HEADER;

  // NO_REQUEST
  // 请求不完整，需要继续读取请求报文数据
  // GET_REQUEST
  // 获得了完整的HTTP请求

  return NO_REQUEST;
}

//每次也是解析一行，而不是直接吧整个请求头解析完，也符合一行一行读取和解析的逻辑
//从状态机一行一行的读，主状态机再一行一行的解析
//分析connection字段，content-length字段，其他字段可以直接跳过
http_conn::HTTP_CODE http_conn::parse_headers(char *text) {
  //判断是空行还是请求头
  if (text[0] == '\0') {
    //判断是GET还是POST请求
    if (m_content_length != 0) {
      // POST需要跳转到消息体处理状态
      m_check_state = CHECK_STATE_CONTENT;
      return NO_REQUEST;
    }
    return GET_REQUEST;
  }
  //解析请求头部连接字段
  else if (strncasecmp(text, "Connection:", 11) == 0) {
    text += 11;

    //跳过空格和\t字符
    text += strspn(text, " \t");
    if (strcasecmp(text, "keep-alive") == 0) {
      //如果是长连接，则将linger标志设置为true
      m_linger = true;
    }
  }
  //解析请求头部内容长度字段
  else if (strncasecmp(text, "Content-length:", 15) == 0) {
    text += 15;
    text += strspn(text, " \t");
    m_content_length = atol(text);
  }
  //解析请求头部HOST字段
  else if (strncasecmp(text, "Host:", 5) == 0) {
    text += 5;
    text += strspn(text, " \t");
    m_host = text;
  } else {
    //不知道的头信息
    logger->log(LogLevel::DEBUG, "oop!unknow header");
    logger->log(LogLevel::DEBUG, text);
  }
  return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char *text) {
  if (m_read_idx >= (m_content_length + m_checked_idx)) {
    //解析出消息体
    text[m_content_length] = '\0';

    m_string = text;
    return GET_REQUEST;
  }
  return NO_REQUEST;
}

//网站根目录，文件夹内存放请求的资源和跳转的html文件
const char *doc_root = "/www/ly";
http_conn::HTTP_CODE http_conn::do_request() {
  strcpy(m_real_file, doc_root);
  int len = strlen(doc_root);

  //找到m_url中/的位置
  //strrchr从字符串的末尾开始向前搜索，直到找到指定的字符或搜索完整个字符串
  const char *p = strrchr(m_url, '/');

  if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')) {
    //登录或者注册cgi
    //根据标志判断是登录检测还是注册检测
    char flag = m_url[1];

    char *m_url_real = (char *)malloc(sizeof(char) * 200);

    strcpy(m_url_real, "/");
    strcat(m_url_real, m_url + 2);
    strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
    free(m_url_real);

    //将用户名和密码提取出来
    // user=123&password=123
    char name[100], password[100];
    int i;

    //以&为分隔符，前面的为用户名
    for (i = 5; m_string[i] != '&'; ++i)
      name[i - 5] = m_string[i];
    name[i - 5] = '\0';

    //以&为分隔符，后面的是密码
    int j = 0;
    for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
      password[j] = m_string[i];
    password[j] = '\0';

    const char *p = strrchr(m_url, '/');

    if (flag == '3') {
      //如果是注册，先检测数据库中是否有重名的
      //没有重名的，进行增加数据
      char *sql_insert = (char *)malloc(sizeof(char) * 200);
      strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
      strcat(sql_insert, "'");
      strcat(sql_insert, name);
      strcat(sql_insert, "', '");
      strcat(sql_insert, password);
      strcat(sql_insert, "')");

      //判断map中能否找到重复的用户名
      if (users.find(name) == users.end()) {
        //向数据库中插入数据时，需要通过锁来同步数据
        m_lock.lock();
        int res = mysql_query(mysql, sql_insert);
        if (!res)
        users.insert(pair<string, string>(name, password));
        m_lock.unlock();

        //校验成功，跳转登录页面
        if (!res)
          strcpy(m_url, "/login.html");
        //校验失败，跳转注册失败页面
        else
          strcpy(m_url, "/registerError.html");
      } else
        strcpy(m_url, "/registerError.html");
    }
    //如果是登录，直接判断
    //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
    else if (flag == '2') {
      if (users.find(name) != users.end() && users[name] == password)
        strcpy(m_url, "/welcome.html");
      else
        strcpy(m_url, "/loginError.html");
    }
    cout << "do_request: " << m_url << endl;
    
  }

  if (*(p + 1) == '0') {
    char *m_url_real = (char *)malloc(sizeof(char) * 200);
    strcpy(m_url_real, "/register.html");
    strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
    free(m_url_real);

  } else if (*(p + 1) == '1') {
    char *m_url_real = (char *)malloc(sizeof(char) * 200);
    strcpy(m_url_real, "/login.html");
    strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
    free(m_url_real);
  } else {
    // 使用 FILENAME_LEN - len - 1 以确保不会溢出
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
  }

  if (stat(m_real_file, &m_file_stat) < 0) {
    return NO_RESOURCE;
  }
  if (!(m_file_stat.st_mode & S_IROTH)) {
    return FORBIDDEN_REQUEST;
  }
  if (S_ISDIR(m_file_stat.st_mode)) {
    return BAD_REQUEST;
  }

  //打开文件
  int fd = open(m_real_file, O_RDONLY);
  // MAP_PRIVATE表示私有映射（即对内存区域的修改不会反映到文件中）
  m_file_address =
      (char *)mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

  //避免文件描述符的浪费和占用
  close(fd);

  //表示请求文件存在，且可以访问
  return FILE_REQUEST;
}

//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN

// m_read_idx指向缓冲区m_read_buf的数据末尾的下一个字节
// m_checked_idx指向从状态机当前正在分析的字节
//  \r\n为一行
http_conn::LINE_STATUS http_conn::parse_line() {
  char temp;
  for (; m_checked_idx < m_read_idx; m_checked_idx++) {
    temp = m_read_buf[m_checked_idx];

    if (temp == '\r') {
      //检查下一个字符是不是 '\n'
      if ((m_checked_idx + 1) == m_read_idx) {
        //到buf的末尾了，需要继续接受
        return LINE_OPEN;
      } else if (m_read_buf[m_checked_idx + 1] == '\n') {
        m_read_buf[m_checked_idx++] = '\0';
        m_read_buf[m_checked_idx++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
      //如果当前字符是\n，也有可能读取到完整行
      //一般是上次读取到\r就到buffer末尾了，没有接收完整，再次接收时会出现这种情况
    } else if (temp == '\n') {
      //前一个字符是\r，则接收完整
      if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r') {
        m_read_buf[m_checked_idx - 1] = '\0';
        m_read_buf[m_checked_idx++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
  }
  return LINE_OPEN;
}

bool http_conn::add_response(const char *format, ...) {
  if (m_write_idx >= WRITE_BUFFER_SIZE) {
    //超过了缓存的大小
    return false;
  }

  //定义可变参数列表
  va_list arg_list;

  //将变量arg_list初始化为传入参数
  va_start(arg_list, format);

  //将数据format从可变参数列表写入缓冲区写，返回写入数据的长度
  int len = vsnprintf(m_write_buf + m_write_idx,
                      WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);

  //如果写入的数据长度超过缓冲区剩余空间，则报错
  if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)) {
    va_end(arg_list);
    return false;
  }

  //更新m_write_idx位置
  m_write_idx += len;
  //清空可变参列表
  va_end(arg_list);

  return true;
}

//添加状态行
bool http_conn::add_status_line(int status, const char *title) {
  return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

//添加消息报头，具体的添加文本长度、连接状态和空行
bool http_conn::add_headers(int content_len) {
  add_content_length(content_len);
  add_linger();
  add_blank_line();
}

//添加Content-Length，表示响应报文的长度
bool http_conn::add_content_length(int content_len) {
  return add_response("Content-Length:%d\r\n", content_len);
}

//添加文本类型，这里是html
bool http_conn::add_content_type() {
  return add_response("Content-Type:%s\r\n", "text/html");
}

//添加连接状态，通知浏览器端是保持连接还是关闭
bool http_conn::add_linger() {
  return add_response("Connection:%s\r\n",
                      (m_linger == true) ? "keep-alive" : "close");
}
//添加空行
bool http_conn::add_blank_line() { return add_response("%s", "\r\n"); }

//添加文本content
bool http_conn::add_content(const char *content) {
  return add_response("%s", content);
}