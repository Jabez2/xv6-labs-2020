#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char const *argv[]) // argc: 参数的数量   argv[] ：参数数组 第一个参数为文件名 第二个参数为睡眠的时间
{
  if (argc != 2) { //参数错误
    fprintf(2, "usage: sleep <time>\n");
    exit(1);
  }
  sleep(atoi(argv[1])); // 将字符串转为数字
  exit(0);
}
