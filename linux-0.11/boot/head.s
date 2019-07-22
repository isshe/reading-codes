/*
 *  linux/boot/head.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  head.s contains the 32-bit startup code.
 *
 * NOTE!!! Startup happens at absolute address 0x00000000, which is also where
 * the page directory will exist. The startup code will be overwritten by
 * the page directory.
 */

/*
* head.s含有32位启动代码。
* 注意!!! 32位启动代码是从绝对地址0x00000000开始的，这里也同样是页目录将存在的地方，
* 因此这里的启动代码将被页目录覆盖掉。
*/
	
.text
.globl idt,gdt,pg_dir,tmp_floppy_area
pg_dir:									# 页目录将会存放在这里。
# 再次注意!!! 这里已经处于32位运行模式，因此这里的$0x10并不是把地址0x10装入各个
# 段寄存器，它现在其实是全局段描述符表中的偏移值，或者更准确地说是一个描述符表项
# 的选择符。有关选择符的说明请参见setup.s中193行下的说明。这里$0x10的含义是请求
# 特权级0(位0-1=0)、选择全局描述符表(位2=0)、选择表中第2项(位3-15=2)。它正好指
# 向表中的数据段描述符项。（描述符的具体数值参见前面setup.s中212，213行）
# 下面代码的含义是：设置ds,es,fs,gs为setup.s 中构造的数据段（全局段描述符表第2项）
# 的选择符=0x10，并将堆栈放置在stack_start指向的user_stack数组区，然后使用本程序
# 后面定义的新中断描述符表和全局段描述表。新全局段描述表中初始内容与setup.s中的基本
# 一样，仅段限长从8MB修改成了16MB。stack_start定义在kernel/sched.c，69行。它是指向
# user_stack数组末端的一个长指针。第23行设置这里使用的栈，姑且称为系统栈。但在移动到
# 任务0执行（init/main.c中137行）以后该栈就被用作任务0和任务1共同使用的用户栈了。

.globl startup_32
startup_32:					# 设置各个数据段寄存器。
	movl $0x10,%eax			# 对于GNU汇编，每个直接操作数要以'$'开始，否则表示地址。
	mov %ax,%ds				# 每个寄存器名都要以'%'开头，eax表示是32位的ax寄存器。
	mov %ax,%es
	mov %ax,%fs
	mov %ax,%gs
	lss stack_start,%esp	# 表示 stack_start -> ss:esp，设置系统堆栈, stack_start定义在kernel/sched.c，69行。
	call setup_idt			# 调用设置中断描述符表子程序。
	call setup_gdt			# 调用设置全局描述符表子程序。
	movl $0x10,%eax			# reload all the segment registers
	mov %ax,%ds				# after changing gdt. CS was already
	mov %ax,%es				# reloaded in 'setup_gdt'
	mov %ax,%fs				# 因为修改了gdt，所以需要重新装载所有的段寄存器。
	mov %ax,%gs				# CS代码段寄存器已经在setup_gdt中重新加载过了。

    # 由于段描述符中的段限长从setup.s中的8MB改成了本程序设置的16MB（见setup.s行208-216
    # 和本程序后面的235-236行），因此这里再次对所有段寄存器执行加载操作是必须的。另外，通过
    # 使用bochs跟踪观察，如果不对CS再次执行加载，那么在执行到26行时CS代码段不可见部分中
    # 的限长还是8MB。这样看来应该重新加载CS。但是由于setup.s中的内核代码段描述符与本程序中
    # 重新设置的代码段描述符除了段限长以外其余部分完全一样，8MB的限长在内核初始化阶段不会有
    # 问题，而且在以后内核执行过程中段间跳转时会重新加载CS。因此这里没有加载它并没有让程序
    # 出错。
    # 针对该问题，目前内核中就在第25行之后添加了一条长跳转指令：'ljmp $(__KERNEL_CS),$1f'，
    # 跳转到第26行来确保CS确实又被重新加载。
	lss stack_start,%esp

    # 32-36行用于测试A20地址线是否已经开启。采用的方法是向内存地址0x000000处写入任意
    # 一个数值，然后看内存地址0x100000(1M)处是否也是这个数值。如果一直相同的话，就一直
    # 比较下去，也即死循环、死机。表示地址A20线没有选通，结果内核就不能使用1MB以上内存。
    # 33行上的'1:'是一个局部符号构成的标号。标号由符号后跟一个冒号组成。此时该符号表示活动
    # 位置计数（Active location counter）的当前值，并可以作为指令的操作数。局部符号用于帮助
    # 编译器和编程人员临时使用一些名称。共有10个局部符号名，可在整个程序中重复使用。这些符号
    # 名使用名称'0'、'1'、...、'9'来引用。为了定义一个局部符号，需把标号写成'N:'形式（其中N
    # 表示一个数字）。为了引用先前最近定义的这个符号，需要写成'Nb'，其中N是定义标号时使用的
    # 数字。为了引用一个局部标号的下一个定义，需要写成'Nf'，这里N是10个前向引用之一。上面
    # 'b'表示“向后（backwards）”，'f'表示“向前（forwards）”。在汇编程序的某一处，我们最大
    # 可以向后/向前引用10个标号（最远第10个）。
	xorl %eax,%eax
