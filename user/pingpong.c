// pingpong.c
// 编写一个使用UNIX系统调用的程序来在两个进程之间“ping-pong”一个字节，请使用两个管道，每个方向一个。父进程应该向子进程发送一个字节;
// 子进程应该打印“<pid>: received ping”，其中<pid>是进程ID，并在管道中写入字节发送给父进程，然后退出;父级应该从读取从子进程而来的字节，
// 打印“<pid>: received pong”，然后退出。您的解决方案应该在文件user/pingpong.c中。
// int p[2];
// char *argv[2];
// argv[0] = "wc";
// argv[1] = 0;
// pipe(p); // 建立一个管道
// if (fork() == 0) { 在子进程中
//     close(0);  //关闭标准输入
//     dup(p[0]); //把管道的读端复制到标准输入
//     close(p[0]); //关闭原来的读端
//     close(p[1]); 关闭写端，子进程不需要写
//     exec("/bin/wc", argv); 执行wc命令
// } else { 父进程中
//     close(p[0]);  关闭读端
//     write(p[1], "hello world\n", 12); 向管道写入字符串
//     close(p[1]); 写完后关闭写端
// }

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#define RD 0
#define WR 1
int main(int argc, char const * argv[]){
    char buf = 'P'; //要发送的字符串
    
    int fd_c2p[2]; // 管道1 子进程->父进程 子进程写，父进程读
    int fd_p2c[2]; // 管道2 父进程->子进程 父进程写，子进程读
    pipe(fd_c2p);
    pipe(fd_p2c);

    int pid = fork(); // 创建子进程，返回子进程的PID
    int exist_status = 0; //记录退出时的状态 0/1
    if(pid < 0){
        // 子进程创建失败
        fprintf(2,"fork() error\n"); // 2代表标准错误输出的文件描述符
        //关闭所有管道读写端
        close(fd_c2p[RD]);
        close(fd_c2p[WR]);
        close(fd_p2c[RD]);
        close(fd_p2c[WR]);
        exit(1);
    }else if(pid == 0){// 子进程
        //子进程读取父端写入的数据并且将数据写入管道，此时管道1中父进程不需要读，管道2中父进程不用写，需要进行关闭
        close(fd_c2p[RD]);
        close(fd_p2c[WR]);
        if(read(fd_p2c[RD],&buf,sizeof(char)) != sizeof(char)){ //希望读入一个字节大小的东西存入缓存区buf
            fprintf(2, "child read() error\n");
            exist_status = 1;
        }else{
            fprintf(1, "%d: received ping\n",getpid()); //子进程完成读取操作
        }
        if(write(fd_c2p[WR],&buf,sizeof(char)) != sizeof(char)){
            fprintf(2, "child write() error\n");
            exist_status = 1;
        }
        close(fd_c2p[WR]);
        close(fd_p2c[RD]);//使用完毕后关闭管道端
        exit(exist_status);
    }else{ //父进程
        //父进程向子进程发送一个字节，然后接收来自子进程的一个字节，此时需要管道2的写端和管道1的读端，其余的关闭
        close(fd_c2p[WR]);
        close(fd_p2c[RD]);
        if(write(fd_p2c[WR], &buf, sizeof(char)) != sizeof(char)){
            fprintf(2, "Parent write() error\n");
            exist_status = 1;
        }
        if(read(fd_c2p[RD], &buf, sizeof(char)) != sizeof(char)){ //希望读入一个字节大小的东西存入缓存区buf
            fprintf(2, "Parent read() error\n");
            exist_status = 1;
        }else{
            fprintf(1, "%d: received pong\n",getpid()); //父进程完成读取操作
        }
        close(fd_c2p[RD]);
        close(fd_p2c[WR]);//使用完毕后关闭管道端
        exit(exist_status);
    }
}