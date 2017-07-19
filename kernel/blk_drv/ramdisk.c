/*
* linux/kernel/blk_drv/ramdisk.c
*
* Written by Theodore Ts'o, 12/2/91
*/
/* 由Theodore Ts'o 编制，12/2/91
*/
/* Theodore Ts’o (Ted Ts’o)是Linux社区中的著名人物。Linux在世界范围内的流行也有他很
大的功劳。早在Linux操作系统刚问世时，他就怀着极大的热情为Linux的发展提供了电子邮件列
表服务maillist，并在北美地区最早设立了 Linux的ftp服务器站点（tsx-ll.mit.edu），
而且至今仍为广大Linux用户提供服务。他对Linux作出的最大贡献之一是提出并实现了 ext2文
件系统。该文件系统已成为Linux世界中事实上的文件系统标准。最近他又推出了 ext3文件系统，
技术中心工作，并从事着有关LSB (Linux Standard Base)等方面的工作。（他的个人主页是：
http://thunk.org/tytso/）
*/

#include <string.h>			/* 字符串头文件。主要定义了一些有关字符串操作的嵌入函数。	*/
#include <linux/config.h>	/* 内核配置头文件。定义键盘语言和硬盘类型（HD_TYPE）可选项。	*/
#include <linux/sched.h>	/* 调度程序头文件，定义了任务结构task_struct、初始任务0 的数据，	*/
							/* 还有一些有关描述符参数设置和获取的嵌入式汇编函数宏语句。	*/
#include <linux/fs.h>		/* 文件系统头文件。定义文件表结构（file,buffer_head,m_inode 等）。	*/
#include <linux/kernel.h>	/* 内核头文件。含有一些内核常用函数的原形定义。	*/
#include <asm/system.h>		/* 系统头文件。定义了设置或修改描述符/中断门等的嵌入式汇编宏。	*/
#include <asm/segment.h>	/* 段操作头文件。定义了有关段寄存器操作的嵌入式汇编函数。	*/
#include <asm/memory.h>		/* 内存拷贝头文件。含有memcpy()嵌入式汇编宏函数。	*/

/* 定义RAM盘主设备号符号常数。在驱动程序中主设备号必须在包含blk.h文件之前被定义。
 * 因为blk.h文件中要用到这个符号常数值来确定一些列的其他常数符号和宏。
 */
#define MAJOR_NR 1			/* 内存主设备号是1。	*/
#include "blk.h"

/* 虚拟盘在内存中的起始位置。该位置会在第52行上初始化函数rd—init()中确定。参见内核
初始化程序init/main.c，第124行。’rd’是’ramdisk’的缩写。
*/
char *rd_start;				/* 虚拟盘在内存中的起始位置。在52 行初始化函数rd_init()中	*/
							/* 确定。参见(init/main.c,124)（缩写rd_代表ramdisk_）。	*/
int rd_length = 0;			/* 虚拟盘所占内存大小（字节）。	*/

