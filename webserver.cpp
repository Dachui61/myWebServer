#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <errno.h>

#define PORT 8989
#define MAX_EVENTS 10000

int main(int argc, char const *argv[])
{
    // 创建监听socket文件描述符
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    // 设置端口复用
    int flag = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1) {
        perror("setsockopt");
        close(listenfd);
        return 1;
    }

    if (bind(listenfd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind");
        close(listenfd);
        return 1;
    }

    if (listen(listenfd, 20) == -1) {
        perror("listen");
        close(listenfd);
        return 1;
    }

    // 创建epoll实例
    epoll_event events[MAX_EVENTS];
    int epollfd = epoll_create(5);
    if (epollfd == -1) {
        perror("epoll_create");
        close(listenfd);
        return 1;
    }

    // 将监听socket添加到epoll实例中
    epoll_event event;
    event.data.fd = listenfd;
    event.events = EPOLLIN;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event) == -1) {
        perror("epoll_ctl");
        close(listenfd);
        close(epollfd);
        return 1;
    }

    while (1) {
        int number = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (number == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                // 有新连接
                struct sockaddr_in client;
                socklen_t client_len = sizeof(client);
                int confd = accept(listenfd, (struct sockaddr*)&client, &client_len);
                if (confd == -1) {
                    perror("accept");
                    continue;
                }

                char ipstr[INET_ADDRSTRLEN];
                printf("client ip %s \t port %d\n", inet_ntop(AF_INET, &client.sin_addr, ipstr, sizeof(ipstr)), ntohs(client.sin_port));

                // 将新连接添加到epoll实例中
                event.data.fd = confd;
                event.events = EPOLLIN;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, confd, &event) == -1) {
                    perror("epoll_ctl");
                    close(confd);
                    continue;
                }
            } else if (events[i].events & EPOLLIN) {
                // 有数据输入
                char buf[2048]; // 调整缓冲区大小以避免栈溢出
                int r = 0;
                while ((r = recv(sockfd, buf, sizeof(buf) - 1, 0)) > 0) {
                    buf[r] = '\0'; // 确保缓冲区以null结尾
                    printf("recv: %s\n", buf);
                }

                if (r == 0) {
                    // 对端关闭连接
                    printf("client disconnected\n");
                    close(sockfd);
                } else if (r == -1 && errno != EAGAIN) {
                    // 读取数据出错
                    perror("recv");
                    close(sockfd);
                }
            } else if (events[i].events & EPOLLOUT) {
                // 有数据输出
                // 你的逻辑
            }
        }
    }

    close(listenfd);
    close(epollfd);
    return 0;
}
