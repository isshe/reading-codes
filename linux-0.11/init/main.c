/*
 *  linux/init/main.c
 *
 *  (C) 1991  Linus Torvalds
 */
/* 定义宏“__LIBRARY__”是为了包括定义在unistd.h中的内嵌汇编代码等信息。*/
#define __LIBRARY__		/*定义了__LIBRARY__才能使用那些接口*/

/*
// *.h头文件所在的默认目录是include/，则在代码中就不用明确指明其位置。如果不是UNIX的
// 标准头文件，则需要指明所在的目录，并用双引号括住。unistd.h是标准符号常数与类型文件。
// 其中定义了各种符号常数和类型，并声明了各种函数。如果还定义了符号__LIBRARY__，则还会
// 包含系统调用号和内嵌汇编代码syscall0()等。
*/
#include <unistd.h>
#include <time.h>

/*
 * we need this inline - forking from kernel space will result
 * in NO COPY ON WRITE (!!!), until an execve is executed. This
 * is no problem, but for the stack. This is handled by not letting
 * main() use the stack at all after fork(). Thus, no function
 * calls - which means inline code for fork too, as otherwise we
 * would use the stack upon exit from 'fork()'.
 *
 * Actually only pause and fork are needed inline, so that there
 * won't be any messing with the stack from main(), but we define
 * some others too.
 */

/* 			// ?????---isshe----
* 我们需要下面这些内嵌语句 - 从内核空间创建进程将导致没有写时复制(COPY ON WRITE)!!!
* 直到执行一个execve调用。这对堆栈可能带来问题。处理方法是在fork()调用后不让main()
* 使用任何堆栈。因此就不能有函数调用 - 这意味着fork也要使用内嵌的代码，否则我们在从
* fork()退出时就要使用堆栈了。
*
* 实际上只有pause和fork需要使用内嵌方式，以保证从main()中不会弄乱堆栈，但是我们同
* 时还定义了其他一些函数。
*/

/*			// !!!!!!----isshe---- // 任务0和任务1都是在内核空间？！
// Linux在内核空间创建进程时不使用写时复制技术（Copy on write）。main()在移动到用户
// 模式（到任务0）后执行内嵌方式的fork()和pause()，因此可保证不使用任务0的用户栈。
// 在执行moveto_user_mode()之后，本程序main()就以任务0的身份在运行了。而任务0是所
// 有将创建子进程的父进程。当它创建一个子进程时（init进程），由于任务1代码属于内核
// 空间，因此没有使用写时复制功能。此时任务0的用户栈就是任务1的用户栈，即它们共同
// 使用一个栈空间。因此希望在main.c运行在任务0的环境下时不要有对堆栈的任何操作，以
// 免弄乱堆栈。而在再次执行fork()并执行过execve()函数后，被加载程序已不属于内核空间，
// 因此可以使用写时复制技术了。请参见5.3节“Linux内核使用内存的方法”内容。

// 下面_syscall0()是unistd.h中的内嵌宏代码。以嵌入汇编的形式调用Linux的系统调用中断
// 0x80。该中断是所有系统调用的入口。该条语句实际上是int fork()创建进程系统调用。可展
// 开看之就会立刻明白。syscall0名称中最后的0表示无参数，1表示1个参数。
// 参见include/unistd.h，133行(_syscall0)。
*/
static inline _syscall0(int,fork)
static inline _syscall0(int,pause) 		/*int pause()系统调用：暂停进程的执行，直到收到一个信号。*/
static inline _syscall1(int,setup,void *,BIOS)	/* int setup(void * BIOS)系统调用，仅用于linux初始化（仅在这个程序中被调用）。*/
static inline _syscall0(int,sync)		/*nt sync()系统调用：更新文件系统*/

#include <linux/tty.h>				/* tty头文件，定义了有关tty_io，串行通信方面的参数、常数。*/
#include <linux/sched.h>			/* 调度程序头文件，定义了任务结构task_struct、第1个初始任务的数据。*/
									/* 还有一些以宏的形式定义的有关描述符参数设置和获取的嵌入式汇编函数程序。*/
#include <linux/head.h>				/* head头文件，定义了段描述符的简单结构，和几个选择符常量。*/
#include <asm/system.h>				/* 系统头文件。以宏形式定义了许多有关设置或修改描述符/中断门等的嵌入式汇编子程序。*/
#include <asm/io.h>					/* io头文件。以宏的嵌入汇编程序形式定义对io端口操作的函数。*/

#include <stddef.h>					/* 标准定义头文件。定义了NULL, offsetof(TYPE, MEMBER)。*/
#include <stdarg.h>					/* 标准参数头文件。以宏的形式定义变量参数列表。主要说明了-个类型(va_list) */
									/* 和三个宏(va_start, va_arg和va_end)，vsprintf、vprintf、vfprintf。*/
#include <unistd.h>
#include <fcntl.h>					/* 文件控制头文件。用于文件及其描述符的操作控制常数符号的定义。*/
#include <sys/types.h>				/* 类型头文件。定义了基本的系统数据类型。*/

