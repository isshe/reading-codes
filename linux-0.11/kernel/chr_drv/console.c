/*
 *  linux/kernel/console.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *	console.c
 *
 * This module implements the console io functions
 *	'void con_init(void)'
 *	'void con_write(struct tty_queue * queue)'
 * Hopefully this will be a rather complete VT102 implementation.
 *
 * Beeping thanks to John T Kohl.
 */

/*
 *  NOTE!!! We sometimes disable and enable interrupts for a short while
 * (to put a word in video IO), but this will work even for keyboard
 * interrupts. We know interrupts aren't enabled when getting a keyboard
 * interrupt, as we use trap-gates. Hopefully all is well.
 */

/*
 * Code to check for different video-cards mostly by Galen Hunt,
 * <g-hunt@ee.utah.edu>
 */

#include <linux/sched.h>
#include <linux/tty.h>
#include <asm/io.h>
#include <asm/system.h>

/*
 * These are set up by the setup-routine at boot-time:
 */

#define ORIG_X			(*(unsigned char *)0x90000)
#define ORIG_Y			(*(unsigned char *)0x90001)
#define ORIG_VIDEO_PAGE		(*(unsigned short *)0x90004)
#define ORIG_VIDEO_MODE		((*(unsigned short *)0x90006) & 0xff)
#define ORIG_VIDEO_COLS 	(((*(unsigned short *)0x90006) & 0xff00) >> 8)
#define ORIG_VIDEO_LINES	(25)
#define ORIG_VIDEO_EGA_AX	(*(unsigned short *)0x90008)
#define ORIG_VIDEO_EGA_BX	(*(unsigned short *)0x9000a)
#define ORIG_VIDEO_EGA_CX	(*(unsigned short *)0x9000c)

#define VIDEO_TYPE_MDA		0x10	/* Monochrome Text Display	*/
#define VIDEO_TYPE_CGA		0x11	/* CGA Display 			*/
#define VIDEO_TYPE_EGAM		0x20	/* EGA/VGA in Monochrome Mode	*/
#define VIDEO_TYPE_EGAC		0x21	/* EGA/VGA in Color Mode	*/

#define NPAR 16

extern void keyboard_interrupt(void);

static unsigned char	video_type;		/* Type of display being used	*/
static unsigned long	video_num_columns;	/* Number of text columns	*/
static unsigned long	video_size_row;		/* Bytes per row		*/
static unsigned long	video_num_lines;	/* Number of test lines		*/
static unsigned char	video_page;		/* Initial video page		*/
static unsigned long	video_mem_start;	/* Start of video RAM		*/
static unsigned long	video_mem_end;		/* End of video RAM (sort of)	*/
static unsigned short	video_port_reg;		/* Video register select port	*/
static unsigned short	video_port_val;		/* Video register value port	*/
static unsigned short	video_erase_char;	/* Char+Attrib to erase with	*/

static unsigned long	origin;		/* Used for EGA/VGA fast scroll	*/
static unsigned long	scr_end;	/* Used for EGA/VGA fast scroll	*/
static unsigned long	pos;
static unsigned long	x,y;
static unsigned long	top,bottom;
static unsigned long	state=0;
static unsigned long	npar,par[NPAR];
static unsigned long	ques=0;
static unsigned char	attr=0x07;

static void sysbeep(void);

/*
 * this is what the terminal answers to a ESC-Z or csi0c
 * query (= vt100 response).
 */
#define RESPONSE "\033[?1;2c"

/* NOTE! gotoxy thinks x==video_num_columns is ok */
/* 注意！gotoxy函数认为 x==video_num_columns 时是正确的 */
static inline void gotoxy(unsigned int new_x,unsigned int new_y)
{
	if (new_x > video_num_columns || new_y >= video_num_lines)
		return;
	x=new_x;											// 列
	y=new_y;											// 行
	pos=origin + y*video_size_row + (x<<1);				// 1列用2个字节表示，所以x<<1。
}

static inline void set_origin(void)
{
	cli();
	outb_p(12, video_port_reg);
	outb_p(0xff&((origin-video_mem_start)>>9), video_port_val);
	outb_p(13, video_port_reg);
	outb_p(0xff&((origin-video_mem_start)>>1), video_port_val);
	sti();
}

