/*
* linux/kernel/blk_dev/ll_rw.c
*
* (C) 1991 Linus Torvalds
*/

/*
* This handles all read/write requests to block devices
*/
/*
* 该程序处理块设备的所有读/写操作。
*/
#include <errno.h>			/* 错误号头文件。包含系统中各种出错号。(Linus 从minix 中引进的)	*/
#include <linux/sched.h>	/* 调度程序头文件，定义了任务结构task_struct、初始任务0 的数据，	*/
							/* 还有一些有关描述符参数设置和获取的嵌入式汇编函数宏语句。	*/
#include <linux/kernel.h>	/* 内核头文件。含有一些内核常用函数的原形定义。	*/
#include <asm/system.h>		/* 系统头文件。定义了设置或修改描述符/中断门等的嵌入式汇编宏。	*/
#include "blk.h"			/* 块设备头文件。定义请求数据结构、块设备数据结构和宏函数等信息。	*/

/*
* The request-struct contains all necessary data
* to load a nr of sectors into memory
*/
/*
* 请求结构中含有加载nr 扇区数据到内存的所有必须的信息。
*/
struct request request[NR_REQUEST];

/*
* used to wait on when there are no free requests
*/
/* 是用于请求数组没有空闲项时的临时等待处 */
struct task_struct *wait_for_request = NULL;

/* blk_dev_struct is:
* do_request-address
* next-request
*/
/* blk_dev_struct 		块设备结构是：(kernel/blk_drv/blk.h,23)
* do_request-address 	对应主设备号的请求处理程序指针。
* current-request 		该设备的下一个请求。
*/
/* 该数组使用主设备号作为索引（下标）。	*/
struct blk_dev_struct blk_dev[NR_BLK_DEV] = {
	{NULL, NULL},			/* no_dev	0 - 无设备。	*/
	{NULL, NULL},			/* dev mem	1 - 内存。	*/
	{NULL, NULL},			/* dev fd 	2 - 软驱设备。	*/
	{NULL, NULL},			/* dev hd	3 - 硬盘设备。	*/
	{NULL, NULL},			/* dev ttyx	4 - ttyx 设备。	*/
	{NULL, NULL},			/* dev tty	5 - tty 设备。	*/
	{NULL, NULL}			/* dev lp	6 - lp 打印机设备。	*/
};

/* 锁定指定的缓冲区bh。
* 如果指定的缓冲区已经被其它任务锁定，则使自己睡眠（不可中断地等待），
* 直到被执行解锁缓冲区的任务明确地唤醒。	*/
static inline void
lock_buffer (struct buffer_head *bh)
{
	cli ();					/* 清中断许可。	*/
	while (bh->b_lock)		/* 如果缓冲区已被锁定，则睡眠，直到缓冲区解锁。	*/
		sleep_on (&bh->b_wait);
	bh->b_lock = 1;			/* 立刻锁定该缓冲区。	*/
	sti ();					/* 开中断。	*/
}

/* 释放（解锁）锁定的缓冲区。	*/
static inline void
unlock_buffer (struct buffer_head *bh)
{
	if (!bh->b_lock)			/* 如果该缓冲区并没有被锁定，则打印出错信息。	*/
		printk ("ll_rw_block.c: buffer not locked\n\r");
	bh->b_lock = 0;			/* 清锁定标志。	*/
	wake_up (&bh->b_wait);	/* 唤醒等待该缓冲区的任务。	*/
}

/*
* add-request adds a request to the linked list.
* It disables interrupts so that it can muck with the
* request-lists in peace.
*/
/*
* add-request()向连表中加入一项请求。它关闭中断，
* 这样就能安全地处理请求连表了 */
/* 向链表中加入请求项。
参数dev是指定块设备结构指针，该结构中有处理请求项函数指针和当前正在请求项指针；
req是已设置好内容的请求项结构指针。
本函数把已经设置好的请求项req添加到指定设备的请求项链表中。如果该设备的当前请求
请求项指针为空，则可以设置req为当前请求项并立刻调用设备请求项处理函数。否则就把
req请求项插入到该请求项链表中。
*/
static void add_request (struct blk_dev_struct *dev, struct request *req)
{
	struct request *tmp;

/* 首先再进一步对参数提供的请求项的指针和标志作初始设置。置空请求项中的下一请求项指
 * 针，关中断并清除请求项相关缓冲区脏标志。
 */
	req->next = NULL;
	cli ();							/* 关中断。	*/
	if (req->bh)
		req->bh->b_dirt = 0;		/* 清缓冲区“脏”标志。	*/
/* 然后查看指定设备是否有当前请求项，即查看设备是否正忙。如果指定设备dev当前请求项
(current—request)子段为空，则表示目前该设备没有请求项，本次是第1个请求项，也是
唯一的一个。因此可将块设备当前请求指针直接指向该请求项，并立刻执行相应设备的请求
函数。
*/
	if (!(tmp = dev->current_request))
		{
			dev->current_request = req;
			sti ();					/* 开中断。	*/
			(dev->request_fn) ();	/* 执行设备请求函数，对于硬盘(3)是do_hd_request()。	*/
			return;
		}
/* 如果目前该设备已经有当前请求项在处理，则首先利用电梯算法搜索最佳插入位置，然后将
当前请求插入到请求链表中。最后开中断并退出函数。电梯算法的作用是让磁盘磁头的移动
距离最小，从而改善（减少）硬盘访问时间。
下面for循环中if语句用于把req所指请求项与请求队列（链表）中已有的请求项作比较，
找出req插入该队列的正确位置顺序。然后中断循环，并把req插入到该队列正确位置处。
*/
	for (; tmp->next; tmp = tmp->next)
		if ((IN_ORDER (tmp, req) ||
	 !IN_ORDER (tmp, tmp->next)) && IN_ORDER (req, tmp->next))
			break;
	req->next = tmp->next;
	tmp->next = req;
	sti ();
}

