
# Linux内核


## A. 内核相关文件的作用
* init/main.c: 内核初始化。（进程管理、内存管理等初始化都在这里）
    * start_kernel():各种初始化。
    * do_basic_setup(): 内存管理、进程管理起来后调用，进行一些设置工作。
* include/linux/netdevice.h: 网络设备相关结构。
* include/linux/notifier.h: 通知链相关。
* include/linux/skbuff.h: sk_buff相关。
* include/linux/interrupt.h: 中断相关函数的定义。
* kernel/irq/manage.c: 中断相关函数的实现。
* include/uapi/linux/if_packet.h: 数据包的类型定义等。