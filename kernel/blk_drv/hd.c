/*
 *  linux/kernel/hd.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * This is the low-level hd interrupt support. It traverses the
 * request-list, using interrupts to jump between functions. As
 * all the functions are called within interrupts, we may not
 * sleep. Special care is recommended.
 * 
 *  modified by Drew Eckhardt to check nr of hd's from the CMOS.
 */
/* 本程序是底层硬盘中断辅助程序。主要用于扫描请求项队列，使用中断 
*在函数之间跳转。由于所有的函数都是在中断里调用的，所以这些函数
*不可以睡眠。请特别注意。
*
*由Drew Eckhardt修改，利用CMOS信息检测硬盘数。
*/

#include <linux/config.h>		/* 内核配置头文件。定义键盘语言和硬盘类型（HD_TYPE）可选项。	*/
#include <linux/sched.h>		/* 调度程序头文件，定义了任务结构task_struct、初始任务0 的数据，	*/
								/* 还有一些有关描述符参数设置和获取的嵌入式汇编函数宏语句。	*/
#include <linux/fs.h>			/* 文件系统头文件。定义文件表结构（file,buffer_head,m_inode 等）。	*/
#include <linux/kernel.h>		/* 内核头文件。含有一些内核常用函数的原形定义。	*/
#include <linux/hdreg.h>		/* 硬盘参数头文件。定义访问硬盘寄存器端口，状态码，分区表等信息。	*/
#include <asm/system.h>			/* 系统头文件。定义了设置或修改描述符/中断门等的嵌入式汇编宏。	*/
#include <asm/io.h>				/* io 头文件。定义硬件端口输入/输出宏汇编语句。	*/
#include <asm/segment.h>		/* 段操作头文件。定义了有关段寄存器操作的嵌入式汇编函数。	*/

#define MAJOR_NR 3				/* 硬盘主设备号是3。	*/
#include "blk.h"				/* 块设备头文件。定义请求数据结构、块设备数据结构和宏函数等信息。	*/

/* 读CMOS 参数宏函数。	*/
/* 这段宏读取CMOS中硬盘信息。outb—p、inb—p是include/asm/io.h中定义的端口输入输出宏。
与init/main.c中读取CMOS时钟信息的宏完全一样。	*/
#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})
/* 0x70 是写端口号，0x80|addr 是要读的 CMOS 内存地址。
*  0x71 是读端口号。
*/
/* Max read/write errors/sector */
/* 每扇区读/写操作允许的最多出错次数	*/
#define MAX_ERRORS 7			/* 读/写一个扇区时允许的最多出错次数。	*/
#define MAX_HD 2				/* 系统支持的最多硬盘数。	*/

/* 重新校正处理函数。
* 复位操作时在硬盘中断处理程序中调用的重新校正函数(287行)。*/
static void recal_intr (void);

static int recalibrate = 1;	/* 重新校正标志。当设置了该标志，程序中会调用recal—intrO以将磁头移动到0柱面。	*/
static int reset = 1;		/* 复位标志。当发生读写错误时会设置该标志并调用相关复位函数，以复位硬盘和控制器。	*/

/*
 *  This struct defines the HD's and their types.
 */
/* 下面结构定义了硬盘参数及类型
* 硬盘信息结构（Harddisk information struct）。
* 各字段分别是磁头数、每磁道扇区数、柱面数、写前预补偿柱面号、磁头着陆区柱面号、
* 控制字节。它们的含义请参见程序列表后的说明。
*/
struct hd_i_struct
{
	int head,sect,cyl,wpcom,lzone,ctl;
};
/* 如果已经在include/linux/config.h配置文件中定义了符号常数HD_TYPE，就取其中定义
* 好的参数作为硬盘信息数组hd_info[]中的数据。否则先默认都设为0值，在setup()函数
* 中会重新进行设置。
*/
#ifdef HD_TYPE
struct hd_i_struct hd_info[] = { HD_TYPE };	/* 硬盘信息数组。	*/
#define NR_HD ((sizeof (hd_info))/(sizeof (struct hd_i_struct)))/* 计算硬盘数。	*/
#else										/* 否则，都设为0 值。	*/
struct hd_i_struct hd_info[] = { {0,0,0,0,0,0},{0,0,0,0,0,0} };
static int NR_HD = 0;
#endif

/* 定义硬盘分区结构。给出每个分区从硬盘0道开始算起的物理起始扇区号和分区扇区总数。
 * 其中5的倍数处的项（例如hd[0]和hd[5]等）代表整个硬盘的参数。
 */
static struct hd_struct
{
	long start_sect;		/* 分区在硬盘中的起始物理（绝对）扇区。	*/
	long nr_sects;			/* 分区中扇区总数。	*/
} hd[5*MAX_HD]={{0,0},};

/* 读端口嵌入汇编宏。读端口 port，共读nr字，保存在buf中。	*/
#define port_read(port,buf,nr) \
__asm__("cld;rep;insw"::"d" (port),"D" (buf),"c" (nr):"cx","di")