/* 创建请求项并插入请求队列。参数major是主设备号；rw是指定命令；bh是存放数据的缓冲区头指针。	*/
static void
make_request (int major, int rw, struct buffer_head *bh)
{
	struct request *req;
	int rw_ahead;

/* WRITEA/READA is special case - it is not really needed, so if the */
/* buffer is locked, we just forget about it, else it's a normal read */
/* WRITEA/READA是一种特殊情况-它们并非必要，所以如果缓冲区已经上锁，
我们就不用管它，否则的话它只是一个一般的读操作。
这里’READ’和’WRITE’后面的’A’字符代表英文单词Ahead，表示提前预读/写数据块的意思。
该函数首先对命令READA/WRITEA的情况进行一些处理。对于这两个命令，当指定的缓冲区
正在使用而已被上锁时，就放弃预读/写请求。否则就作为普通的READ/WRITE命令进行操作。
另外，如果参数给出的命令既不是READ也不是WRITE，则表示内核程序有错，显示出错信
息并停机。注意，在修改命令之前这里已为参数是否是预读/写命令设置了标志rw—ahead。
*/
	if (rw_ahead = (rw == READA || rw == WRITEA))
		{
			if (bh->b_lock)
	return;
			if (rw == READA)
	rw = READ;
			else
	rw = WRITE;
		}
/* 如果命令不是READ 或WRITE 则表示内核程序有错，显示出错信息并死机。	*/
	if (rw != READ && rw != WRITE)
		panic ("Bad block dev command, must be R/W/RA/WA");
/* 对命令rw进行了一番处理之后，现在只有READ或WRITE两种命令。在开始生成和添加相
应读/写数据请求项之前，我们再来看看此次是否有必要添加请求项。在两种情况下可以不
必添加请求项。一是当命令是写（WRITE），但缓冲区中的数据在读入之后并没有被修改过；
二是当命令是读（READ），但缓冲区中的数据已经是更新过的，即与块设备上的完全一样。
因此这里首先锁定缓冲区对其检查一下。如果此时缓冲区已被上锁，则当前任务就会睡眠，
直到被明确地唤醒。如果确实是属于上述两种情况，那么就可以直接解锁缓冲区，并返回。
这几行代码体现了高速缓冲区的用意，在数据可靠的情况下就无须再执行硬盘操作，而直接
使用内存中的现有数据。缓冲块的b_dirt标志用于表示缓冲块中的数据是否已经被修改过。
 b_uptodate标志用于表示缓冲块中的数据是与块设备上的同步，即在从块设备上读入缓冲块
后没有修改过。
*/
	lock_buffer (bh);
/* 如果命令是写并且缓冲区数据不脏，或者命令是读并且缓冲区数据是更新过的，则不用添加	*/
/* 这个请求。将缓冲区解锁并退出。	*/
	if ((rw == WRITE && !bh->b_dirt) || (rw == READ && bh->b_uptodate))
		{
			unlock_buffer (bh);
			return;
		}
repeat:
/* we don't allow the write-requests to fill up the queue completely:
* we want some room for reads: they take precedence. The last third
* of the requests are only for reads.
*/
/* 我们不能让队列中全都是写请求项：我们需要为读请求保留一些空间：读操作
* 是优先的。请求队列的后三分之一空间是为读准备的。
*/
/* 好，现在我们必须为本函数生成并添加读/写请求项了。首先我们需要在请求数组中寻找到
一个空闲项（槽）来存放新请求项。搜索过程从请求数组末端开始。根据上述要求，对于读
命令请求，我们直接从队列末尾开始搜索，而对于写请求就只能从队列2/3处向队列头处搜
索空项填入。于是我们开始从后向前搜索，当请求结构request的设备字段dev值=-1时，
表示该项未被占用（空闲）。如果没有一项是空闲的（此时请求项数组指针已经搜索越过头
部），则查看此次请求是否是提前读/写（READA或WRITEA），如果是则放弃此次请求操作。
否则让本次请求操作先睡眠（以等待请求队列腾出空项），过一会再来搜索请求队列。
*/
	if (rw == READ)
		req = request + NR_REQUEST;				/* 对于读请求，将队列指针指向队列尾部。	*/
	else
		req = request + ((NR_REQUEST * 2) / 3);	/* 对于写请求，队列指针指向队列2/3 处。	*/
/* find an empty request */
/* 搜索一个空请求项 */
/* 从后向前搜索，当请求结构request 的dev 字段值=-1 时，表示该项未被占用。	*/
	while (--req >= request)
		if (req->dev < 0)
			break;
/* if none found, sleep on new requests: check for rw_ahead */
/* 如果没有找到空闲项，则让该次新请求睡眠：需检查是否提前读/写 */
/* 如果没有一项是空闲的（此时request 数组指针已经搜索越过头部），则查看此次请求是否是	*/
/* 提前读/写（READA 或WRITEA），如果是则放弃此次请求。否则让本次请求睡眠（等待请求队列	*/
/* 腾出空项），过一会再来搜索请求队列。	*/
	if (req < request)
		{									/* 如果请求队列中没有空项，则	*/
			if (rw_ahead)
	{									/* 如果是提前读/写请求，则解锁缓冲区，退出。	*/
		unlock_buffer (bh);
		return;
	}
			sleep_on (&wait_for_request);		/* 否则让本次请求睡眠，过会再查看请求队列。	*/
			goto repeat;
		}
/* fill up the request-info, and add it to the queue */
/* 向空闲请求项中填写请求信息，并将其加入队列中 */
/* 0K，程序执行到这里表示已找到一个空闲请求项。于是我们在设置好的新请求项后就调用
add—request()把它添加到请求队列中，立马退出。请求结构请参见blk—drv/blk.h，23行。
req-)sector是读写操作的起始扇区号，req-}buffer是请求项存放数据的缓冲区。	*/
	req->dev = bh->b_dev;					/* 设备号。	*/
	req->cmd = rw;							/* 命令(READ/WRITE)。	*/
	req->errors = 0;						/* 操作时产生的错误次数。	*/
	req->sector = bh->b_blocknr << 1;		/* 起始扇区。(1 块=2 扇区)	*/
	req->nr_sectors = 2;					/* 读写扇区数。	*/
	req->buffer = bh->b_data;				/* 数据缓冲区。	*/
	req->waiting = NULL;					/* 任务等待操作执行完成的地方。	*/
	req->bh = bh;							/* 缓冲区头指针。	*/
	req->next = NULL;						/* 指向下一请求项。	*/
	add_request (major + blk_dev, req);		/* 将请求项加入队列中(blk_dev[major],req)。	*/
}

