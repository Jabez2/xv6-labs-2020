// xargs.c
// $ echo hello too | xargs echo bye
// bye hello too
// $
// int exec(char *file, char *argv[])	加载一个文件并使用参数执行它; 只有在出错时才返回
//int fork()	创建一个进程，返回子进程的PID
//int wait(int *status)	等待一个子进程退出; 将退出状态存入*status; 返回子进程PID。
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 执行某个程序
void run(char *program, char *args[]) {
    if (fork() == 0) { // 子进程执行
        exec(program, args);
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    char buf[2048];        // 存储输入的内存池
    char *current_char = buf;    // 当前字符指针
    char *start_of_arg = buf;    // 当前参数的开始指针
    char *args[128];           // 存储参数的数组

    int arg_count = 0;

    // 将命令行参数 (argv) 复制到 args 数组中
    for (int i = 1; i < argc; i++) {
        args[arg_count] = argv[i];
        arg_count++;
    }

    while (read(0, current_char, 1) != 0) {  // 读取输入字符
        if (*current_char == ' ' || *current_char == '\n') {
            *current_char = '\0';  // 用 \0 结束当前参数
            args[arg_count] = start_of_arg;  // 保存当前参数
            arg_count++;
            start_of_arg = current_char + 1;  // 下一个参数的开始

            if (*current_char == '\n') {  // 如果是行结束
                args[arg_count] = 0;  // 参数列表结尾
                run(argv[1], args);    // 执行命令
                arg_count = 0;         // 重置参数计数器
            }
        }
        current_char++;
    }

    if (arg_count > 0) {  // 如果最后一行不是空的
        *current_char = '\0';  // 结束最后一个参数
        args[arg_count] = 0;   // 参数列表结尾
        run(argv[1], args);    // 执行最后一行命令
    }

    while (wait(0) != -1) {}; // 等待所有子进程结束
    exit(0);
}