1:	incl %eax				# check that A20 really IS enabled
	movl %eax,0x000000		# loop forever if it isn't
	cmpl %eax,0x100000		# 此时如果打开了A20，应该可以访问高于1M的内存，因此0x000000和0x1000000不会相等
	je 1b					# 如果相等就跳，b表示向后跳，f表示向前跳

/*
 * NOTE! 486 should set bit 16, to check for write-protect in supervisor
 * mode. Then it would be unnecessary with the "verify_area()"-calls.
 * 486 users probably want to set the NE (#5) bit also, so as to use
 * int 16 for math errors.
 */
/*
* 注意! 在下面这段程序中，486应该将位16置位，以检查在超级用户模式下的写保护,
* 此后 "verify_area()" 调用就不需要了。486的用户通常也会想将NE(#5)置位，以便
* 对数学协处理器的出错使用int 16。
*/
# 上面原注释中提到的486 CPU中CR0控制寄存器的位16是写保护标志WP（Write-Protect），
# 用于禁止超级用户级的程序向一般用户只读页面中进行写操作。该标志主要用于操作系统在创建
# 新进程时实现写时复制（copy-on-write）方法。
# 下面这段程序（43-65）用于检查数学协处理器芯片是否存在。方法是修改控制寄存器CR0，在
# 假设存在协处理器的情况下执行一个协处理器指令，如果出错的话则说明协处理器芯片不存在，
# 需要设置CR0中的协处理器仿真位EM（位2），并复位协处理器存在标志MP（位1）。
	movl %cr0,%eax		# check math chip
	andl $0x80000011,%eax	# Save PG,PE,ET
/* "orl $0x10020,%eax" here for 486 might be good */
	orl $2,%eax		# set MP
	movl %eax,%cr0
	call check_x87
	jmp after_page_tables

# ------------------------------------------------------------------------------

/*
 * We depend on ET to be correct. This checks for 287/387.
 */
/*
* 我们依赖于ET标志的正确性来检测287/387存在与否。
*/
check_x87:
	fninit				# 向协处理器发出初始化命令。
	fstsw %ax			# 取协处理器状态字到ax寄存器中。
	cmpb $0,%al			# 初始化后状态字应该为0，否则说明协处理器不存在。
	je 1f				/* no coprocessor: have to set bits */  # 如果存在则向前跳转到标号1处，否则改写cr0。
	movl %cr0,%eax
	xorl $6,%eax		/* reset MP, set EM */
	movl %eax,%cr0
	ret

