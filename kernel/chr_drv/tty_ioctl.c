/*
* linux/kernel/chr_drv/tty_ioctl.c
*
* (C) 1991 Linus Torvalds
*/

#include <errno.h>			/* 错误号头文件。包含系统中各种出错号。(Linus 从minix 中引进的)	。	*/
#include <termios.h>		/* 终端输入输出函数头文件。主要定义控制异步通信口的终端接口。	*/
#include <linux/sched.h>	/* 调度程序头文件，定义了任务结构task_struct、初始任务0 的数据，	*/
							/* 还有一些有关描述符参数设置和获取的嵌入式汇编函数宏语句。	*/
#include <linux/kernel.h>	/* 内核头文件。含有一些内核常用函数的原形定义。	*/
#include <linux/tty.h>		/* tty 头文件，定义了有关tty_io，串行通信方面的参数、常数。	*/
#include <asm/io.h>			/* io 头文件。定义硬件端口输入/输出宏汇编语句。	*/
#include <asm/segment.h>	/* 段操作头文件。定义了有关段寄存器操作的嵌入式汇编函数。	*/
#include <asm/system.h>		/* 系统头文件。定义了设置或修改描述符/中断门等的嵌入式汇编宏。	*/

/* 这是波特率因子数组（或称为除数数组）。波特率与波特率因子的对应关系参见列表后说明。
 * 例如波特率是2400bps时，对应的因子是48 (0x30);9600bps的因子是12(0xlc)。
 */
static unsigned short quotient[] = {
	0, 2304, 1536, 1047, 857,
	768, 576, 384, 192, 96,
	64, 48, 24, 12, 6, 3
};

/* 修改传输波特率。
 * 参数：tty -终端对应的tty数据结构。
 * 在除数锁存标志DLAB置位情况下，通过端口 0x3f8和0x3f9向UART分别写入波特率因子低
 * 字节和高字节。写完后再复位DLAB位。对于串口 2，这两个端口分别是0x2f8和0x2f9。
 */
static void
change_speed (struct tty_struct *tty)
{
	unsigned short port, quot;

/* 函数首先检查参数tty指定的终端是否是串行终端，若不是则退出。对于串口终端的tty结
 * 构，其读缓冲队列data字段存放着串行端口基址（0x3f8或0x2f8），而一般控制台终端的tty结
 * 构的read_q.data字段值为0。然后从终端termios结构的控制模式标志集中取得已设
 * 置的波特率索引号，并据此从波特率因子数组quotient[]中取得对应的波特率因子值quot。
 * CBAUD是控制模式标志集中波特率位屏蔽码。
*/
	if (!(port = tty->read_q.data))
		return;
	quot = quotient[tty->termios.c_cflag & CBAUD];
/* 接着把波特率因子quot写入串行端口对应UART芯片的波特率因子锁存器中。在写之前我们
 * 先要把线路控制寄存器LCR的除数锁存访问比特位DLAB (位7)置1。然后把16位的波特
 * 率因子低高字节分别写入端口 0x3f8、0x3f9 (分别对应波特率因子低、高字节锁存器)。
 * 最后再复位LCR的DLAB标志位。
 */
	cli ();							/* 关中断。	*/
	outb_p (0x80, port + 3);		/* set DLAB	首先设置除数锁定标志DLAB。	*/
	outb_p (quot & 0xff, port);		/* LS of divisor	输出因子低字节。	*/
	outb_p (quot >> 8, port + 1);	/* MS of divisor	输出因子高字节。	*/
	outb (0x03, port + 3);			/* reset DLAB		复位DLAB。	*/
	sti ();							/* 开中断。	*/
}

/* 刷新tty 缓冲队列。	*/
/* 参数：gueue - 指定的缓冲队列指针。	*/
/* 令缓冲队列的头指针等于尾指针，从而达到清空缓冲区(零字符)的目的。	*/
static void
flush (struct tty_queue *queue)
{
	cli ();
	queue->head = queue->tail;
	sti ();
}

/* 等待字符发送出去。	*/
static void
wait_until_sent (struct tty_struct *tty)
{
/* do nothing - not implemented	什么都没做 - 还未实现 */
}

/* 发送BREAK 控制符。	*/
static void
send_break (struct tty_struct *tty)
{
/* do nothing - not implemented	什么都没做 - 还未实现 */
}