/* 写端口嵌入汇编宏。写端口 port，共写nr字，从buf中取数据。	*/
#define port_write(port,buf,nr) \
__asm__("cld;rep;outsw"::"d" (port),"S" (buf),"c" (nr):"cx","si")

extern void hd_interrupt(void);	/* 硬盘中断过程（system_call.s，221 行）。	*/
extern void rd_load(void);		/* 虚拟盘创建加载函数（ramdisk.c，71 行）。	*/

/* This may be used only once, enforced by 'static int callable'
* 下面该函数只在初始化时被调用一次。用静态变量callable作为可调用标志。*/
/*系统设置函数。
函数参数BIOS是由初始化程序init/main.c中init子程序设置为指向硬盘参数表结构的指针。
该硬盘参数表结构包含2个硬盘参数表的内容（共32字节），是从内存0x90080处复制而来。
0x90080处的硬盘参数表是由setup.s程序利用ROM BIOS功能取得。硬盘参数表信息参见程序
列表后的说明。本函数主要功能是读取CMOS和硬盘参数表信息，用于设置硬盘分区结构hd，
并尝试加载RAM虚拟盘和根文件系统。
*/
int sys_setup(void * BIOS)
{
	static int callable = 1;	/* 限制本函数只能被调用1次的标志。	*/
	int i,drive;
	unsigned char cmos_disks;
	struct partition *p;
	struct buffer_head * bh;

/* 首先设置callable标志，使得本函数只能被调用1次。然后设置硬盘信息数组hd—info[]。
如果在include/linux/config.h文件中已定义了符号常数HD_TYPE，那么hd_info[]数组
已经在前面第49行上设置好了。否则就需要读取boot/setup.s程序存放在内存0x90080处
开始的硬盘参数表。setup.s程序在内存此处连续存放着一到两个硬盘参数表。
*/
	if (!callable)
		return -1;
	callable = 0;

#ifndef HD_TYPE
	for (drive=0 ; drive<2 ; drive++) {
		hd_info[drive].cyl = *(unsigned short *) BIOS;			/* 如果没有在config.h 中定义硬盘参数，就从0x90080 处读入。	*/

		hd_info[drive].head = *(unsigned char *) (2+BIOS);		/* 磁头数。	*/
		hd_info[drive].wpcom = *(unsigned short *) (5+BIOS);	/* 写前预补偿柱面号。	*/
		hd_info[drive].ctl = *(unsigned char *) (8+BIOS);		/* 控制字节。	*/
		hd_info[drive].lzone = *(unsigned short *) (12+BIOS);	/* 磁头着陆区柱面号。	*/
		hd_info[drive].sect = *(unsigned char *) (14+BIOS);		/* 每磁道扇区数。	*/
		BIOS += 16;		/* 每个硬盘的参数表长16 字节，这里BIOS 指向下一个表。	*/
	}

/* setup.s 程序在取BIOS 中的硬盘参数表信息时，如果只有1 个硬盘，就会将对应第2 个硬盘的
 * 16 字节全部清零。因此这里只要判断第2 个硬盘柱面数是否为0 就可以知道有没有第2 个硬盘了。 */
	if (hd_info[1].cyl)
		NR_HD=2;				/* 硬盘数置为2。	*/
	else
		NR_HD=1;
#endif
/* 到这里，硬盘信息数组hd—info□已经设置好，并且确定了系统含有的硬盘数NR—HD。现在
开始设置硬盘分区结构数组hd□。该数组的项0和项5分别表示两个硬盘的整体参数，而
项1一4和6—9分别表示两个硬盘的4个分区的参数。因此这里仅设置表示硬盘整体信息
的两项（项0和5）。	*/
	for (i=0 ; i<NR_HD ; i++) {
		hd[i*5].start_sect = 0;					/* 硬盘起始扇区号。	*/
		hd[i*5].nr_sects = hd_info[i].head*
				hd_info[i].sect*hd_info[i].cyl;	/* 硬盘总扇区数。	*/
	}

/*	We querry CMOS about hard disks : it could be that 
	we have a SCSI/ESDI/etc controller that is BIOS
	compatable with ST-506, and thus showing up in our
	BIOS table, but not register compatable, and therefore
	not present in CMOS.

	Furthurmore, we will assume that our ST-506 drives
	<if any> are the primary drives in the system, and 
	the ones reflected as drive 1 or 2.

	The first drive is stored in the high nibble of CMOS
	byte 0x12, the second in the low nibble.  This will be
	either a 4 bit drive type or 0xf indicating use byte 0x19 
	for an 8 bit type, drive 1, 0x1a for drive 2 in CMOS.

	Needless to say, a non-zero value means we have 
	an AT controller hard disk for that drive.
*/
/*
* 我们对CMOS 有关硬盘的信息有些怀疑：可能会出现这样的情况，我们有一块SCSI/ESDI/等的
* 控制器，它是以ST-506 方式与BIOS 兼容的，因而会出现在我们的BIOS 参数表中，但却又不
* 是寄存器兼容的，因此这些参数在CMOS 中又不存在。
* 另外，我们假设ST-506 驱动器（如果有的话）是系统中的基本驱动器，也即以驱动器1 或2
* 出现的驱动器。
* 第1 个驱动器参数存放在CMOS 字节0x12 的高半字节中，第2 个存放在低半字节中。该4 位字节
* 信息可以是驱动器类型，也可能仅是0xf。0xf 表示使用CMOS 中0x19 字节作为驱动器1 的8 位
* 类型字节，使用CMOS 中0x1A 字节作为驱动器2 的类型字节。
* 总之，一个非零值意味着我们有一个AT 控制器硬盘兼容的驱动器。
*/

/* 这里根据上述原理，下面代码用来检测硬盘到底是不是AT控制器兼容的。有关CMOS信息
请参见第4章中4.2.3.1节。这里从CMOS偏移地址0x12处读出硬盘类型字节。如果低半
字节值（存放着第2个硬盘类型值）不为0，则表示系统有两硬盘，否则表示系统只有1个硬盘。
如果0x12处读出的值为0，则表示系统中没有AT兼容硬盘。	*/

	if ((cmos_disks = CMOS_READ(0x12)) & 0xf0)
		if (cmos_disks & 0x0f)
			NR_HD = 2;
		else
			NR_HD = 1;
	else
		NR_HD = 0;
/* 若NR_HD=0，则两个硬盘都不是AT 控制器兼容的，硬盘数据结构清零。	*/
/* 若NR_HD=1，则将第2 个硬盘的参数清零。	*/
	for (i = NR_HD ; i < 2 ; i++) {
		hd[i*5].start_sect = 0;
		hd[i*5].nr_sects = 0;
	}
/* 好，到此为止我们已经真正确定了系统中所含的硬盘个数NR—HD。现在我们来读取每个硬盘
上第1个扇区中的分区表信息，用来设置分区结构数组hd[]中硬盘各分区的信息。首先利
用读块函数bread()读硬盘第1个数据块（fs/buffer.c,第267行），第1个参数（0x300、
0x305 ）分别是两个硬盘的设备号，第2个参数（0）是所需读取的块号。若读操作成功，
II则数据会被存放在缓冲块bh的数据区中。若缓冲块头指针bh为0，则说明读操作失败，则
显示出错信息并停机。否则我们根据硬盘第1个扇区最后两个字节应该是0xAA55来判断扇
区中数据的有效性，从而可以知道扇区中位于偏移OxlBE开始处的分区表是否有效。若有效
则将硬盘分区表信息放入硬盘分区结构数组hd□中。最后释放bh缓冲区。	*/

	for (drive=0 ; drive<NR_HD ; drive++) {
		if (!(bh = bread(0x300 + drive*5,0))) {	/* 0x300, 0x305 逻辑设备号。	*/
			printk("Unable to read partition table of drive %d\n\r",
				drive);
			panic("");
		}
		if (bh->b_data[510] != 0x55 || (unsigned char)
		    bh->b_data[511] != 0xAA) {			/* 判断硬盘信息有效标志'55AA'。	*/
			printk("Bad partition table on drive %d\n\r",drive);
			panic("");
		}
		p = 0x1BE + (void *)bh->b_data;			/* 分区表位于硬盘第1 扇区的0x1BE 处。	*/
		for (i=1;i<5;i++,p++) {
			hd[i+5*drive].start_sect = p->start_sect;
			hd[i+5*drive].nr_sects = p->nr_sects;
		}
		brelse(bh);			/* 释放为存放硬盘块而申请的内存缓冲区页。	*/
	}
/* 现在总算完成设置硬盘分区结构数组hd[]的任务。如果确实有硬盘存在并且已读入其分区
表，则显示“分区表正常”信息。然后尝试在系统内存虚拟盘中加载启动盘中包含的根文
件系统映像（blk—drv/ramdisk.c，第71行）。即在系统设置有虚拟盘的情况下判断启动盘
上是否还含有根文件系统的映像数据。如果有（此时该启动盘称为集成盘）则尝试把该映像
加载并存放到虚拟盘中，然后把此时的根文件系统设备号ROOT—DEV修改成虚拟盘的设备号。
最后安装根文件系统（fs/super.c，第242行）。	*/

	if (NR_HD)
		printk("Partition table%s ok.\n\r",(NR_HD>1)?"s":"");
	rd_load();				/* 尝试创建并加载虚拟盘。(kernel/blk_drv/ramdisk.c,71)。	*/
	mount_root();			/* 安装根文件系统(fs/super.c,242)。	*/
	return (0);
}

