#include "kernel/types.h"
#include "user/user.h"
// 把从控制台输入的内容打印到控制台
int main()
{
    char buf[64];
    while(1){
        // 从console读取输入，通过system call中的 read函数
        int n = read(0,buf,sizeof(buf)); // 0 : 文件描述符，代表输入 buf ：读进来的内容存入buf数组， 读取sizeof(buf)大小的内容
        if(n<=0){
           break; //无输入的时候结束程序，进入下一次循环
        }
        write(1,buf,n);
    }
    exit(0);
}