static void scrup(void)
{
	if (video_type == VIDEO_TYPE_EGAC || video_type == VIDEO_TYPE_EGAM)
	{
		if (!top && bottom == video_num_lines) {
			origin += video_size_row;
			pos += video_size_row;
			scr_end += video_size_row;
			if (scr_end > video_mem_end) {
				__asm__("cld\n\t"
					"rep\n\t"
					"movsl\n\t"
					"movl video_num_columns,%1\n\t"
					"rep\n\t"
					"stosw"
					::"a" (video_erase_char),
					"c" ((video_num_lines-1)*video_num_columns>>1),
					"D" (video_mem_start),
					"S" (origin)
					);
				scr_end -= origin-video_mem_start;
				pos -= origin-video_mem_start;
				origin = video_mem_start;
			} else {
				__asm__("cld\n\t"
					"rep\n\t"
					"stosw"
					::"a" (video_erase_char),
					"c" (video_num_columns),
					"D" (scr_end-video_size_row)
					);
			}
			set_origin();
		} else {
			__asm__("cld\n\t"
				"rep\n\t"
				"movsl\n\t"
				"movl video_num_columns,%%ecx\n\t"
				"rep\n\t"
				"stosw"
				::"a" (video_erase_char),
				"c" ((bottom-top-1)*video_num_columns>>1),
				"D" (origin+video_size_row*top),
				"S" (origin+video_size_row*(top+1))
				);
		}
	}
	else		/* Not EGA/VGA */
	{
		__asm__("cld\n\t"
			"rep\n\t"
			"movsl\n\t"
			"movl video_num_columns,%%ecx\n\t"
			"rep\n\t"
			"stosw"
			::"a" (video_erase_char),
			"c" ((bottom-top-1)*video_num_columns>>1),
			"D" (origin+video_size_row*top),
			"S" (origin+video_size_row*(top+1))
			);
	}
}

static void scrdown(void)
{
	if (video_type == VIDEO_TYPE_EGAC || video_type == VIDEO_TYPE_EGAM)
	{
		__asm__("std\n\t"
			"rep\n\t"
			"movsl\n\t"
			"addl $2,%%edi\n\t"	/* %edi has been decremented by 4 */
			"movl video_num_columns,%%ecx\n\t"
			"rep\n\t"
			"stosw"
			::"a" (video_erase_char),
			"c" ((bottom-top-1)*video_num_columns>>1),
			"D" (origin+video_size_row*bottom-4),
			"S" (origin+video_size_row*(bottom-1)-4)
			);
	}
	else		/* Not EGA/VGA */
	{
		__asm__("std\n\t"
			"rep\n\t"
			"movsl\n\t"
			"addl $2,%%edi\n\t"	/* %edi has been decremented by 4 */
			"movl video_num_columns,%%ecx\n\t"
			"rep\n\t"
			"stosw"
			::"a" (video_erase_char),
			"c" ((bottom-top-1)*video_num_columns>>1),
			"D" (origin+video_size_row*bottom-4),
			"S" (origin+video_size_row*(bottom-1)-4)
			);
	}
}

static void lf(void)
{
	if (y+1<bottom) {
		y++;
		pos += video_size_row;
		return;
	}
	scrup();
}

static void ri(void)
{
	if (y>top) {
		y--;
		pos -= video_size_row;
		return;
	}
	scrdown();
}

static void cr(void)
{
	pos -= x<<1;
	x=0;
}

static void del(void)
{
	if (x) {
		pos -= 2;
		x--;
		*(unsigned short *)pos = video_erase_char;
	}
}

static void csi_J(int par)
{
	long count;
	long start;

	switch (par) {
		case 0:	/* erase from cursor to end of display */
			count = (scr_end-pos)>>1;
			start = pos;
			break;
		case 1:	/* erase from start to cursor */
			count = (pos-origin)>>1;
			start = origin;
			break;
		case 2: /* erase whole display */
			count = video_num_columns * video_num_lines;
			start = origin;
			break;
		default:
			return;
	}
	__asm__("cld\n\t"
		"rep\n\t"
		"stosw\n\t"
		::"c" (count),
		"D" (start),"a" (video_erase_char)
		);
}

static void csi_K(int par)
{
	long count;
	long start;

	switch (par) {
		case 0:	/* erase from cursor to end of line */
			if (x>=video_num_columns)
				return;
			count = video_num_columns-x;
			start = pos;
			break;
		case 1:	/* erase from start of line to cursor */
			start = pos - (x<<1);
			count = (x<video_num_columns)?x:video_num_columns;
			break;
		case 2: /* erase whole line */
			start = pos - (x<<1);
			count = video_num_columns;
			break;
		default:
			return;
	}
	__asm__("cld\n\t"
		"rep\n\t"
		"stosw\n\t"
		::"c" (count),
		"D" (start),"a" (video_erase_char)
		);
}

