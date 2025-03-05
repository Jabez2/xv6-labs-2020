#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
// 用户态陷阱处理函数，处理来自用户模式的中断、异常和系统调用
void
usertrap(void)
{
  int which_dev = 0; // 用于标识中断来源的设备类型

  // 检查是否来自用户模式（SSTATUS_SPP=0 表示用户模式）
  // 若 SPP 位被设置（来自内核模式），触发 panic
  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // 将 STVEC 寄存器指向内核陷阱处理程序（kernelvec）
  // 后续在内核中发生的中断将直接由内核处理，避免递归陷阱
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc(); // 获取当前进程的 proc 结构体

  // 保存用户程序计数器到进程的 trapframe 中
  // 通过读取 SEPC 寄存器获取陷阱发生时的指令地址
  p->trapframe->epc = r_sepc();

  // 根据 SCAUSE 寄存器的值判断陷阱类型
  if(r_scause() == 8) {
    // 系统调用（RISC-V 中 scause=8 表示环境调用，即 ecall 指令）
    
    // 检查进程是否已被标记为终止
    if(p->killed)
      exit(-1);

    // 调整 EPC：ecall 指令占 4 字节，返回到下一条指令
    // 避免重复执行 ecall 导致死循环
    p->trapframe->epc += 4;

    // 在完成关键寄存器操作后，重新启用中断
    // （避免操作过程中被中断破坏寄存器状态）
    intr_on();

    // 执行系统调用处理函数
    syscall();
  } else if((which_dev = devintr()) != 0) {
    // 设备中断处理（devintr 返回非零表示识别到设备中断）
    // 例如定时器中断（返回 2）或磁盘中断（返回 1）
    // which_dev 标识具体的中断来源设备
  } else {
    // 未知类型的陷阱（例如缺页异常、非法指令等）
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1; // 标记进程为已终止
  }

  // 如果进程被终止，调用 exit 释放资源
  if(p->killed)
    exit(-1);

  // 如果是定时器中断（which_dev=2），主动让出 CPU
  // 触发调度器切换到其他进程（实现时间片轮转）
  if(which_dev == 2)
    yield();

  // 准备返回用户态：恢复寄存器、设置 SEPC 等
  usertrapret();
}


//
// return to user space
//
// 从内核态返回用户态的准备工作，最终通过 trampoline 代码切换回用户空间
void
usertrapret(void)
{
  struct proc *p = myproc(); // 获取当前进程的 proc 结构体

  // 关闭中断，确保在切换回用户空间过程中不会被中断打断
  // 中断会在执行完 sret 指令返回用户空间后重新开启
  intr_off();

  // 设置 STVEC 寄存器指向用户态的陷阱处理入口（trampoline.S 中的 uservec）
  // TRAMPOLINE 是内核和用户空间共享的物理内存区域（在虚拟地址空间高位）
  // uservec - trampoline 计算 uservec 在 TRAMPOLINE 区域内的偏移量
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // 配置进程的 trapframe（陷阱帧），供下次陷阱进入内核时使用
  p->trapframe->kernel_satp = r_satp();         // 保存内核页表的 SATP 值
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // 进程内核栈顶（栈从高地址向低地址增长）
  p->trapframe->kernel_trap = (uint64)usertrap;  // 下次陷阱处理函数入口（即 usertrap）
  p->trapframe->kernel_hartid = r_tp();         // 保存 CPU 核心 ID（用于多核环境）

  // 设置 SSTATUS 寄存器，为返回用户态做准备
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // 清除 SPP 位（SPP=0 表示返回用户模式）
  x |= SSTATUS_SPIE; // 启用用户模式下的中断（SPIE=1）
  w_sstatus(x);       // 写回 SSTATUS 寄存器

  // 设置 SEPC 寄存器为保存的用户程序计数器（来自 usertrap 中保存的 epc）
  // 当执行 sret 指令时，PC 会跳转到 SEPC 指向的地址（即用户程序的下一条指令）
  w_sepc(p->trapframe->epc);

  // 生成用户进程页表的 SATP 值（格式：MODE | ASID | PPN）
  // MAKE_SATP 宏会组合页表物理地址和分页模式（如 Sv39）
  uint64 satp = MAKE_SATP(p->pagetable);

  // 跳转到 trampoline.S 中的 userret 函数，完成最后的切换操作
  // TRAMPOLINE 是内核和用户空间共享的物理页，用户和内核都能访问
  // userret - trampoline 计算 userret 在 TRAMPOLINE 区域内的偏移量
  uint64 fn = TRAMPOLINE + (userret - trampoline);

  // 调用 userret 函数，传入两个参数：
  // TRAPFRAME：用户进程 trapframe 的虚拟地址（用户页表内的地址）
  // satp：用户进程页表的 SATP 值
  // （函数指针强制转换，因为 C 语言无法直接传递参数到汇编函数）
  ((void (*)(uint64, uint64))fn)(TRAPFRAME, satp);
}


// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