/* 判断并循环等待硬盘控制器就绪。
读硬盘控制器状态寄存器端口 HD_STATUS(0xlf7)，循环检测其中的驱动器就绪比特位（位6）
是否被置位并且控制器忙位（位7）是否被复位。如果返回值retries为0，则表示等待控制
器空闲的时间已经超时而发生错误，若返回值不为0则说明在等待（循环）时间期限内控制器
回到空闲状态，0K!
实际上，我们仅需检测状态寄存器忙位（位7）是否为1来判断控制器是否处于忙状态，驱动
器是否就绪（即位6是否为1）与控制器的状态无关。因此我们可以把第165行语句改写成：
 “while (--retries &&（inb—p(HD_STATUS)&0x80）);” 另外，由于现在的PC机
速度都很快，因此我们可以把等待的循环次数再加大一些，例如再增加10倍！
*/
static int controller_ready(void)
{
	int retries=10000;

	while (--retries && (inb_p(HD_STATUS)&0xc0)!=0x40);
	return (retries);		/* 返回等待循环的次数。	*/
}

/* 检测硬盘执行命令后的状态。(win_表示温切斯特硬盘的缩写)	*/
/* 读取状态寄存器中的命令执行结果状态。返回0 表示正常，1 出错。如果执行命令错，	*/
/* 则再读错误寄存器HD_ERROR(0x1f1)。	*/
static int win_result(void)
{
	int i=inb_p(HD_STATUS);		/* 取状态信息。	*/

	if ((i & (BUSY_STAT | READY_STAT | WRERR_STAT | SEEK_STAT | ERR_STAT))
		== (READY_STAT | SEEK_STAT))
		return(0); 				/* ok */
	if (i&1) i=inb(HD_ERROR);	/* 若ERR_STAT 置位，则读取错误寄存器。	*/
	return (1);
}