void csi_m(void)
{
	int i;

	for (i=0;i<=npar;i++)
		switch (par[i]) {
			case 0:attr=0x07;break;
			case 1:attr=0x0f;break;
			case 4:attr=0x0f;break;
			case 7:attr=0x70;break;
			case 27:attr=0x07;break;
		}
}

static inline void set_cursor(void)
{
	cli();
	outb_p(14, video_port_reg);
	outb_p(0xff&((pos-video_mem_start)>>9), video_port_val);
	outb_p(15, video_port_reg);
	outb_p(0xff&((pos-video_mem_start)>>1), video_port_val);
	sti();
}

static void respond(struct tty_struct * tty)
{
	char * p = RESPONSE;

	cli();
	while (*p) {
		PUTCH(*p,tty->read_q);
		p++;
	}
	sti();
	copy_to_cooked(tty);
}

static void insert_char(void)
{
	int i=x;
	unsigned short tmp, old = video_erase_char;
	unsigned short * p = (unsigned short *) pos;

	while (i++<video_num_columns) {
		tmp=*p;
		*p=old;
		old=tmp;
		p++;
	}
}

static void insert_line(void)
{
	int oldtop,oldbottom;

	oldtop=top;
	oldbottom=bottom;
	top=y;
	bottom = video_num_lines;
	scrdown();
	top=oldtop;
	bottom=oldbottom;
}

static void delete_char(void)
{
	int i;
	unsigned short * p = (unsigned short *) pos;

	if (x>=video_num_columns)
		return;
	i = x;
	while (++i < video_num_columns) {
		*p = *(p+1);
		p++;
	}
	*p = video_erase_char;
}

static void delete_line(void)
{
	int oldtop,oldbottom;

	oldtop=top;
	oldbottom=bottom;
	top=y;
	bottom = video_num_lines;
	scrup();
	top=oldtop;
	bottom=oldbottom;
}

static void csi_at(unsigned int nr)
{
	if (nr > video_num_columns)
		nr = video_num_columns;
	else if (!nr)
		nr = 1;
	while (nr--)
		insert_char();
}

static void csi_L(unsigned int nr)
{
	if (nr > video_num_lines)
		nr = video_num_lines;
	else if (!nr)
		nr = 1;
	while (nr--)
		insert_line();
}

static void csi_P(unsigned int nr)
{
	if (nr > video_num_columns)
		nr = video_num_columns;
	else if (!nr)
		nr = 1;
	while (nr--)
		delete_char();
}

static void csi_M(unsigned int nr)
{
	if (nr > video_num_lines)
		nr = video_num_lines;
	else if (!nr)
		nr=1;
	while (nr--)
		delete_line();
}

static int saved_x=0;
static int saved_y=0;

static void save_cur(void)
{
	saved_x=x;
	saved_y=y;
}

static void restore_cur(void)
{
	gotoxy(saved_x, saved_y);
}

