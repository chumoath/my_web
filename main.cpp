#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "locker.h"
#include "threadpool.h"
#include <signal.h>
#include "http_conn.h"
#define MAX_FD 65535           // 最大连接数
#define MAX_EVENT_NUMBER 10000 // epoll 监听的最大数量

// 添加信号捕捉
void addsig(int sig, void(handler)(int))
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

// 添加文件描述符到 epoll 中
extern void addfd(int epollfd, int fd, bool one_shot);
// 从epoll 中删除文件描述符
extern void removefd(int epollfd, int fd);
// 修改文件描述符
extern void modfd(int epollfd, int fd, int ev);

int main(int argc, char const *argv[])
{
    if (argc <= 1)
    {
        printf("Usage: %s <port>\n", basename(argv[0]));
        exit(-1);
    }

    // 对 SIGPIE信号进行处理
    addsig(SIGPIPE, SIG_IGN);

    // 创建线程池，初始化线程池
    threadpool<http_conn> *pool = NULL;
    try
    {
        pool = new threadpool<http_conn>;
    }
    catch (...)
    {
        exit(-1);
    }

    // 创建一个数组用于保存所有的客户端信息
    http_conn *users = new http_conn[MAX_FD];

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    // 设置端口复用
    // 在绑定之前设置
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 绑定
    sockaddr_in listen_adr; // c++

    memset(&listen_adr, 0, sizeof(listen_adr));
    listen_adr.sin_family = AF_INET;
    listen_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_adr.sin_port = htons(atoi(argv[1]));

    if (bind(listenfd, (sockaddr *)&listen_adr, sizeof(listen_adr)) < 0)
    {
        perror("bind");
        exit(-1);
    }
    // 监听
    if (listen(listenfd, 5) < 0)
    {
        perror("listen");
        exit(-1);
    }

    // 创建  epoll 对象，事件数组，添加
    // 一次可以接收的事件数量
    epoll_event events[MAX_EVENT_NUMBER];
    // size 被忽略
    int epollfd = epoll_create(5);

    // 将监听的文件描述符添加到 epoll 对象中
    addfd(epollfd, listenfd, false); // 不需要 oneshot
    // 静态成员变量
    http_conn::m_epollfd = epollfd;

    while (true)
    {
        // -1 表示 无限等待
        // 若为水平触发，只要数据没有读，应该一直都有才对
        // 但是 非 listenfd 设置了 oneshot，所以 该 描述符仅仅提示一次

        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        // 返回 -1 并且 错误代码不是 eintr，即不是被中断
        if ((num < 0) && (errno != EINTR))
        {
            printf("epoll failure\n");
            break;
        }

        // 循环遍历事件数组
        for (int i = 0; i < num; i++)
        {
            int sockfd = events[i].data.fd;

            // 是 监听套接字 => 门卫
            if (sockfd == listenfd)
            {
                struct sockaddr_in client_adr;
                socklen_t client_adrlen = sizeof(client_adr);
                int connfd = accept(listenfd, (struct sockaddr *)&client_adr, &client_adrlen);

                if ( connfd < 0 ) {
                    printf( "errno is: %d\n", errno );
                    continue;
                } 

                if (http_conn::m_user_count >= MAX_FD)
                {
                    // 连接数满了
                    // 该客户端一个信息，服务器内部正忙
                    close(connfd);
                    continue;
                }

                // 将新的客户数据初始化，放到数组中
                // connfd 是 递增的，且没有重复的 fd
                users[connfd].init(connfd, client_adr);
                printf("user_count = %d\n", http_conn::m_user_count);
                fflush(stdout);
            }
            // 不是 listenfd
            // else
            // {
                else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
                {
                // printf("user_count = %d\n", http_conn::m_user_count);
                // fflush(stdout);
                    // 对方异常断开 或 错误 等事件
                    users[sockfd].close_conn();
                }
                else if (events[i].events & EPOLLIN)
                {
                    // users => http_conn 数组
                    if (users[sockfd].read())
                    {
                        // 一次性把数据都读完，将 任务挂到 pool 任务队列
                        pool->append(users + sockfd);
                    }
                    else
                    {
                        users[sockfd].close_conn();
                    }
                }
                else if (events[i].events & EPOLLOUT)
                {
                    // 一次性写完所有数据
                    if (!users[sockfd].write())
                    {
                        users[sockfd].close_conn();
                    }
                }
            // }
        }
    }

    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;
}