# 下面是一汇编语言指示符。其含义是指存储边界对齐调整。"2"表示把随后的代码或数据的偏移位置
# 调整到地址值最后2比特位为零的位置（2^2），即按4字节方式对齐内存地址。不过现在GNU as
# 直接时写出对齐的值而非2的次方值了。使用该指示符的目的是为了提高32位CPU访问内存中代码
# 或数据的速度和效率。参见程序后的详细说明。
# 下面的两个字节值是80287协处理器指令fsetpm的机器码。其作用是把80287设置为保护模式。
# 80387无需该指令，并且将会把该指令看作是空操作。
.align 2
1:	.byte 0xDB,0xE4		/* fsetpm for 287, ignored by 387 */	# 287协处理器码。
	ret

/*
 *  setup_idt
 *
 *  sets up a idt with 256 entries pointing to
 *  ignore_int, interrupt gates. It then loads
 *  idt. Everything that wants to install itself
 *  in the idt-table may do so themselves. Interrupts
 *  are enabled elsewhere, when we can be relatively
 *  sure everything is ok. This routine will be over-
 *  written by the page tables.
 */
/*
* 下面这段是设置中断描述符表子程序 setup_idt
*
* 将中断描述符表idt设置成具有256个项，并都指向ignore_int中断门。然后加载中断
* 描述符表寄存器(用lidt指令)。真正实用的中断门以后再安装。当我们在其他地方认为一切
* 都正常时再开启中断。该子程序将会被页表覆盖掉。
*/
# 中断描述符表中的项虽然也是8字节组成，但其格式与全局表中的不同，被称为门描述符
# (Gate Descriptor)。它的0-1,6-7字节是偏移量，2-3字节是选择符，4-5字节是一些标志。
# 这段代码首先在edx、eax中组合设置出8字节默认的中断描述符值，然后在idt表每一项中
# 都放置该描述符，共256项。eax含有描述符低4字节，edx含有高4字节。内核在随后的初始
# 化过程中会替换安装那些真正实用的中断描述符项。

setup_idt:
	lea ignore_int,%edx		# 将ignore_int的有效地址（偏移值）值->edx寄存器
	movl $0x00080000,%eax	# 将选择符0x0008置入eax的高16位中。 
	movw %dx,%ax			/* selector = 0x0008 = cs */ # 偏移值的低16位置入eax的低16位中。此时eax含有门描述符低4字节的值。
	movw $0x8E00,%dx		/* interrupt gate - dpl=0, present */	# 此时edx含有门描述符高4字节的值。

	lea idt,%edi			# idt是中断描述符表的地址。
	mov $256,%ecx			# 256次
rp_sidt:
	movl %eax,(%edi)		# 将哑中断门描述符存入表中。
	movl %edx,4(%edi)		# edx内容放到 edi+4 所指内存位置处。
	addl $8,%edi			# edi指向表中下一项。
	dec %ecx
	jne rp_sidt
	lidt idt_descr			 # 加载中断描述符表寄存器值。
	ret

/*
 *  setup_gdt
 *
 *  This routines sets up a new gdt and loads it.
 *  Only two entries are currently built, the same
 *  ones that were built in init.s. The routine
 *  is VERY complicated at two whole lines, so this
 *  rather long comment is certainly needed :-).
 *  This routine will beoverwritten by the page tables.
 */

# 设置全局描述符表项 setup_gdt
# 这个子程序设置一个新的全局描述符表gdt，并加载。此时仅创建了两个表项，与前
# 面的一样。该子程序只有两行，“非常的”复杂，所以当然需要这么长的注释了。
# 该子程序将被页表覆盖掉。
setup_gdt:
	lgdt gdt_descr
	ret

/*
 * I put the kernel page tables right after the page directory,
 * using 4 of them to span 16 Mb of physical memory. People with
 * more than 16MB will have to expand this.
 */
