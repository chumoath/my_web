#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>

int clnt_sock;

// 信号来了才开始设置为 非阻塞
void alarm_handler(int sig)
{
    if(sig == SIGALRM)
    {

    }
}


int main(int argc, char const *argv[])
{
    int listenfd = -1;
    struct sockaddr_in adr, clnt_adr;
    socklen_t clnt_adr_sz;

    char buf[1024];
    if(argc != 2){
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    listenfd = socket(PF_INET, SOCK_STREAM, 0);


    memset(&adr, 0, sizeof(adr));
    adr.sin_family = AF_INET;
    adr.sin_addr.s_addr = htonl(INADDR_ANY);
    adr.sin_port = htons(atoi(argv[1]));


    bind(listenfd, (struct sockaddr*)&adr, sizeof(adr));

    listen(listenfd, 5);

    // 注册 alarm 信号处理函数
    struct sigaction sig;
    sig.sa_handler = alarm_handler;
    sig.sa_flags = 0;
    sigemptyset(&sig.sa_mask);
    sigaction(SIGALRM, &sig, NULL);

    while(1)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(listenfd, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
        
        // alarm(10); // 十秒后打开非阻塞，并打断睡眠

        // 也可以直接打开非阻塞，信号到了再处理数据
        int status = fcntl(clnt_sock, F_GETFL, 0);
        status |= O_NONBLOCK;
        fcntl(clnt_sock, F_SETFL, status);

        // 每个 socket 都是 10秒后 唤醒，读完数据后 关闭
        // 实际的定时器
        alarm(10); // 10秒后唤醒

        // alarm 只是 注册一个事件而已，不会阻塞
        
        // 相当于 阻塞进程
        sleep(50000); // 会被 唤醒

        int read_cnt = 0;        
        // 若没有数据，< 0，则会直接断开连接
        while((read_cnt = read(clnt_sock, buf, 1024)) > 0){
            // printf("read_cnt = %d\n", read_cnt);
            // 读多少，打印多少
            buf[read_cnt] = 0;
            printf("%s\n", buf);
        }

        close(clnt_sock);
    }

    close(listenfd);

    return 0;
}