/* 向硬盘控制器发送命令块。
 * 参数：drive -硬盘号(0-1); 	nsect -读写扇区数；	sect -起始扇区；
 * 		head -磁头号； 		cyl -柱面号； 		cmd -命令码（见控制器命令列表）；
 * 		intr—addrO - 硬盘中断处理程序中将调用的C处理函数指针。
 * 该函数在硬盘控制器就绪之后，先设置全局函数指针变量do—hd指向硬盘中断处理程序中将会调用
 * 的C处理函数，然后再发送硬盘控制字节和7字节的参数命令块。硬盘中断处理程序的代码位于
 * kernel/system—call.s 程序第 221 行处。
 * 第184行定义了一个寄存器变量__res。该变量将被保存在一个寄存器中，以便于快速访问和操作。
 * 如果想指定寄存器（例如eax）,那么我们可以把该句写成“register char __res asm(“ax”)；”。
 */
static void hd_out(unsigned int drive,unsigned int nsect,unsigned int sect,
		unsigned int head,unsigned int cyl,unsigned int cmd,
		void (*intr_addr)(void))
{
	register int port asm("dx");				/* 定义局部寄存器变量并放在指定寄存器dx中。	*/
/* 首先对参数进行有效性检查。如果驱动器号大于1 (只能是0、1)或者磁头号大于15，则程
* 序不支持，停机。否则就判断并循环等待驱动器就绪。如果等待一段时间后仍未就绪则表示
* 硬盘控制器出错，也停机。
*/

	if (drive>1 || head>15)						/* 如果驱动器号(0,1)>1 或磁头号>15，则程序不支持。	*/
		panic("Trying to write bad sector");
	if (!controller_ready())					/* 如果等待一段时间后仍未就绪则出错，死机。	*/
		panic("HD controller not ready");
/* 接着我们设置断发生时将调用的C函数指针do—hd (该函数指针定义在blk.h文件的
第54—99行之间，请特别留意其中的第81行和97行)。然后在向硬盘控制器发送参数和
命令之前，规定要先向控制器命令端口（0x3f6）发送一指定硬盘的控制字节，以建立相应
的硬盘控制方式。该控制字节即是硬盘信息结构数组中的ctl字段。然后向控制器端口
0xlfl-0xlf7发送7字节的参数命令块。
*/

	do_hd = intr_addr;							/* do_hd 函数指针将在硬盘中断程序中被调用。	*/
	outb_p(hd_info[drive].ctl,HD_CMD);			/* 向控制寄存器(0x3f6)输出控制字节。	*/
	port=HD_DATA;								/* 置dx 为数据寄存器端口(0x1f0)。	*/
	outb_p(hd_info[drive].wpcom>>2,++port);		/* 参数：写预补偿柱面号(需除4)。	*/
	outb_p(nsect,++port);						/* 参数：读/写扇区总数。	*/
	outb_p(sect,++port);						/* 参数：起始扇区。	*/
	outb_p(cyl,++port);							/* 参数：柱面号低8 位。	*/
	outb_p(cyl>>8,++port);						/* 参数：柱面号高8 位。	*/
	outb_p(0xA0|(drive<<4)|head,++port);		/* 参数：驱动器号+磁头号。	*/
	outb(cmd,++port);							/* 命令：硬盘控制命令。	*/
}