void con_write(struct tty_struct * tty)
{
	int nr;
	char c;

	nr = CHARS(tty->write_q);
	while (nr--) {
		GETCH(tty->write_q,c);
		switch(state) {
			case 0:
				if (c>31 && c<127) {
					if (x>=video_num_columns) {
						x -= video_num_columns;
						pos -= video_size_row;
						lf();
					}
					__asm__("movb attr,%%ah\n\t"
						"movw %%ax,%1\n\t"
						::"a" (c),"m" (*(short *)pos)
						);
					pos += 2;
					x++;
				} else if (c==27)
					state=1;
				else if (c==10 || c==11 || c==12)
					lf();
				else if (c==13)
					cr();
				else if (c==ERASE_CHAR(tty))
					del();
				else if (c==8) {
					if (x) {
						x--;
						pos -= 2;
					}
				} else if (c==9) {
					c=8-(x&7);
					x += c;
					pos += c<<1;
					if (x>video_num_columns) {
						x -= video_num_columns;
						pos -= video_size_row;
						lf();
					}
					c=9;
				} else if (c==7)
					sysbeep();
				break;
			case 1:
				state=0;
				if (c=='[')
					state=2;
				else if (c=='E')
					gotoxy(0,y+1);
				else if (c=='M')
					ri();
				else if (c=='D')
					lf();
				else if (c=='Z')
					respond(tty);
				else if (x=='7')
					save_cur();
				else if (x=='8')
					restore_cur();
				break;
			case 2:
				for(npar=0;npar<NPAR;npar++)
					par[npar]=0;
				npar=0;
				state=3;
				if ((ques=(c=='?')))
					break;
			case 3:
				if (c==';' && npar<NPAR-1) {
					npar++;
					break;
				} else if (c>='0' && c<='9') {
					par[npar]=10*par[npar]+c-'0';
					break;
				} else state=4;
			case 4:
				state=0;
				switch(c) {
					case 'G': case '`':
						if (par[0]) par[0]--;
						gotoxy(par[0],y);
						break;
					case 'A':
						if (!par[0]) par[0]++;
						gotoxy(x,y-par[0]);
						break;
					case 'B': case 'e':
						if (!par[0]) par[0]++;
						gotoxy(x,y+par[0]);
						break;
					case 'C': case 'a':
						if (!par[0]) par[0]++;
						gotoxy(x+par[0],y);
						break;
					case 'D':
						if (!par[0]) par[0]++;
						gotoxy(x-par[0],y);
						break;
					case 'E':
						if (!par[0]) par[0]++;
						gotoxy(0,y+par[0]);
						break;
					case 'F':
						if (!par[0]) par[0]++;
						gotoxy(0,y-par[0]);
						break;
					case 'd':
						if (par[0]) par[0]--;
						gotoxy(x,par[0]);
						break;
					case 'H': case 'f':
						if (par[0]) par[0]--;
						if (par[1]) par[1]--;
						gotoxy(par[1],par[0]);
						break;
					case 'J':
						csi_J(par[0]);
						break;
					case 'K':
						csi_K(par[0]);
						break;
					case 'L':
						csi_L(par[0]);
						break;
					case 'M':
						csi_M(par[0]);
						break;
					case 'P':
						csi_P(par[0]);
						break;
					case '@':
						csi_at(par[0]);
						break;
					case 'm':
						csi_m();
						break;
					case 'r':
						if (par[0]) par[0]--;
						if (!par[1]) par[1] = video_num_lines;
						if (par[0] < par[1] &&
						    par[1] <= video_num_lines) {
							top=par[0];
							bottom=par[1];
						}
						break;
					case 's':
						save_cur();
						break;
					case 'u':
						restore_cur();
						break;
				}
		}
	}
	set_cursor();
}

/*
 *  void con_init(void);
 *
 * This routine initalizes console interrupts, and does nothing
 * else. If you want the screen to clear, call tty_write with
 * the appropriate escape-sequece.
 *
 * Reads the information preserved by setup.s to determine the current display
 * type and sets everything accordingly.
 */

