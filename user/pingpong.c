#include "../kernel/types.h"
#include "user.h"

int main(int argc, char* argv[]) {
    int pipe_fd[2]; // 管道的文件描述符

    pipe(pipe_fd); // 创建管道
    char buf[4];
    char f_out[] = "pong";
    char s_out[] = "ping";
    int pid;

    if ((pid = fork()) == 0) { // 创建子进程
        read(pipe_fd[0], buf, sizeof(buf)); // 读取数据
        printf("%d: received %s\n", getpid(), buf);
        write(pipe_fd[1], f_out, sizeof(buf)); // 写入数据
        close(pipe_fd[1]); // 关闭管道1
        exit(0);    // 退出子进程
    } else if (pid > 0) { // 父进程
        write(pipe_fd[1], s_out, sizeof(buf)); // 写入数据
        wait(0); // 等待子进程结束
        read(pipe_fd[0], buf, sizeof(buf)); // 读取数据
        printf("%d: received %s\n", getpid(), buf);
        close(pipe_fd[0]); // 关闭管道0
        exit(0); // 退出父进程
    } else { // 创建子进程失败
        printf("fork failed!\n");
        exit(-1);
    }
    return 0;
}