/* 等待硬盘就绪。
该函数循环等待主状态控制器忙标志位复位。若仅有就绪或寻道结束标志置位，则表示硬盘
就绪，成功返回0。若经过一段时间仍为忙，则返回1。
*/
static int drive_busy(void)
{
	unsigned int i;
	
/* 循环读取控制器的主状态寄存器HD—STATUS，等待就绪标志位置位并且忙位复位。然后再次
读取主控制器状态字节，检测其中忙位、就绪位和寻道结束位。若仅有就绪或寻道结束标志
置位，则表示硬盘就绪，返回0。否则表示等待超时。于是警告显示信息。并返回1。
*/
	for (i = 0; i < 10000; i++)					/* 循环等待就绪标志位置位。	*/
		if (READY_STAT == (inb_p(HD_STATUS) & (BUSY_STAT|READY_STAT)))
			break;
	i = inb(HD_STATUS);							/* 再取主控制器状态字节。	*/
	i &= BUSY_STAT | READY_STAT | SEEK_STAT;	/* 检测忙位、就绪位和寻道结束位。	*/
	if (i == READY_STAT | SEEK_STAT)			/* 若仅有就绪或寻道结束标志，则返回0。	*/
		return(0);
	printk("HD controller times out\n\r");		/* 否则等待超时，显示信息。并返回1。	*/
	return(1);
}

/* 诊断复位（重新校正）硬盘控制器。	*/
static void reset_controller(void)
{
	int	i;

/* 首先向控制寄存器端口（0x3f6）发送允许复位（4）控制字节。然后循环空操作等待一段时
间让控制器执行复位操作。接着再向该端口发送正常的控制字节(不禁止重试、重读)，并等
待硬盘就绪。若等待硬盘就绪超时，则显示警告信息。然后读取错误寄存器内容，若其不等
于1(表示无错误)则显示硬盘控制器复位失败信息。
*/
	outb(4,HD_CMD);								/* 向控制寄存器端口发送控制字节(4-复位)。	*/
	for(i = 0; i < 100; i++) nop();				/* 等待一段时间（循环空操作）。	*/
	outb(hd_info[0].ctl & 0x0f ,HD_CMD);		/* 再发送正常的控制字节(不禁止重试、重读)。	*/
	if (drive_busy())							/* 若等待硬盘就绪超时，则显示出错信息。	*/
		printk("HD-controller still busy\n\r");
	if ((i = inb(HD_ERROR)) != 1)				/* 取错误寄存器，若不等于1（无错误）则出错。	*/
		printk("HD-controller reset failed: %02x\n\r",i);
}

/* 复位硬盘nr。首先复位（重新校正）硬盘控制器。然后发送硬盘控制器命令“建立驱动器参数”，	*/
/* 其中函数recal_intr()是在硬盘中断处理程序中调用的重新校正处理函数。	*/
static void reset_hd(int nr)
{
	reset_controller();
	hd_out(nr,hd_info[nr].sect,hd_info[nr].sect,hd_info[nr].head-1,
		hd_info[nr].cyl,WIN_SPECIFY,&recal_intr);
}

/* 意外硬盘中断调用函数。	*/
/* 发生意外硬盘中断时，硬盘中断处理程序中调用的默认C 处理函数。在被调用函数指针为空时	*/
/* 调用该函数。参见(kernel/system_call.s,241 行)。	*/
void unexpected_hd_interrupt(void)
{
	printk("Unexpected HD interrupt\n\r");
}

/* 如果读扇区时的出错次数大于或等于7次时，则结束当前请求项并唤醒等待该请求的进程，
而且对应缓冲区更新标志复位，表示数据没有更新。如果读一扇区时的出错次数已经大于
3次，则要求执行复位硬盘控制器操作（设置复位标志）。	*/
static void bad_rw_intr(void)
{
	if (++CURRENT->errors >= MAX_ERRORS)		/* 如果读扇区时的出错次数大于或等于7 次时，	*/
		end_request(0);							/* 则结束请求并唤醒等待该请求的进程，而且	*/
												/* 对应缓冲区更新标志复位（没有更新）。	*/
	if (CURRENT->errors > MAX_ERRORS/2)			/* 如果读一扇区时的出错次数已经大于3 次，	*/
		reset = 1;
}