#include <linux/fs.h>				/* 文件系统头文件。定义文件表结构（file,buffer_head,m_inode等）。其中有定义：extern int ROOT_DEV。*/

static char printbuf[1024];			/* 静态字符串数组，用作内核显示信息的缓存。*/

extern int vsprintf();				/* 送格式化输出到一字符串中*/
extern void init(void);				/* 函数原形，初始化（本程序168行）。*/
extern void blk_dev_init(void);		/* 块设备初始化子程序（blk_drv/ll_rw_blk.c,157行）*/
extern void chr_dev_init(void);		/* 字符设备初始化（chr_drv/tty_io.c, 347行）*/
extern void hd_init(void);			/* 硬盘初始化程序（blk_drv/hd.c, 343行）*/
extern void floppy_init(void);		/* 软驱初始化程序（blk_drv/floppy.c, 457行）*/
extern void mem_init(long start, long end);		/* 内存管理初始化（mm/memory.c, 399行）*/
extern long rd_init(long mem_start, int length);		/* 虚拟盘初始化(blk_drv/ramdisk.c,52)*/
extern long kernel_mktime(struct tm * tm);		/* 计算系统开机启动时间（秒）。*/
extern long startup_time;			/**/

/*
 * This is set up by the setup-routine at boot-time
 */
/*
* 以下这些数据是在内核引导期间由setup.s程序设置的。
*/
// 下面三行分别将指定的线性地址强行转换为给定数据类型的指针，并获取指针所指内容。由于
// 内核代码段被映射到从物理地址零开始的地方，因此这些线性地址正好也是对应的物理地址。
// 这些指定地址处内存值的含义请参见第6章的表6-2（setup程序读取并保存的参数）。
// drive_info结构请参见下面第125行。
#define EXT_MEM_K (*(unsigned short *)0x90002)
#define DRIVE_INFO (*(struct drive_info *)0x90080)
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */
/*
* 是啊，是啊，下面这段程序很差劲，但我不知道如何正确地实现，而且好象
* 它还能运行。如果有关于实时时钟更多的资料，那我很感兴趣。这些都是试
* 探出来的，另外还看了一些bios程序，呵！
*/
/*这段宏读取CMOS实时时钟信息。outb_p和inb_p是include/asm/io.h中定义的端口输入输出宏。*/
#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \		/* 0x70是写地址端口号，0x80|addr是要读取的CMOS内存地址。*/
inb_p(0x71); \					/* 0x71是读数据端口号。*/
})

/*
// 定义宏。将BCD码转换成二进制数值。BCD码利用半个字节（4比特）表示一个10进制数，因此
// 一个字节表示2个10进制数。(val)&15取BCD表示的10进制【个位数】，而 (val)>>4取BCD表示
// 的10进制【十位数】，再乘以10。因此最后两者相加就是一个字节BCD码的实际二进制数值。
*/
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

/* 
// 该函数取CMOS实时钟信息作为开机时间，并保存到全局变量startup_time(秒)中。参见后面
// CMOS内存列表说明。其中调用的函数kernel_mktime()用于计算从1970年1月1日0时起到
// 开机当日经过的秒数，作为开机时间（kernel/mktime.c 41行）。
*/	// 如果开机时，时间不对，那么得到的时间不就不正确喽？？？---isshe---
static void time_init(void)
{
	struct tm time;

	do {
		time.tm_sec = CMOS_READ(0);				/* 当前时间秒值（均是BCD码值）。*/
		time.tm_min = CMOS_READ(2);				/* 当前分钟值。*/
		time.tm_hour = CMOS_READ(4);			/* 当前小时值。*/
		time.tm_mday = CMOS_READ(7);			/* 一月中的当天日期。*/
		time.tm_mon = CMOS_READ(8);				/* 当前月份（1—12）。*/
		time.tm_year = CMOS_READ(9);			/* 当前年份。*/
	} while (time.tm_sec != CMOS_READ(0));
	BCD_TO_BIN(time.tm_sec);					/* 转换成二进制数值。*/
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	BCD_TO_BIN(time.tm_year);
	time.tm_mon--;								 /* tm_mon中月份范围是0—11。*/
	startup_time = kernel_mktime(&time);		 /* 计算开机时间。kernel/mktime.c 41行。*/
}

static long memory_end = 0;						/* 机器具有的物理内存容量（字节数）。*/
static long buffer_memory_end = 0;				/* 高速缓冲区末端地址。*/
static long main_memory_start = 0;				/* 主内存（将用于分页）开始的位置。*/

struct drive_info { char dummy[32]; } drive_info;		// 用于存放硬盘参数表信息。

