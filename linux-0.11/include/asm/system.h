// 移动到用户模式运行。
// 该函数利用iret指令实现从内核模式移动到初始任务0中去执行。

// 保存堆栈指针esp到eax寄存器中。
// 首先将堆栈段选择符(SS)入栈。(0x17)
// 然后将保存的堆栈指针值(esp)入栈。
// 将标志寄存器(eflags)内容入栈。
// 将Task0代码段选择符(cs)入栈。（0x0f）
// 将下面标号1的偏移地址(eip)入栈。
// 执行中断返回指令，则会跳转到下面标号1处。（不需要中断也可以执行中断返回指令吗？！）
// 此时开始执行任务0，
// 初始化段寄存器指向本局部表的数据段。
#define move_to_user_mode() \
__asm__ ("movl %%esp,%%eax\n\t" \
	"pushl $0x17\n\t" \
	"pushl %%eax\n\t" \
	"pushfl\n\t" \
	"pushl $0x0f\n\t" \
	"pushl $1f\n\t" \
	"iret\n" \
	"1:\tmovl $0x17,%%eax\n\t" \
	"movw %%ax,%%ds\n\t" \
	"movw %%ax,%%es\n\t" \
	"movw %%ax,%%fs\n\t" \
	"movw %%ax,%%gs" \
	:::"ax")

#define sti() __asm__ ("sti"::)
#define cli() __asm__ ("cli"::)
#define nop() __asm__ ("nop"::)

#define iret() __asm__ ("iret"::)

// 设置门描述符宏。
// 根据参数中的中断或异常处理过程地址addr、门描述符类型type和特权级信息dpl，设置位于
// 地址gate_addr处的门描述符。（注意：下面“偏移”值是相对于内核代码或数据段来说的）。
// 参数：gate_addr -描述符地址；type -描述符类型域值；dpl -描述符特权级；addr -偏移地址。
// %0 - (由dpl,type组合成的类型标志字)；%1 - (描述符低4字节地址)；
// %2 - (描述符高4字节地址)；%3 - edx(程序偏移地址addr)；%4 - eax(高字中含有段选择符0x8)。
#define _set_gate(gate_addr,type,dpl,addr) \
__asm__ ("movw %%dx,%%ax\n\t" \
	"movw %0,%%dx\n\t" \
	"movl %%eax,%1\n\t" \
	"movl %%edx,%2" \
	: \
	: "i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
	"o" (*((char *) (gate_addr))), \
	"o" (*(4+(char *) (gate_addr))), \
	"d" ((char *) (addr)),"a" (0x00080000))

// 设置中断门函数（自动屏蔽随后的中断）。
// 参数：n：中断号；addr：中断程序偏移地址。
// &idt[n]是中断描述符表中中断号n对应项的偏移值；中断描述符的类型是14，特权级是0。（类型是什么意思？？？----isshe）
#define set_intr_gate(n,addr) \
	_set_gate(&idt[n],14,0,addr)

#define set_trap_gate(n,addr) \
	_set_gate(&idt[n],15,0,addr)				// 特权级是0

#define set_system_gate(n,addr) \
	_set_gate(&idt[n],15,3,addr)				// 特权级是3

#define _set_seg_desc(gate_addr,type,dpl,base,limit) {\
	*(gate_addr) = ((base) & 0xff000000) | \
		(((base) & 0x00ff0000)>>16) | \
		((limit) & 0xf0000) | \
		((dpl)<<13) | \
		(0x00408000) | \
		((type)<<8); \
	*((gate_addr)+1) = (((base) & 0x0000ffff)<<16) | \
		((limit) & 0x0ffff); }

#define _set_tssldt_desc(n,addr,type) \
__asm__ ("movw $104,%1\n\t" \
	"movw %%ax,%2\n\t" \
	"rorl $16,%%eax\n\t" \
	"movb %%al,%3\n\t" \
	"movb $" type ",%4\n\t" \
	"movb $0x00,%5\n\t" \
	"movb %%ah,%6\n\t" \
	"rorl $16,%%eax" \
	::"a" (addr), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)), \
	 "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7)) \
	)

#define set_tss_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)),"0x89")
#define set_ldt_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)),"0x82")