/* Linus将内核的内存页表直接放在页目录之后，使用了4个表来寻址16 MB的物理内存。
* 如果你有多于16 Mb的内存，就需要在这里进行扩充修改。
*/
# 每个页表长为4 Kb字节（1页内存页面, =4kB），而每个页表项需要4个字节，因此一个页表共可以存放
# 1024个表项。如果一个页表项寻址4 KB的地址空间，则一个页表就可以寻址4 MB的物理内存。
# 页表项的格式为：项的前0-11位存放一些标志，例如是否在内存中(P位0)、读写许可(R/W位1)、
# 普通用户还是超级用户使用(U/S位2)、是否修改过(是否脏了)(D位6)等；表项的位12-31是
# 页框地址，用于指出一页内存的物理起始地址。

# 这下面几句代码要怎么执行？？？？？？？？--------isshe----
.org 0x1000			# 从偏移0x1000处开始是第1个页表（偏移0开始处将存放页表目录）。
pg0:

.org 0x2000
pg1:

.org 0x3000
pg2:

.org 0x4000
pg3:

.org 0x5000			# 定义下面的内存数据块从偏移0x5000处开始。

/*
 * tmp_floppy_area is used by the floppy-driver when DMA cannot
 * reach to a buffer-block. It needs to be aligned, so that it isn't
 * on a 64kB border.
 */
/* 当DMA（直接存储器访问）不能访问缓冲块时，下面的tmp_floppy_area内存块
* 就可供软盘驱动程序使用。其地址需要对齐调整，这样就不会跨越64KB边界。
*/
tmp_floppy_area:
	.fill 1024,1,0		# 共保留1024项，每项1字节，填充数值0。

# 下面这几个入栈操作用于为跳转到init/main.c中的main()函数作准备工作。head.s
# 会弹出main()的地址，并把控制权转移到init/main.c
# 程序中。参见第3章中有关C函数调用机制的说明。
# 前面3个入栈0值应该分别表示envp、argv指针和argc的值，但main()没有用到。
# `pushl $L6`入栈操作是模拟调用main.c程序时首先将返回地址入栈的操作，所以如果
# main.c程序真的退出时，就会返回到这里的标号L6处继续执行下去，也即死循环。
# `pushl $main`将main.c的地址压入堆栈，这样，在设置分页处理（setup_paging）结束后
# 执行'ret'返回指令时就会将main.c程序的地址弹出堆栈，并去执行main.c程序了。
# 有关C函数调用机制请参见程序后的说明。
after_page_tables:
	pushl $0		# These are the parameters to main :-)
	pushl $0		# 这些是调用main程序的参数（指init/main.c）。
	pushl $0		# 其中的'$'符号表示这是一个立即操作数。
	pushl $L6		# return address for main, if it decides to. # 压入了返回地址
	pushl $main		# 压入了main()函数代码的地址
	jmp setup_paging		# 这个函数执行完以后，就会去执行main()
L6:
	jmp L6			# main should never return here, but
					# just in case, we know what happens.
					# main程序绝对不应该返回到这里。不过为了以防万一，
					# 所以添加了该语句。这样我们就知道发生什么问题了。

/* This is the default interrupt "handler" :-) */
/* 下面是默认的中断“向量句柄”（中断处理程序）*/
# 功能：打印下面的一条信息
int_msg:
	.asciz "Unknown interrupt\n\r"
.align 2			# 按4字节方式对齐内存地址。
ignore_int:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds		# 这里请注意！！ds,es,fs,gs等虽然是16位的寄存器，但入栈后
	push %es		# 仍然会以32位的形式入栈，也即需要占用4个字节的堆栈空间。
	push %fs
	movl $0x10,%eax	# 置段选择符（使ds,es,fs指向gdt表中的数据段）。
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	pushl $int_msg	# 把调用printk函数的参数指针（地址）入栈。注意！若int_msg
	call printk		# 前不加'$'，则表示把int_msg符号处的长字（'Unkn'）入栈J。
	popl %eax		# 该函数在/kernel/printk.c中。'_printk'是printk编译后模块中
	pop %fs			# 的内部表示法。
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret			# 中断返回（把中断调用时压入栈的CPU标志寄存器（32位）值也弹出）。