// 内核初始化主程序。初始化结束后将以任务0（idle任务即空闲任务）的身份运行。
// 英文注释含义是“这里确实是void，没错。在startup程序(head.s)中就是这样假设的”。参见
// head.s程序第136行开始的几行代码。
void main(void)		/* This really IS void, no error here. */
{			/* The startup routine assumes (well, ...) this */
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
/*
* 此时中断仍被禁止着，做完必要的设置后就将其开启。
*/
// 首先保存根文件系统设备号和交换文件设备号，并根据setup.s程序中获取的信息设置控制台
// 终端屏幕行、列数环境变量TERM，并用其设置初始init进程中执行etc/rc文件和shell程序
// 使用的环境变量，以及复制内存0x90080处的硬盘参数表。
// 其中ROOT_DEV 已在前面包含进的include/linux/fs.h文件第206行上被声明为extern int，
// 而SWAP_DEV在include/linux/mm.h文件内也作了相同声明。这里mm.h文件并没有显式地列在
// 本程序前部，因为前面包含进的include/linux/sched.h文件中已经含有它。

	ROOT_DEV = ORIG_ROOT_DEV;					// ROOT_DEV定义在fs/super.c，29行。
	drive_info = DRIVE_INFO;					// 复制内存0x90080处的硬盘参数表。

    // 接着根据机器物理内存容量设置高速缓冲区和主内存区的位置和范围。
    // 高速缓存末端地址->buffer_memory_end；
	// 机器内存容量->memory_end；
    // 主内存开始地址->main_memory_start；
	memory_end = (1<<20) + (EXT_MEM_K<<10);		// 内存大小=1Mb + 扩展内存(k)*1024字节。
	memory_end &= 0xfffff000;					// 忽略不到4Kb（1页）的内存数。
	if (memory_end > 16*1024*1024)				// 如果内存量超过16Mb，则按16Mb计。
		memory_end = 16*1024*1024;
	if (memory_end > 12*1024*1024) 				// 如果内存>12Mb，则设置缓冲区末端=4Mb
		buffer_memory_end = 4*1024*1024;
	else if (memory_end > 6*1024*1024) 			// 否则若内存>6Mb，则设置缓冲区末端=2Mb
		buffer_memory_end = 2*1024*1024;
	else
		buffer_memory_end = 1*1024*1024;		// 否则则设置缓冲区末端=1Mb
	main_memory_start = buffer_memory_end;		// 主内存起始位置 = 缓冲区末端。

// 如果在Makefile文件中定义了内存虚拟盘符号RAMDISK，则初始化虚拟盘。此时主内存将减少。
//参见kernel/blk_drv/ramdisk.c。
#ifdef RAMDISK
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024);
#endif

    // 以下是内核进行所有方面的初始化工作。阅读时最好跟着调用的程序深入进去看，			!!!!!!!!!!!!!!!!!
	//若实在看不下去了，就先放一放，继续看下一个初始化调用 -- 这是经验之谈 - -。
	mem_init(main_memory_start,memory_end);		// 主内存区初始化。（mm/memory.c，399）
	trap_init();								// 陷阱门（硬件中断向量）初始化；设置各种向量及其对应的处理函数
	blk_dev_init();								// 块设备初始化。
	chr_dev_init();								// 字符设备初始化。
	tty_init();									// tty初始化。
	time_init();								// 时间初始化，就设置了一下开机时间，函数在本文件上面
	sched_init();								// 内核调度程序的初始化子程序。设置TSS LDT之类的
	buffer_init(buffer_memory_end);				// 缓冲区初始化函数。缓冲管理初始化，建内存链表等
	hd_init();									// 硬盘初始化。
	floppy_init();								// 软驱初始化。
	sti();										// 所有初始化工作都做完了，开中断。

	// 下面过程通过在堆栈中设置的参数，利用中断返回指令启动任务0执行。
	move_to_user_mode();						// 移到用户模式下执行，不懂！！！
	if (!fork()) {		/* we count on this going ok */
		init();
	}
/*
 *   NOTE!!   For any other task 'pause()' would mean we have to get a
 * signal to awaken, but task0 is the sole exception (see 'schedule()')
 * as task 0 gets activated at every idle moment (when no other tasks
 * can run). For task0 'pause()' just means we go check if some other
 * task can run, and if not we return here.
 */
	for(;;) pause();
}

static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}

static char * argv_rc[] = { "/bin/sh", NULL };
static char * envp_rc[] = { "HOME=/", NULL };

static char * argv[] = { "-/bin/sh",NULL };
static char * envp[] = { "HOME=/usr/root", NULL };

void init(void)
{
	int pid,i;

	setup((void *) &drive_info);
	(void) open("/dev/tty0",O_RDWR,0);
	(void) dup(0);
	(void) dup(0);
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
		NR_BUFFERS*BLOCK_SIZE);
	printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);
	if (!(pid=fork())) {
		close(0);
		if (open("/etc/rc",O_RDONLY,0))
			_exit(1);
		execve("/bin/sh",argv_rc,envp_rc);
		_exit(2);
	}
	if (pid>0)
		while (pid != wait(&i))
			/* nothing */;
	while (1) {
		if ((pid=fork())<0) {
			printf("Fork failed in init\r\n");
			continue;
		}
		if (!pid) {
			close(0);close(1);close(2);
			setsid();
			(void) open("/dev/tty0",O_RDWR,0);
			(void) dup(0);
			(void) dup(0);
			_exit(execve("/bin/sh",argv,envp));
		}
		while (1)
			if (pid == wait(&i))
				break;
		printf("\n\rchild %d died with code %04x\n\r",pid,i);
		sync();
	}
	_exit(0);	/* NOTE! _exit, not exit() */
}