/* 取终端termios 结构信息。	*/
/* 参数：tty - 指定终端的tty 结构指针；termios - 用户数据区termios 结构缓冲区指针。	*/
/* 返回0 。	*/
static int
get_termios (struct tty_struct *tty, struct termios *termios)
{
	int i;

/* 首先验证用户缓冲区指针所指内存区容量是否足够，如不够则分配内存。然后复制指定终端
 * 的termios结构信息到用户缓冲区中。最后返回0。*/
	verify_area (termios, sizeof (*termios));
	for (i = 0; i < (sizeof (*termios)); i++)
		put_fs_byte (((char *) &tty->termios)[i], i + (char *) termios);
	return 0;
}

/* 设置终端termios 结构信息。	*/
/* 参数：tty - 指定终端的tty 结构指针；termios - 用户数据区termios 结构指针。	*/
/* 返回0 。	*/
static int
set_termios (struct tty_struct *tty, struct termios *termios)
{
	int i;

/* 首先把用户数据区中termios结构信息复制到指定终端tty结构的termios结构中。因为用
 * 户有可能已修改了终端串行口传输波特率，所以这里再根据termios结构中的控制模式标志
 * c_cflag中的波特率信息修改串行UART芯片内的传输波特率。最后返回0。
 */
	for (i = 0; i < (sizeof (*termios)); i++)
		((char *) &tty->termios)[i] = get_fs_byte (i + (char *) termios);
	change_speed (tty);
	return 0;
}

/* 读取termio 结构中的信息。
 * 参数：tty -指定终端的tty结构指针；termio -保存termio结构信息的用户缓冲区。
 * 返回0。	*/
static int
get_termio (struct tty_struct *tty, struct termio *termio)
{
	int i;
	struct termio tmp_termio;

/* 首先验证用户的缓冲区指针所指内存区容量是否足够，如不够则分配内存。然后将termios
 * 结构的信息复制到临时termio结构中。这两个结构基本相同，但输入、输出、控制和本地
 * 标志集数据类型不同。前者的是long，而后者的是short。因此先复制到临时termio结构
 * 中目的是为了进行数据类型转换。
*/
	verify_area (termio, sizeof (*termio));
/* 将termios 结构的信息复制到termio 结构中。目的是为了其中模式标志集的类型进行转换，也即	*/
/* 从termios 的长整数类型转换为termio 的短整数类型。	*/
	tmp_termio.c_iflag = tty->termios.c_iflag;
	tmp_termio.c_oflag = tty->termios.c_oflag;
	tmp_termio.c_cflag = tty->termios.c_cflag;
	tmp_termio.c_lflag = tty->termios.c_lflag;
/* 两种结构的c_line 和c_cc[]字段是完全相同的。	*/
	tmp_termio.c_line = tty->termios.c_line;
	for (i = 0; i < NCC; i++)
		tmp_termio.c_cc[i] = tty->termios.c_cc[i];
/* /最后逐字节地把临时termio结构中的信息复制到用户termio结构缓冲区中。并返回0。	*/
	for (i = 0; i < (sizeof (*termio)); i++)
		put_fs_byte (((char *) &tmp_termio)[i], i + (char *) termio);
	return 0;
}

/*
* This only works as the 386 is low-byt-first
*/
/*
* 下面的termio 设置函数仅在386 低字节在前的方式下可用。
*/
/* 设置终端termio结构信息。
 * 参数：tty -指定终端的tty结构指针；termio -用户数据区中termio结构。
 * 将用户缓冲区termio的信息复制到终端的termios结构中。返回0。
 */
static int set_termio (struct tty_struct *tty, struct termio *termio)
{
	int i;
	struct termio tmp_termio;

/* 首先复制用户数据区中termio结构信息到临时termio结构中。然后再将termio结构的信息
 * 复制到tty的termios结构中。这样做的目的是为了对其中模式标志集的类型进行转换，即 
 * 从termio的短整数类型转换成termios的长整数类型。但两种结构的c—line和c—cc[]字段
 * 是完全相同的。
 */
	for (i = 0; i < (sizeof (*termio)); i++)
		((char *) &tmp_termio)[i] = get_fs_byte (i + (char *) termio);
/* 再将termio 结构的信息复制到tty 的termios 结构中。目的是为了其中模式标志集的类型进行转换，	*/
/* 也即从termio 的短整数类型转换成termios 的长整数类型。	*/
	*(unsigned short *) &tty->termios.c_iflag = tmp_termio.c_iflag;
	*(unsigned short *) &tty->termios.c_oflag = tmp_termio.c_oflag;
	*(unsigned short *) &tty->termios.c_cflag = tmp_termio.c_cflag;
	*(unsigned short *) &tty->termios.c_lflag = tmp_termio.c_lflag;
/* 两种结构的c_line 和c_cc[]字段是完全相同的。	*/
	tty->termios.c_line = tmp_termio.c_line;
	for (i = 0; i < NCC; i++)
		tty->termios.c_cc[i] = tmp_termio.c_cc[i];
/* 最后因为用户有可能已修改了终端串行口传输波特率，所以这里再根据termios结构中的控制
 * 模式标志c_cflag中的波特率信息修改串行UART芯片内的传输波特率，并返回0。
 */
	change_speed (tty);
	return 0;
}