/* 读操作中断调用函数。
该函数将在硬盘读命令结束时引发的硬盘中断过程中被调用。
在读命令执行后会产生硬盘中断信号，并执行硬盘中断处理程序，此时在硬盘中断处理程序
中调用的C函数指针do—hd已经指向read—intrO，因此会在一次读扇区操作完成（或出错）
后就会执行该函数。
*/
static void read_intr(void)
{
/* 该函数首先判断此次读命令操作是否出错。若命令结束后控制器还处于忙状态，或者命令
执行错误，则处理硬盘操作失败问题，接着再次请求硬盘作复位处理并执行其他请求项。
然后返回。每次读操作出错都会对当前请求项作出错次数累计，若出错次数不到最大允许
出错次数的一半，则会先执行硬盘复位操作，然后再执行本次请求项处理。若出错次数已
经大于等于最大允许出错次数MAX—ERR0RS(7次)，则结束本次请求项的处理而去处理队
列中下一个请求项。
*/
	if (win_result()) {							/* 若控制器忙、读写错或命令执行错，	*/
		bad_rw_intr();							/* 则进行读写硬盘失败处理	*/
		do_hd_request();						/* 然后再次请求硬盘作相应(复位)处理。	*/
		return;
	}
/* 如果读命令没有出错，则从数据寄存器端口把1个扇区的数据读到请求项的缓冲区中，并且
递减请求项所需读取的扇区数值。若递减后不等于0，表示本项请求还有数据没取完，于是
再次置中断调用C函数指针do_hd为read_intrO并直接返回，等待硬盘在读出另1个扇区
数据后发出中断并再次调用本函数。注意：257行语句中的256是指内存字，即512字节。
注意1:262行再次置do_hd指针指向read_intr()是因为硬盘中断处理程序每次调用do_hd
时都会将该函数指针置空。参见system_call.s程序第237—238行。
*/

	port_read(HD_DATA,CURRENT->buffer,256);		/* 将数据从数据寄存器口读到请求结构缓冲区。	*/
	CURRENT->errors = 0;						/* 清出错次数。	*/
	CURRENT->buffer += 512;						/* 调整缓冲区指针，指向新的空区。	*/
	CURRENT->sector++;							/* 起始扇区号加1，	*/
	if (--CURRENT->nr_sectors) {				/* 如果所需读出的扇区数还没有读完，则	*/
		do_hd = &read_intr;						/* 再次置硬盘调用C 函数指针为read_intr()	*/
		return;									/* 因为硬盘中断处理程序每次调用do_hd 时	*/
	}											/* 都会将该函数指针置空。参见system_call.s	*/
/* 执行到此，说明本次请求项的全部扇区数据已经读完，则调用end_requestO函数去处理请
求项结束事宜。最后再次调用do_hd_requestO，去处理其他硬盘请求项。执行其他硬盘请求操作。
*/
	end_request(1);								/* 若全部扇区数据已经读完，则处理请求结束事宜，	*/
	do_hd_request();							/* 执行其它硬盘请求操作。数据已更新标志置位（1）	*/
}

/* 写扇区中断调用函数。在硬盘中断处理程序中被调用。	*/
/* 该函数将在硬盘写命令结束时引发的硬盘中断过程中被调用。函数功能与read_intrO类似。
在写命令执行后会产生硬盘中断信号，并执行硬盘中断处理程序，此时在硬盘中断处理程序
中调用的C函数指针do_hd已经指向write_intrO，因此会在一次写扇区操作完成（或出错）
后就会执行该函数。	*/
static void write_intr(void)
{
/* 该函数首先判断此次写命令操作是否出错。若命令结束后控制器还处于忙状态，或者命令
执行错误，则处理硬盘操作失败问题，接着再次请求硬盘作复位处理并执行其他请求项。
然后返回。在bad_rw_intrO函数中，每次操作出错都会对当前请求项作出错次数累计，
若出错次数不到最大允许出错次数的一半，则会先执行硬盘复位操作，然后再执行本次请
求项处理。若出错次数已经大于等于最大允许出错次数MAX_ERR0RS(7次)，则结束本次
请求项的处理而去处理队列中下一个请求项。do_hd_request()中会根据当时具体的标志
状态来判别是否需要先执行复位、重新校正等操作，然后再继续或处理下一个请求项。
*/
	if (win_result()) {							/* 如果硬盘控制器返回错误信息，	*/
		bad_rw_intr();							/* 则首先进行硬盘读写失败处理，	*/
		do_hd_request();						/* 然后再次请求硬盘作相应(复位)处理，	*/
		return;									/* 然后返回（也退出了此次硬盘中断）。	*/
	}
/* 此时说明本次写一扇区操作成功，因此将欲写扇区数减1。若其不为0，则说明还有扇区
要写，于是把当前请求起始扇区号+1，并调整请求项数据缓冲区指针指向下一块欲写的
数据。然后再重置硬盘中断处理程序中调用的C函数指针do—hd (指向本函数)。接着向
控制器数据端口写入512字节数据，然后函数返回去等待控制器把这些数据写入硬盘后产
生的中断。
*/
	if (--CURRENT->nr_sectors) {				/* 否则将欲写扇区数减1，若还有扇区要写，则	*/
		CURRENT->sector++;						/* 当前请求起始扇区号+1，	*/
		CURRENT->buffer += 512;					/* 调整请求缓冲区指针，	*/
		do_hd = &write_intr;					/* 置硬盘中断程序调用函数指针为write_intr()，	*/
		port_write(HD_DATA,CURRENT->buffer,256);/* 再向数据寄存器端口写256 字节。	*/
		return;									/* 返回等待硬盘再次完成写操作后的中断处理。	*/
	}
/* 若本次请求项的全部扇区数据已经写完，则调用end_requestO函数去处理请求项结束事宜。
最后再次调用do_hd_requestO，去处理其他硬盘请求项。执行其他硬盘请求操作。
*/
	end_request(1);								/* 若全部扇区数据已经写完，则处理请求结束事宜，	*/
	do_hd_request();							/* 执行其它硬盘请求操作。	*/
}