/* 低层读写数据块函数（Low Level Read Write Block）。
该函数是块设备驱动程序与系统其他部分的接口函数。通常在fs/buffer.c程序中被调用。
主要功能是创建块设备读写请求项并插入到指定块设备请求队列中。实际的读写操作则是
由设备的request_fn()函数完成。对于硬盘操作，该函数是do_hd_request();对于软盘
操作，该函数是do_fd_request();对于虚拟盘则是do_rd_request()。另外，在调用该函
数之前，调用者需要首先把读/写块设备的信息保存在缓冲块头结构中，如设备号、块号。
参数：rw - READ、READA、WRITE或WRITEA是命令；bh -数据缓冲块头指针。
*/
void
ll_rw_block (int rw, struct buffer_head *bh)
{
	unsigned int major;					/* 主设备号（对于硬盘是3）。	*/

/* 如果设备主设备号不存在或者该设备的请求操作函数不存在，则显示出错信息，并返回。
 * 否则创建请求项并插入请求队列。
 */
	if ((major = MAJOR (bh->b_dev)) >= NR_BLK_DEV ||
			!(blk_dev[major].request_fn))
		{
			printk ("Trying to read nonexistent block-device\n\r");
			return;
		}
	make_request (major, rw, bh);			/* 创建请求项并插入请求队列。	*/
}

/* 块设备初始化函数，由初始化程序main.c 调用（init/main.c,128）。	*/
/* 初始化请求数组，将所有请求项置为空闲项(dev = -1)。有32 项(NR_REQUEST = 32)。	*/
void
blk_dev_init (void)
{
	int i;

	for (i = 0; i < NR_REQUEST; i++)
		{
			request[i].dev = -1;
			request[i].next = NULL;
		}
}