/* tty终端设备输入输出控制函数。
 * 参数：dev -设备号；cmd - ioctl命令；arg -操作参数指针。
 * 该函数首先根据参数给出的设备号找出对应终端的tty结构，然后根据控制命令cmd分别进行处理。
*/
int tty_ioctl (int dev, int cmd, int arg)
{
	struct tty_struct *tty;
/* 首先根据设备号取得tty子设备号，从而取得终端的tty结构。若主设备号是5(控制终端)，
 * 则进程的tty字段即是tty子设备号。此时如果进程的tty子设备号是负数，表明该进程没有
 * 控制终端，即不能发出该ioctl调用，于是显示出错信息并停机。如果主设备号不是5而是4，
 * 我们就可以从设备号中取出子设备号。子设备号可以是0 (控制台终端)、1 (串口 1终端)、
 * 2(串口 2终端)。
 */
	if (MAJOR (dev) == 5)
	{
		dev = current->tty;
		if (dev < 0)
			panic ("tty_ioctl: dev<0");
/* 否则直接从设备号中取出子设备号。	*/
		}
	else
		dev = MINOR (dev);		/* 子设备号可以是0(控制台终端)、1(串口1 终端)、2(串口2 终端)。	*/

/* 然后根据子设备号表，我们可取得对应终端的tty结构。于是让tty指向对应子设备
 * 号的tty结构。然后再根据参数提供的ioctl命令cmd进行分别处理。	*/

	tty = dev + tty_table;		/* 让tty 指向对应子设备号的tty 结构。	*/

	switch (cmd)				/* 根据tty 的ioctl 命令进行分别处理。	*/
		{
/* 取相应终端termios结构信息。此时参数arg是用户缓冲区指针。	*/
		case TCGETS:
			return get_termios (tty, (struct termios *) arg);
/* 在设置termios结构信息之前，需要先等待输出队列中所有数据处理完毕，并且刷新（清空）
 * 输入队列。再接着执行下面设置终端termios结构的操作。	*/
		case TCSETSF:
			flush (&tty->read_q);	/* fallthroug	接着继续执行	*/
/* 在设置终端termios 的信息之前，需要先等待输出队列中所有数据处理完(耗尽)。	对于修改参数	*/
/* 会影响输出的情况，就需要使用这种形式。	*/
		case TCSETSW:
			wait_until_sent (tty);	/* fallthrough	接着继续执行	*/
/* 设置相应终端termios结构信息。此时参数arg是保存termios结构的用户缓冲区指针。	*/
		case TCSETS:
			return set_termios (tty, (struct termios *) arg);
/* 取相应终端terrnio结构中的信息。此时参数arg是用户缓冲区指针。	*/
		case TCGETA:
			return get_termio (tty, (struct termio *) arg);
/* 在设置termio结构信息之前，需要先等待输出队列中所有数据处理完毕，并且刷新（清空）
 * 输入队列。再接着执行下面设置终端termio结构的操作。	*/
		case TCSETAF:
			flush (&tty->read_q);	/* fallthrough */
/* 在设置终端termios的信^前，需要先等待输出队列中所有数据处理完（耗尽）。对于修改
 * 参数会影响输出的情况，就需要使用这种形式。	*/
		case TCSETAW:
			wait_until_sent (tty);	/* fallthrough	继续执行 */
/* 设置相应终端termio结构信息。此时参数arg是保存termio结构的用户缓冲区指针。	*/
		case TCSETA:
			return set_termio (tty, (struct termio *) arg);
/* 如果参数arg值是0，则等待输出队列处理完毕（空），并发送一个break。	*/
		case TCSBRK:
			if (!arg)
			{
				wait_until_sent (tty);
				send_break (tty);
			}
			return 0;
/* 开始/停止控制。	
 * 如果参数值是0，则挂起输出；
 * 如果是1，则恢复挂起的输出；
 * 如果是2，则挂起输入；
 * 如果是3，则重新开启挂起的输入。
 */
		case TCXONC:

			return -EINVAL;		/* not implemented	未实现 */
		case TCFLSH:
/* 刷新已写输出但还没有发送、或已收但还没有读的数据。如果参数arg是0，则刷新(清空)
 * 输入队列；如果是1，则刷新输出队列；如果是2，则刷新输入和输出队列。
 */

			if (arg == 0)
				flush (&tty->read_q);
			else if (arg == 1)
				flush (&tty->write_q);
			else if (arg == 2)
			{
				flush (&tty->read_q);
				flush (&tty->write_q);
			}
			else
				return -EINVAL;
			return 0;
		case TIOCEXCL:			/* 设置终端串行线路专用模式。	*/
			return -EINVAL;		/* not implemented	未实现 */
		case TIOCNXCL:			/* 复位终端串行线路专用模式。	*/
			return -EINVAL;		/* not implemented	未实现 */
		case TIOCSCTTY:			/* 设置tty 为控制终端。(TIOCNOTTY - 禁止tty 为控制终端)。	*/
			return -EINVAL;		/* set controlling term NI	设置控制终端NI */
		case TIOCGPGRP:		/* NI - Not Implemented。	*/
/* 读取终端设备进程组号。首先验证用户缓冲区长度，然后复制tty的pgrp字段到用户缓冲区。
 * 此时参数紅g是用户缓冲区指针。	*/
			verify_area ((void *) arg, 4);
			put_fs_long (tty->pgrp, (unsigned long *) arg);
			return 0;
		case TIOCSPGRP:
/* 设置终端设备的进程组号pgrp。此时参数紅g是用户缓冲区中pgrp的指针。	*/
			tty->pgrp = get_fs_long ((unsigned long *) arg);
			return 0;
		case TIOCOUTQ:
/* 返回输出队列中还未送出的字符数。首先验证用户缓冲区长度，然后复制队列中字符数给用户。
 * 此时参数紅g是用户缓冲区指针。	*/
			verify_area ((void *) arg, 4);
			put_fs_long (CHARS (tty->write_q), (unsigned long *) arg);
			return 0;
		case TIOCINQ:
/* 返回输入队列中还未读取的字符数。首先验证用户缓冲区长度，然后复制队列中字符数给用户。
 * 此时参数紅g是用户缓冲区指针。	*/
			verify_area ((void *) arg, 4);
			put_fs_long (CHARS (tty->secondary), (unsigned long *) arg);
			return 0;
		case TIOCSTI:
/* 模拟终端输入。该命令以一个指向字符的指针作为参数，并假装该字符是在终端上键入的。
 * 用户必须在该控制终端上具有超级用户权限或具有读许可权限。 */

			return -EINVAL;		/* not implemented	未实现 */
		case TIOCGWINSZ:		/* 读取终端设备窗口大小信息（参见termios.h 中的winsize 结构）。	*/
			return -EINVAL;		/* not implemented	未实现 */
		case TIOCSWINSZ:		/* 设置终端设备窗口大小信息（参见winsize 结构）。	*/
			return -EINVAL;		/* not implemented	未实现 */
		case TIOCMGET:			/* 返回modem 状态控制引线的当前状态比特位标志集（参见termios.h 中185-196 行）。	*/
			return -EINVAL;		/* not implemented	未实现 */
		case TIOCMBIS:			/* 设置单个modem 状态控制引线的状态(true 或false)。	*/
			return -EINVAL;		/* not implemented	未实现 */
		case TIOCMBIC:			/* 复位单个modem 状态控制引线的状态。	*/
			return -EINVAL;		/* not implemented	未实现 */
		case TIOCMSET:			/* 设置modem 状态引线的状态。如果某一比特位置位，则modem 对应的状态引线将置为有效。	*/
			return -EINVAL;		/* not implemented	未实现 */
		case TIOCGSOFTCAR:		/* 读取软件载波检测标志(1 - 开启；0 - 关闭)。	*/
			return -EINVAL;		/* not implemented	未实现 */
		case TIOCSSOFTCAR:		/* 设置软件载波检测标志(1 - 开启；0 - 关闭)。	*/
			return -EINVAL;		/* not implemented	未实现 */
		default:
			return -EINVAL;
		}
}