/* 硬盘重新校正（复位）中断调用函数。
该函数会在硬盘执行重新校正操作而引发的硬盘中断中被调用。
如果硬盘控制器返回错误信息，则函数首先进行硬盘读写失败处理，然后请求硬盘作相应
(复位)处理。在bad—rw—intr()函数中，每次操作出错都会对当前请求项作出错次数
累计，若出错次数不到最大允许出错次数的一半，则会先执行硬盘复位操作，然后再执行
本次请求项处理。若出错次数已经大于等于最大允许出错次数MAX—ERR0RS(7次)，则结
束本次请求项的处理而去处理队列中下一个请求项。do—hd—request()中会根据当时具体
的标志状态来判别是否需要先执行复位、重新校正等操作，然后再继续或处理下一请求项。
*/
static void recal_intr(void)
{
	if (win_result())		/* 若返回出错，则调用 bad_rw_intr()。	*/
		bad_rw_intr();
	do_hd_request();
}

/* 执行硬盘读写请求操作。
该函数根据设备当前请求项中的设备号和起始扇区号信息首先计算得到对应硬盘上的柱面号、
当前磁道中扇区号、磁头号数据，然后再根据请求项中的命令（READ/WRITE）对硬盘发送相应
读/写命令。若控制器复位标志或硬盘重新校正标志已被置位，那么首先会去执行复位或重新
校正操作。
若请求项此时是块设备的第1个（原来设备空闲），则块设备当前请求项指针会直接指向该请
求项（参见ll—rw—blk.c，28行），并会立刻调用本函数执行读写操作。否则在一个读写操作
完成而引发的硬盘中断过程中，若还有请求项需要处理，则也会在硬盘中断过程中调用本函数。
参见 kernel/system—call.s，221 行。
*/
void do_hd_request(void)
{
	int i,r;
	unsigned int block,dev;
	unsigned int sec,head,cyl;
	unsigned int nsect;

/* 函数首先检测请求项的合法性。若请求队列中已没有请求项则退出（参见blk.h，127行）。
然后取设备号中的子设备号（见列表后对硬盘设备号的说明）以及设备当前请求项中的起始
扇区号。子设备号即对应硬盘上各分区。如果子设备号不存在或者起始扇区大于该分区扇
区数-2，则结束该请求项，并跳转到标号r印eat处（定义在INIT—REQUEST开始处）。因为
一次要求读写一块数据（2个扇区，即1024字节），所以请求的扇区号不能大于分区中最后
倒数第二个扇区号。然后通过加上子设备号对应分区的起始扇区号，就把需要读写的块对应
到整个硬盘的绝对扇区号block上。而子设备号被5整除即可得到对应的硬盘号。
*/
	INIT_REQUEST;
	dev = MINOR(CURRENT->dev);
	block = CURRENT->sector;			/* 请求的起始扇区。	*/
/* 如果子设备号不存在或者起始扇区大于该分区扇区数-2，则结束该请求，并跳转到标号repeat 处	*/
/* （定义在INIT_REQUEST 开始处）。因为一次要求读写2 个扇区（512*2 字节），所以请求的扇区号	*/
/* 不能大于分区中最后倒数第二个扇区号。	*/
	if (dev >= 5*NR_HD || block+2 > hd[dev].nr_sects) {
		end_request(0);
		goto repeat;					/* 该标号在blk.h 最后面。	*/
	}
	block += hd[dev].start_sect;		/* 将所需读的块对应到整个硬盘上的绝对扇区号。	*/
	dev /= 5;							/* 此时dev 代表硬盘号（硬盘0 或硬盘1）。	*/
/* 然后根据求得的绝对扇区号block和硬盘号dev，我们就可以计算出对应硬盘中的磁道中扇
区号（sec）、所在柱面号（cyl）和磁头号（head）。下面嵌入的汇编代码即用来根据硬
盘信息结构中的每磁道扇区数和硬盘磁头数来计算这些数据。计算方法为：
310一311行代码初始时eax是扇区号block，edx中置Oodivl指令把edx:eax组成的扇区
号除以每磁道扇区数（hd—inf0[dev].sect），所得整数商值在eax中，余数在edx中。其
中eax中是到指定位置的对应总磁道数（所有磁头面），edx中是当前磁道上的扇区号。
312一313行代码初始时eax是计算出的对应总磁道数，edx中置Oodivl指令把edx:eax
的对应总磁道数除以硬盘总磁头数（hd—inf0[dev].head），在eax中得到的整除值是柱面
号（cyl）, edx中得到的余数就是对应得当前磁头号（head）。
*/
	__asm__("divl %4":"=a" (block),"=d" (sec):"0" (block),"1" (0),
		"r" (hd_info[dev].sect));
	__asm__("divl %4":"=a" (cyl),"=d" (head):"0" (block),"1" (0),
		"r" (hd_info[dev].head));
	sec++;								/* 对计算所得当前磁道扇区号进行调整。	*/
	nsect = CURRENT->nr_sectors;		/* 欲读/写的扇区数。	*/
/* 此时我们得到了欲读写的硬盘起始扇区block所对应的硬盘上柱面号(cyl)、在当前磁道
上的扇区号（sec）、磁头号（head）以及欲读写的总扇区数（nsect）。接着我们可以根
据这些信息向硬盘控制器发送I/O操作信息了。但在发送之前我们还需要先看看是否有复
位控制器状态和重新校正硬盘的标志。通常在复位操作之后都需要重新校正硬盘磁头位置。
若这些标志已被置位，则说明前面的硬盘操作可能出现了一些问题，或者现在是系统第一
次硬盘读写操作等情况。于是我们就需要重新复位硬盘或控制器并重新校正硬盘。
如果此时复位标志reset是置位的，则需要执行复位操作。复位硬盘和控制器，并置硬盘
需要重新校正标志，返回。reset—hdO将首先向硬盘控制器发送复位（重新校正）命令，
然后发送硬盘控制器命令“建立驱动器参数”。
*/
	if (reset) {
		reset = 0;
		recalibrate = 1;				/* 置需重新校正标志。	*/
		reset_hd(CURRENT_DEV);
		return;
	}
/* 如果以上两个标志都没有置位，那么我们就可以开始向硬盘控制器发送真正的数据读/写
操作命令了。如果当前请求是写扇区操作，则发送写命令，循环读取状态寄存器信息并判
断请求服务标志DRQ—STAT是否置位。DRQ—STAT是硬盘状态寄存器的请求服务位，表示驱
动器已经准备好在主机和数据端口之间传输一个字或一个字节的数据。这方面的信息可参
见本章前面的硬盘操作读/写时序图。
*/
	if (recalibrate) {
		recalibrate = 0;
		hd_out(dev,hd_info[CURRENT_DEV].sect,0,0,0,
			WIN_RESTORE,&recal_intr);
		return;
	}
/* 如果当前请求是写扇区操作，则发送写命令，循环读取状态寄存器信息并判断请求服务标志	*/
/* DRQ_STAT 是否置位。DRQ_STAT 是硬盘状态寄存器的请求服务位（include/linux/hdreg.h，27）。	*/
	if (CURRENT->cmd == WRITE) {
		hd_out(dev,nsect,sec,head,cyl,WIN_WRITE,&write_intr);
		for(i=0 ; i<3000 && !(r=inb_p(HD_STATUS)&DRQ_STAT) ; i++)
			/* nothing */ ;
/* nothing */ ;
/* 如果请求服务DRQ置位则退出循环。若等到循环结束也没有置位，则表示发送的要求写硬
盘命令失败，于是跳转去处理出现的问题或继续执行下一个硬盘请求。否则我们就可以向
硬盘控制器数据寄存器端口 HD—DATA写入1个扇区的数据。	*/
		if (!r) {
			bad_rw_intr();
			goto repeat;
		}
		port_write(HD_DATA,CURRENT->buffer,256);
/* 如果当前请求是读硬盘数据，则向硬盘控制器发送读扇区命令。若命令无效则停机。	*/
	} else if (CURRENT->cmd == READ) {
		hd_out(dev,nsect,sec,head,cyl,WIN_READ,&read_intr);
	} else
		panic("unknown hd-command");
}