/*
 * Setup_paging
 *
 * This routine sets up paging by setting the page bit
 * in cr0. The page tables are set up, identity-mapping
 * the first 16MB. The pager assumes that no illegal
 * addresses are produced (ie >4Mb on a 4Mb machine).
 *
 * NOTE! Although all physical memory should be identity
 * mapped by this routine, only the kernel page functions
 * use the >1Mb addresses directly. All "normal" functions
 * use just the lower 1Mb, or the local data space, which
 * will be mapped to some other place - mm keeps track of
 * that.
 *
 * For those with more memory than 16 Mb - tough luck. I've
 * not got it, why should you :-) The source is here. Change
 * it. (Seriously - it shouldn't be too difficult. Mostly
 * change some constants etc. I left it at 16Mb, as my machine
 * even cannot be extended past that (ok, but it was cheap :-)
 * I've tried to show which constants to change by having
 * some kind of marker at them (search for "16Mb"), but I
 * won't guarantee that's all :-( )
 */
/*
* 这个子程序通过设置控制寄存器cr0的标志（PG 位31）来启动对内存的分页处理功能，
* 并设置各个页表项的内容，以恒等映射前16 MB的物理内存。分页器假定不会产生非法的
* 地址映射（也即在只有4Mb的机器上设置出大于4Mb的内存地址）。
* 注意！尽管所有的物理地址都应该由这个子程序进行恒等映射，但只有内核页面管理函数能
* 直接使用>1Mb的地址。所有“普通”函数仅使用低于1Mb的地址空间，或者是使用局部数据
* 空间，该地址空间将被映射到其他一些地方去 -- mm（内存管理程序）会管理这些事的。
* 对于那些有多于16Mb内存的家伙 – 真是太幸运了，我还没有，为什么你会有。代码就在
* 这里，对它进行修改吧。（实际上，这并不太困难的。通常只需修改一些常数等。我把它设置
* 为16Mb，因为我的机器再怎么扩充甚至不能超过这个界限（当然，我的机器是很便宜的）。
* 我已经通过设置某类标志来给出需要改动的地方（搜索“16Mb”），但我不能保证作这些
* 改动就行了）。
*/
# 上面英文注释第2段的含义是指在机器物理内存中大于1MB的内存空间主要被用于主内存区。
# 主内存区空间由mm模块管理。它涉及到页面映射操作。内核中所有其他函数就是这里指的一般
# （普通）函数。若要使用主内存区的页面，就需要使用get_free_page()等函数获取。因为主内
# 存区中内存页面是共享资源，必须有程序进行统一管理以避免资源争用和竞争。
#
# 在内存物理地址0x0处开始存放1页页目录表和4页页表。页目录表是系统所有进程公用的，而
# 这里的4页页表则属于内核专用，它们一一映射线性地址起始16MB空间范围到物理内存上。对于
# 新的进程，系统会在主内存区为其申请页面存放页表。另外，1页内存长度是4096字节。

.align 2
setup_paging:
	movl $1024*5,%ecx		/* 5 pages - pg_dir+4 page tables */  	# 首先对5页内存（1页目录 + 4页页表）清零。
	xorl %eax,%eax
	xorl %edi,%edi			/* pg_dir is at 0x000 */				# 页目录从0x000地址开始。
	cld;rep;stosl													# eax内容存到es:edi所指内存位置处，且edi增4。

    # 下面4句设置页目录表中的项，因为我们（内核）共有4个页表所以只需设置4项。	# ！！！！设置页表项
    # 页目录项的结构与页表中项的结构一样，4个字节为1项。参见上面113行下的说明。
    # 例如"$pg0+7"表示：0x00001007，是页目录表中的第1项。
    # 则第1个页表所在的地址 = 0x00001007 & 0xfffff000 = 0x1000；
    # 第1个页表的属性标志 = 0x00001007 & 0x00000fff = 0x07，表示该页存在、用户可读写。
	movl $pg0+7,pg_dir			/* set present bit/user r/w */ 		# pg_dir 在最开头， 0x7,页存在并且可读写
	movl $pg1+7,pg_dir+4		/*  --------- " " --------- */
	movl $pg2+7,pg_dir+8		/*  --------- " " --------- */
	movl $pg3+7,pg_dir+12		/*  --------- " " --------- */

    # 下面6行填写4个页表中所有项的内容，共有：4(页表)*1024(项/页表)=4096项(0 - 0xfff)，
    # 也即能映射物理内存 4096*4Kb = 16Mb。
    # 每项的内容是：当前项所映射的物理内存地址 + 该页的标志（这里均为7）。
    # 使用的方法是从最后一个页表的最后一项开始按倒退顺序填写。一个页表的最后一项在页表中的
    # 位置是1023*4 = 4092。因此最后一页的最后一项的位置就是$pg3+4092。
	movl $pg3+4092,%edi		# edi -> 最后一页的最后一项。
	movl $0xfff007,%eax		/*  16Mb - 4096 + 7 (r/w user,p) */
							# 最后1项对应物理内存页面的地址是0xfff000，加上属性标志7，即为0xfff007。
	std						# 方向位置位，edi值递减(4字节)。