/*
* void con_init(void);
*
* 这个子程序初始化控制台中断，其他什么都不做。如果你想让屏幕干净的话，就使用
* 适当的转义字符序列调用tty_write()函数。
*
* 读取setup.s程序保存的信息，用以确定当前显示器类型，并且设置所有相关参数。
*/
void con_init(void)
{
	register unsigned char a;
	char *display_desc = "????";
	char *display_ptr;

	// 首先根据setup.s程序取得的系统硬件参数（见本程序第60--68行）初始化几个本函数专用
	// 的静态全局变量。
	video_num_columns = ORIG_VIDEO_COLS;			// 显示器显示字符列数。
	video_size_row = video_num_columns * 2;			// 每行字符需使用的字节数。
	video_num_lines = ORIG_VIDEO_LINES;				// 显示器显示字符行数。
	video_page = ORIG_VIDEO_PAGE;					// 当前显示页面。
	video_erase_char = 0x0720;						// 擦除字符（0x20是字符，0x07属性）。
	
	// 然后根据显示模式是单色还是彩色分别设置所使用的 显示内存起始位置 以及 显示寄存器索引
	// 端口号和显示寄存器数据端口号。如果获得的BIOS显示方式等于7，则表示是单色显示卡。
	if (ORIG_VIDEO_MODE == 7)			/* Is this a monochrome display? */ 		// 单色显示
	{
		video_mem_start = 0xb0000;					// 设置单显映像内存起始地址。
		video_port_reg = 0x3b4;						// 设置单显索引寄存器端口。
		video_port_val = 0x3b5;						// 设置单显数据寄存器端口。

		// 哪里调了中断？？？？？？？
		// 接着我们根据BIOS中断int 0x10功能0x12获得的显示模式信息，判断显示卡是单色显示卡还是彩色显示卡。
		//若使用上述中断功能所得到的BX寄存器返回值不等于0x10，则说明是EGA卡。因此初始显示类型为 EGA单色。
		//虽然 EGA 卡上有较多显示内存，但在单色方式下最多只 能利用地址范围在0xb0000--0xb8000之间的显示内存。
		// 然后置显示器描述字符串为'EGAm'。 并会在系统初始化期间显示器描述字符串将显示在屏幕的右上角。
		// 注意，这里使用了bx在调用中断int 0x10前后是否被改变的方法来判断卡的类型。若BL在
		// 中断调用后值被改变，表示显示卡支持Ah=12h功能调用，是EGA或后推出来的VGA等类型的
		// 显示卡。若中断调用返回值未变，表示显示卡不支持这个功能，则说明是一般单色显示卡。
		if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10)
		{
			video_type = VIDEO_TYPE_EGAM;			// 设置显示类型（EGA单色）。
			video_mem_end = 0xb8000;				// 0xb0000--0xb8000
			display_desc = "EGAm";					// 设置显示描述字符串。
		}
		else
		{
			video_type = VIDEO_TYPE_MDA;			// 设置显示类型(MDA单色)。
			video_mem_end	= 0xb2000;				// 0xb0000--0xb2000
			display_desc = "*MDA";
		}
	}
	else								/* If not, it is color. */
	{
		// 如果显示方式不为7，说明是彩色显示卡。此时文本方式下所用显示内存起始地址为0xb8000；
		// 显示控制索引寄存器端口地址为 0x3d4；数据寄存器端口地址为 0x3d5。
		video_mem_start = 0xb8000;
		video_port_reg	= 0x3d4;
		video_port_val	= 0x3d5;

		// 再判断显示卡类别。如果BX不等于0x10，则说明是EGA显示卡，此时共有32KB显示内存可用（0xb8000-0xc0000）。
		// 否则说明是CGA显示卡，只能使用8KB显示内存（0xb8000-0xba000）。
		if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10)
		{
			video_type = VIDEO_TYPE_EGAC;
			video_mem_end = 0xbc000;
			display_desc = "EGAc";
		}
		else
		{
			video_type = VIDEO_TYPE_CGA;
			video_mem_end = 0xba000;
			display_desc = "*CGA";
		}
	}

	// 然后我们在屏幕的右上角显示描述字符串。采用的方法是直接将字符串写到显示内存的相应
	// 位置处。首先将显示指针display_ptr指到屏幕第1行右端差4个字符处（每个字符需2个
	// 字节，因此减8），然后循环复制字符串的字符，并且每复制1个字符都空开1个属性字节。
	display_ptr = ((char *)video_mem_start) + video_size_row - 8; 		// 8字节
	while (*display_desc)
	{
		*display_ptr++ = *display_desc++;
		display_ptr++;
	}
	
	/* Initialize the variables used for scrolling (mostly EGA/VGA)	*/
	/* 初始化用于滚屏的变量（主要用于EGA/VGA）*/
	// 注意，此时当前虚拟控制台号currcons已被初始化位0。因此下面实际上是初始化0号虚拟控
	// 制台的结构vc_cons[0]中的所有字段值。例如，这里符号origin在前面第115行上已被定义为
	// vc_cons[0].vc_origin。下面首先设置0号控制台的默认滚屏开始内存位置 video_mem_start
	// 和默认滚屏末行内存位置，实际上它们也就是0号控制台占用的部分显示内存区域。然后初始
	// 设置0号虚拟控制台的其他属性和标志值。
	origin	= video_mem_start;												// 快速滚屏操作起始内存位置。
	scr_end	= video_mem_start + video_num_lines * video_size_row; 			// 快速滚屏操作末端内存位置。
	top	= 0;																// 初始设置滚动时顶行行号和底行行号。
	bottom	= video_num_lines;

	// 在设置了0号控制台当前光标所在位置和光标对应的内存位置pos后，我们循环设置其余的几
	// 个虚拟控制台结构的参数值。除了各自占用的显示内存开始和结束位置不同，它们的初始值基
	// 本上都与0号控制台相同。
	gotoxy(ORIG_X,ORIG_Y);
	set_trap_gate(0x21,&keyboard_interrupt);			// 参见system.h，第36行开始。
	outb_p(inb_p(0x21)&0xfd,0x21);					 	// 取消对键盘中断的屏蔽，允许IRQ1。
	a=inb_p(0x61);										// 读取键盘端口0x61（8255A端口PB）。
	outb_p(a|0x80,0x61);								// 设置禁止键盘工作（位7置位），
	outb(a,0x61);										// 再允许键盘工作，**复位键盘**。
}
/* from bsd-net-2: */

void sysbeepstop(void)
{
	/* disable counter 2 */
	outb(inb_p(0x61)&0xFC, 0x61);
}

int beepcount = 0;

static void sysbeep(void)
{
	/* enable counter 2 */
	outb_p(inb_p(0x61)|3, 0x61);
	/* set command for counter 2, 2 byte write */
	outb_p(0xB6, 0x43);
	/* send 0x637 for 750 HZ */
	outb_p(0x37, 0x42);
	outb(0x06, 0x42);
	/* 1/8 second */
	beepcount = HZ/8;	
}