/* 硬盘系统初始化。
设置硬盘中断描述符，并允许硬盘控制器发送中断请求信号。
该函数设置硬盘设备的请求项处理函数指针为do—hd—requestO，然后设置硬盘中断门描述
符。hd—interrupt(kernel/system—call.s，第221行)是其中断处理过程。硬盘中断号为
int 0x2E (46)，对应8259A芯片通中断请求信号IRQ13。接着复位接联的主8259A int2
的屏蔽位，允许从片发出中断请求信号。再复位硬盘的中断请求屏蔽位（在从片上），允许
硬盘控制器发送中断请求信号。中断描述符表IDT内中断门描述符设置宏set—intr—gateO
在 include/asm/system.h 中实现。
*/
void hd_init(void)
{
	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;	/* do_hd_request()。	*/
	set_intr_gate(0x2E,&hd_interrupt);				/* 设置硬盘中断门向量 int 0x2E(46)。	*/
													/* hd_interrupt 在(kernel/system_call.s,221)。	*/
	outb_p(inb_p(0x21)&0xfb,0x21);					/* 复位接联的主8259A int2 的屏蔽位，允许从片	*/
													/* 发出中断请求信号。	*/
	outb(inb_p(0xA1)&0xbf,0xA1);					/* 复位硬盘的中断请求屏蔽位（在从片上），允许	*/
													/* 硬盘控制器发送中断请求信号。	*/
}