/* 虚拟盘当前请求项操作函数。
该函数的程序结构与硬盘的do—hd—requestO函数类似，参见hd.c，294行。在低级块设备
接口函数ll—rW—block()建立起虚拟盘（rd）的请求项并添加到rd的链表中之后，就会调
用该函数对rd当前请求项进行处理。该函数首先计算当前请求项中指定的起始扇区对应虚
拟盘所处内存的起始位置addr和要求的扇区数对应的字节长度值len，然后根据请求项中
的命令进行操作。若是写命令TOITE，就把请求项所指缓冲区中的数据直接复制到内存位置
addr处。若是读操作则反之。数据复制完成后即可直接调用end—requestO对本次请求项
作结束处理。然后跳转到函数开始处再去处理下一个请求项。若已没有请求项则退出。
*/
void
do_rd_request (void)
{
	int len;
	char *addr;

/* 首先检测请求项的合法性，若已没有请求项则退出（参见blk.h，第127行）。然后计算请
求项处理的虚拟盘中起始扇区在物理内存中对应的地址addr和占用的内存字节长度值len。
下句用于取得请求项中的起始扇区对应的内存起始位置和内存长度。其中sector << 9表示
sector * 512，换算成字节值。CURRENT 被定义为（blk_dev[MAJOR—NR].current—request）。
*/

	INIT_REQUEST;				/* 检测请求的合法性(参见kernel/blk_drv/blk.h,127)。	*/
/* 下面语句取得ramdisk 的起始扇区对应的内存起始位置和内存长度。	*/
/* 其中sector << 9 表示sector * 512，CURRENT 定义为(blk_dev[MAJOR_NR].current_request)。	*/
	addr = rd_start + (CURRENT->sector << 9);
	len = CURRENT->nr_sectors << 9;
/* 如果当前请求项中子设备号不为1或者对应内存起始位置大于虚拟盘末尾，则结束该请求项，
并跳转到repeat处去处理下一个虚拟盘请求项。标号repeat定义在宏INIT—REQUEST内，
位于宏的开始处，参见blk.h文件第127行。	*/
	if ((MINOR (CURRENT->dev) != 1) || (addr + len > rd_start + rd_length))
		{
			end_request (0);
			goto repeat;
		}
/* 然后进行实际的读写操作。如果是写命令（WRITE），则将请求项中缓冲区的内容复制到地址
addr处，长度为len字节。如果是读命令（READ），则将addr开始的内存内容复制到请求项
缓冲区中，长度为len字节。否则显示命令不存在，死机。
*/
	if (CURRENT->cmd == WRITE)
		{
			(void) memcpy (addr, CURRENT->buffer, len);
/* 如果是读命令(READ)，则将addr 开始的内容复制到请求项中缓冲区中，长度为len 字节。	*/
		}
	else if (CURRENT->cmd == READ)
		{
			(void) memcpy (CURRENT->buffer, addr, len);
/* 否则显示命令不存在，死机。	*/
		}
	else
		panic ("unknown ramdisk-command");
/* 然后在请求项成功后处理，置更新标志。并继续处理本设备的下一请求项。	*/
	end_request (1);
	goto repeat;
}

/*
* Returns amount of memory which needs to be reserved.
*/
/* 返回内存虚拟盘ramdisk所需的内存量
虚拟盘初始化函数。
该函数首先设置虚拟盘设备的请求项处理函数指针指向do—rd—requestO，然后确定虚拟盘
在物理内存中的起始地址、占用字节长度值。并对整个虚拟盘区清零。最后返回盘区长度。
当linux/Makefile文件中设置过RAMDISK值不为零时，表示系统中会创建RAM虚拟盘设备。
在这种情况下的内核初始化过程中，本函数就会被调用（init/main.c，L124行）。该函数
的第2个参数length会被赋值成RAMDISK * 1024，单位为字节。
*/
long
rd_init (long mem_start, int length)
{
	int i;
	char *cp;

	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;/* do_rd_request()。	*/
	rd_start = (char *) mem_start;				/* 对于16MB系统该值为 4MB。	*/
	rd_length = length;							/* 虚拟盘区域长度值。	*/
	cp = rd_start;
	for (i = 0; i < length; i++)
		*cp++ = '\0';
	return (length);
}

