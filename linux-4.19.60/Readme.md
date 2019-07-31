
# Linux内核


## A. 内核相关文件的作用
* include/
    * linux/
        * netdevice.h: 网络设备相关结构。
        * notifier.h: 通知链相关。
        * skbuff.h: sk_buff相关。
        * interrupt.h: 中断相关函数的定义。
        * moduleparam.h: 模块参数相关。
        * mod_devicetable.h: PCI设备标识符等数据结构。
        * pci.h: pci驱动注册/注销等。
        * init.h: 相关初始化。
* include/
    * uapi/
        * linux/
            * if_packet.h: 数据包的类型定义等。
            * if.h: 
* init/
    * main.c: 内核初始化。（进程管理、内存管理等初始化都在这里）
        * start_kernel():各种初始化。
        * do_basic_setup(): 内存管理、进程管理起来后调用，进行一些设置工作。
* kernel/
    * irq/
        * manage.c: 中断相关函数的实现。
* drivers/
    * block/
        * loop.c: module_param和__setup的使用范例。
    * pci/
        * pci-driver.c: pci驱动程序注册等。
            * __pci_register_driver()
    * net/
        * ethernet/
            * intel/
                * e100.c: intel相关的PCI驱动。
* net/
    * core/
        * dev.c: 设备初始化等。
            * net_dev_init()