1:	stosl			/* fill pages backwards - more efficient :-) */
	subl $0x1000,%eax		# 每填写好一项，物理地址值减0x1000（1页=4096KB）。 
	jge 1b					# 如果小于0则说明全添写好了。

	# 设置页目录表基址寄存器cr3的值，指向页目录表。cr3中保存的是页目录表的物理地址。
	xorl %eax,%eax		/* pg_dir is at 0x0000 */
	movl %eax,%cr3		/* cr3 - page directory start */

	# 设置启动使用分页处理（cr0的PG标志，位31），先读后写
	movl %cr0,%eax
	orl $0x80000000,%eax	# 添上PG标志。
	movl %eax,%cr0		/* set paging (PG) bit */
	ret			/* this also flushes prefetch-queue */
# 在改变分页处理标志后要求使用转移指令刷新预取指令队列，这里用的是返回指令ret。
# 该返回指令的另一个作用是将前面压入堆栈中的main程序的地址弹出，并跳转到/init/main.c
# 程序去运行。本程序到此就真正结束了。

# -----------------------------------------------------------------------------------

.align 2
.word 0
idt_descr:
	.word 256*8-1		# idt contains 256 entries # 共256项，限长=长度 - 1。
	.long idt
.align 2
.word 0

# 下面加载全局描述符表寄存器gdtr的指令lgdt要求的6字节操作数。前2字节是gdt表的限长，
# 后4字节是gdt表的线性基地址。这里全局表长度设置为2KB字节（0x7ff即可），因为每8字节
# 组成一个描述符项，所以表中共可有256项。符号_gdt是全局表在本程序中的偏移位置，见234行。
gdt_descr:
	.word 256*8-1		# so does gdt (not that that's any
	.long gdt		# magic number, but it works for me :^)

	.align 8				# 按8（2^3）字节方式对齐内存地址边界。(0.12里面这里是3，应该是这里错了？)
idt:	.fill 256,8,0		# idt is uninitialized				 # 256项，每项8字节，填0。

# 全局表。前4项分别是空项（不用）、代码段描述符、数据段描述符、系统调用段描述符，其中
# 系统调用段描述符并没有派用处，Linus当时可能曾想把系统调用代码专门放在这个独立的段中。
# 后面还预留了252项的空间，用于放置所创建任务的局部描述符(LDT)和对应的任务状态段TSS
# 的描述符。
# (0-nul, 1-cs, 2-ds, 3-syscall, 4-TSS0, 5-LDT0, 6-TSS1, 7-LDT1, 8-TSS2 etc...)
gdt:	.quad 0x0000000000000000	/* NULL descriptor */
	.quad 0x00c09a0000000fff	/* 16Mb */
	.quad 0x00c0920000000fff	/* 16Mb */
	.quad 0x0000000000000000	/* TEMPORARY - don't use */
	.fill 252,8,0			/* space for LDT's and TSS's etc */
