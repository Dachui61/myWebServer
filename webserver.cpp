#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>

#define PORT 8989
int main(int argc, char const *argv[])
{
    /* code */
    //创建监听socket文件描述符
    int listenfd = socket(PF_INET,SOCK_STREAM,0);
    
    int ret = 0;

    /* Structure describing an Internet socket address.  */
    // struct sockaddr_in
    //   {
    //     __SOCKADDR_COMMON (sin_);
    //     in_port_t sin_port;			/* Port number.  */
    //     struct in_addr sin_addr;		/* Internet address.  */

    //     /* Pad to size of `struct sockaddr'.  */
    //     unsigned char sin_zero[sizeof (struct sockaddr)
    // 			   - __SOCKADDR_COMMON_SIZE
    // 			   - sizeof (in_port_t)
    // 			   - sizeof (struct in_addr)];
    //   };
    
    //装ip地址
    struct sockaddr_in address;
    socklen_t addrlen;
    bzero(&address , sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    //设置端口复用 为啥要设置flag??
    int flag = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag));

    ret = bind(listenfd,(struct sockaddr *)&address,sizeof(address));

    ret = listen(listenfd,20);
    
    //epoll 创建内核事件表
    epoll_event events[10000];
    int epollfd = epoll_create(5);


    // typedef union epoll_data
    // {
    // void *ptr;
    // int fd;
    // uint32_t u32;
    // uint64_t u64;
    // } epoll_data_t;

    // struct epoll_event
    // {
    // uint32_t events;	/* Epoll events */
    // epoll_data_t data;	/* User data variable */
    // } __EPOLL_PACKED;

    epoll_event event;
    event.data.fd = listenfd;
    event.events = EPOLLIN;

    epoll_ctl(epollfd,EPOLL_CTL_ADD,listenfd,&event);


    
    int events_num;
    while(1){
        int number = epoll_wait(epollfd,events,10000,-1);
        for(int i = 0 ; i < number ; i++){
            //取到发生变化的文件描述符
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd){
                //有新连接
                struct sockaddr_in client;
                int confd = accept(listenfd,(struct sockaddr*)&client,&addrlen);
                char ipstr[128];
                printf("client ip %s \t port %d\n",inet_ntop(AF_INET,
                (struct sockaddr *)&client.sin_addr.s_addr,ipstr,sizeof(ipstr)),
                ntohs(client.sin_port));
                epoll_event event;
                event.data.fd = confd;
                event.events = EPOLLIN;
                epoll_ctl(epollfd,EPOLL_CTL_ADD,confd,&event);

            }else if(events[i].events & EPOLLIN){
                //有数据输入
                char buf[20480];
                int r = 0;
                while(1){
                    r = recv(sockfd,buf,sizeof(buf),0);
                    if(r == -1 || r == 0){
                        break;
                    }
                    printf("recv:%s \n",buf);
                }
                
            }else if (events[i].events & EPOLLOUT){
                //有数据输出
                
            }
        }
    }
    

    /* Convert a Internet address in binary network format for interface
   type AF in buffer starting at CP to presentation form and place
   result in buffer of length LEN astarting at BUF.  */
// extern const char *inet_ntop (int __af, const void *__restrict __cp,
// 			      char *__restrict __buf, socklen_t __len)
//      __THROW;
    
    
    
    // close(confd);
    return 0;
}