/*
* If the root device is the ram disk, try to load it.
* In order to do this, the root device is originally set to the
* floppy, and we later change it to be ram disk.
*/
/*
* 如果根文件系统设备(root device)是ramdisk 的话，则尝试加载它。root device 原先是指向
* 软盘的，我们将它改成指向ramdisk。
*/
/* 尝试把根文件系统加载到虚拟盘中。
该函数将在内核设置函数Setup()(hd.c，156行)中被调用。另外，1磁盘块=1024字节。
第75行上的变量block=256表示根文件系统映像文件被存储于boot盘第256磁盘块开始处。
*/
void
rd_load (void)
{
	struct buffer_head *bh;		/* 高速缓冲块头指针。	*/
	struct super_block s;			/* 文件超级块结构。	*/
	int block = 256;				/* Start at block 256 */
	int i = 1;
	int nblocks;					/* 文件系统盘块总数。	*/
	char *cp;						/* Move pointer */

/* 首先检查虚拟盘的有效性和完整性。如果ramdisk的长度为零，则退出。否则显示ramdisk
的大小以及内存起始位置。如果此时根文件设备不是软盘设备，则也退出。
*/
	if (!rd_length)				/* 如果ramdisk 的长度为零，则退出。	*/
		return;
	printk ("Ram disk: %d bytes, starting at 0x%x\n", rd_length, (int) rd_start);	/* 显示ramdisk 的大小以及内存起始位置。	*/
	if (MAJOR (ROOT_DEV) != 2)	/* 如果此时根文件设备不是软盘，则退出。	*/
		return;
/* 然后读根文件系统的基本参数。即读软盘块256+1、256和256+2。这里block+1是指磁盘上
的超级块。breadaO用于读取指定的数据块，并标出还需要读的块，然后返回含有数据块的
缓冲区指针。如果返回NULL，则表示数据块不可读（fs/buffer.c，322）。然后把缓冲区中
的磁盘超级块（d—super—block是磁盘超级块结构）复制到s变量中，并释放缓冲区。接着
我们开始对超级块的有效性进行判断。如果超级块中文件系统魔数不对，则说明加载的数据
块不是MINIX文件系统，于是退出。有关MINIX超级块的结构请参见文件系统一章内容。
*/
	bh = breada (ROOT_DEV, block + 1, block, block + 2, -1);
	if (!bh)
		{
			printk ("Disk error while looking for ramdisk!\n");
			return;
		}
/* 将s 指向缓冲区中的磁盘超级块。(d_super_block 磁盘中超级块结构)。	*/
	*((struct d_super_block *) &s) = *((struct d_super_block *) bh->b_data);
	brelse (bh);					/* [为什么数据没有复制就立刻释放呢？]	*/
	if (s.s_magic != SUPER_MAGIC)	/* No ram disk image present, assume normal floppy boot */
									/*磁盘中没有ramdisk映像文件，退出去执行通常的软盘引导却	*/

/* 然后我们试图把整个根文件系统读入到内存虚拟盘区中。对于一个文件系统来说，其超级块
结构的s_nzones字段中保存着总逻辑块数（或称为区段数）。一个逻辑块中含有的数据块
数则由字段s_log_zone_size指定。因此文件系统中的数据块总数nblocks就等于（逻辑块
数 * 2^ (每区段块数的次方)），即 nblocks = (s_nzones * 2~s_log_zone_ize)。如果遇到
文件系统中数据块总数大于内存虚拟盘所能容纳&块数的情况，则不能执行加载操作，而只
能显示出错信息并返回。
*/
		return;
	nblocks = s.s_nzones << s.s_log_zone_size;
	if (nblocks > (rd_length >> BLOCK_SIZE_BITS))
		{
			printk ("Ram disk image too big! (%d blocks, %d avail)\n",
				nblocks, rd_length >> BLOCK_SIZE_BITS);
			return;
		}
/* 否则若虚拟盘能容纳得下文件系统总数据块数，则我们显示加载数据块信息，并让cp指向
内存虚拟盘起始处，然后开始执行循环操作将磁盘上根文件系统映像文件加载到虚拟盘上。
在操作过程中，如果一次需要加载的盘块数大于2块，我们就是用超前预读函数breadaO，
否则就使用breadO函数进行单块读取。若在读盘过程中出现I/O操作错误，就只能放弃加
载过程返回。所读取的磁盘块会使用memcpyO函数从高速缓冲区中复制到内存虚拟盘相应
位置处，同时显示已加载的块数。显示字符串中的八进制数’\010’表示显示一个制表符。
*/
	printk ("Loading %d bytes into ram disk... 0000k",
		nblocks << BLOCK_SIZE_BITS);
/* cp 指向虚拟盘起始处，然后将磁盘上的根文件系统映象文件复制到虚拟盘上。	*/
	cp = rd_start;
	while (nblocks)
		{
			if (nblocks > 2)		/* 如果需读取的块数多于3 快则采用超前预读方式读数据块。	*/
	bh = breada (ROOT_DEV, block, block + 1, block + 2, -1);
			else					/* 否则就单块读取。	*/
	bh = bread (ROOT_DEV, block);
			if (!bh)
	{
		printk ("I/O error on block %d, aborting load\n", block);
		return;
	}
			(void) memcpy (cp, bh->b_data, BLOCK_SIZE);	/* 将缓冲区中的数据复制到cp 处。	*/
			brelse (bh);								/* 释放缓冲区。	*/
			printk ("\010\010\010\010\010%4dk", i);		/* 打印加载块计数值。	*/
			cp += BLOCK_SIZE;							/* 虚拟盘指针前移。	*/
			block++;
			nblocks--;
			i++;
		}
/* 当boot盘中从256盘块开始的整个根文件系统加载完毕后，我们显示“done”，并把目前
根文件设备号修改成虚拟盘的设备号0x0101，最后返回。
*/
	printk ("\010\010\010\010\010done \n");
	ROOT_DEV = 0x0101;		/* 修改ROOT_DEV 使其指向虚拟盘ramdisk。	*/
}
