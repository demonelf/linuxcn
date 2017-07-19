/*
* linux/mm/memory.c
*
* (C) 1991 Linus Torvalds
*/

/*
* demand-loading started 01.12.91 - seems it is high on the list of
* things wanted, and it should be easy to implement. - Linus
*/
/*
* 需求加载是从01.12.91 开始编写的 - 在程序编制表中是呼是最重要的程序，
* 并且应该是很容易编制的 - linus
*/

/*
* Ok, demand-loading was easy, shared pages a little bit tricker. Shared
* pages started 02.12.91, seems to work. - Linus.
*
* Tested sharing by executing about 30 /bin/sh: under the old kernel it
* would have taken more than the 6M I have free, but it worked well as
* far as I could see.
*
* Also corrected some "invalidate()"s - I wasn't doing enough of them.
*/
/*
* OK，需求加载是比较容易编写的，而共享页面却需要有点技巧。共享页面程序是
* 02.12.91 开始编写的，好象能够工作 - Linus。
*
* 通过执行大约30 个/bin/sh 对共享操作进行了测试：在老内核当中需要占用多于
* 6M 的内存，而目前却不用。现在看来工作得很好。
*
* 对"invalidate()"函数也进行了修正 - 在这方面我还做的不够。
*/
#include <signal.h>					/* 信号头文件。定义信号符号常量，信号结构以及信号操作函数原型。	*/
#include <asm/system.h>				/* 系统头文件。定义了设置或修改描述符/中断门等的嵌入式汇编宏。	*/
#include <linux/sched.h>			/* 调度程序头文件，定义了任务结构task_struct、初始任务0 的数据，	*/
									/* 还有一些有关描述符参数设置和获取的嵌入式汇编函数宏语句。	*/
#include <linux/head.h>				/* head 头文件，定义了段描述符的简单结构，和几个选择符常量。	*/
#include <linux/kernel.h>			/* 内核头文件。含有一些内核常用函数的原形定义。	*/


/* 函数名前的关键字volatile用于告诉编译器gcc该函数不会返回。这样可让gcc产生更好一 
 * 些的代码，更重要的是使用这个关键字可以避免产生某些（未初始化变量的）假警告信息。
 */
volatile void do_exit (long code);	/* 进程退出处理函数，在kernel/exit.c，102 行。	*/

/* 显示内存已用完出错信息，并退出。	*/
static inline volatile void oom (void)
{
	printk ("out of memory\n\r");
	do_exit (SIGSEGV);				/* do_exit()应该使用退出代码，这里用了信号值SIGSEGV(11)	*/
}									/* 相同值的出错码含义是“资源暂时不可用”，正好同义。	*/
/* 刷新页变换高速缓冲宏函数。
 * 为了提高地址转换的效率，CPU 将最近使用的页表数据存放在芯片中高速缓冲中。在修改过页表
 * 信息之后，就需要刷新该缓冲区。这里使用重新加载页目录基址寄存器cr3 的方法来进行刷新。
 * 下面eax = 0，是页目录的基址。
 */
#define invalidate() \
__asm__( "movl %%eax,%%cr3":: "a" (0))

/* these are not to be changed without changing head.s etc */
/* 下面定义若需要改动，则需要与head.s 等文件中的相关信息一起改变 */
/* linux 0.11 内核默认支持的最大内存容量是16M，可以修改这些定义以适合更多的内存。	*/
#define LOW_MEM 0x100000					/* 内存低端（1MB）。	*/
#define PAGING_MEMORY (15*1024*1024)		/* 分页内存15MB。主内存区最多15M。	*/
#define PAGING_PAGES (PAGING_MEMORY>>12)	/* 分页后的物理内存页数。	*/
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)	/* 指定内存地址映射为页号。	*/
#define USED 100							/* 页面被占用标志，参见405 行。	*/
/* CODE_SPACE(addr) ((((addr)+0xfff)&~0xfff) < current->start_code + current->end_code)。	*/
/* 该宏用于判断给定线性地址是否位于当前进程的代码段中，“（Gaddr）+4095}&~4095y”用于
 * 取得线性地址addr所在内存页面的末端地址。参见252行。*/
#define CODE_SPACE(addr) ((((addr)+4095)&~4095) < \
current->start_code + current->end_code)

static long HIGH_MEMORY = 0;				/* 全局变量，存放实际物理内存最高端地址。	*/
/* 复制1 页内存（4K 字节）。	*/
#define copy_page(from,to) \
__asm__( "cld ; rep ; movsl":: "S" (from), "D" (to), "c" (1024): "cx", "di", "si")
/* 物理内存映射字节图（1字节代表1页内存）。每个页面对应的字节用于标志页面当前被引用
 * (占用)次数。它最大可以映射15Mb的内存空间。在初始化函数mem—init()中，对于不能用
 * 作主内存区页面的位置均都预先被设置成USED(IOO)。
*/
static unsigned char mem_map[PAGING_PAGES] = { 0, };

/*
* Get physical address of first (actually last :-) free page, and mark it
* used. If no free pages left, return 0.
*/
/* 获取首个(实际上是最后1 个:-)空闲页面，并标记为已使用。如果没有空闲页面，就返回0。*/

/* 取空闲页面。如果已经没有可用内存了，则返回0。
 * 在主内存区中取空闲物理页面。如果已经没有可用物理内存页面，则返回0。
 * 输入：%l(ax=0) - 0;
 * %2(LOW—MEM)内存字节位图管理的起始位置；
 * %3(cx= PAGING—PAGES);
 * %4(edi=mem—map+PAGING—PAGES-l)。
 * 输出：返回%0(3义=物理页面起始地址)。
 * 上面%4寄存器实际指向mem—map□内存字节位图的最后一个字节。本函数从位图末端开始向
 * 前扫描所有页面标志（页面总数为PAGING—PAGES），若有页面空闲（内存位图字节为0）则
 * 返回页面地址。注意！本函数只是指出在主内存区的一页空闲物理页面，但并没有映射到某
 * 个进程的地址空间中去。后面的put—pageO函数即用于把指定页面映射到某个进程的地址
 * 空间中。当然对于内核使用本函数并不需要再使用put—pageO进行映射，因为内核代码和
 * 数据空间（16MB）已经对等地映射到物理地址空间。
 * 第65行定义了一个局部寄存器变量。该变量将被保存在eax寄存器中，以便于高效访问和
 * 操作。这种定义变量的方法主要用于内嵌汇编程序中。详细说明参见gcc手册“在指定寄存
 * 器中的变量”。
 */

unsigned long get_free_page(void)
{
	register unsigned long __res asm("ax");

	__asm__("std ; repne ; scasb\n\t"
			"jne 1f\n\t"
			"movb $1,1(%%edi)\n\t"
			"sall $12,%%ecx\n\t"
			"addl %2,%%ecx\n\t"
			"movl %%ecx,%%edx\n\t"
			"movl $1024,%%ecx\n\t"
			"leal 4092(%%edx),%%edi\n\t"
			"rep ; stosl\n\t"
			"movl %%edx,%%eax\n"
			"1:"
			:"=a" (__res)
			:"0" (0),"i" (LOW_MEM),"c" (PAGING_PAGES),
			"D" (mem_map+PAGING_PAGES-1)
			:"di","cx","dx");
	return __res;							/* 返回空闲页面地址（如果无空闲也则返回0）。	*/
}
/* 方向位置位，将al(0)与对应每个页面的(di)内容比较，	*/
/* 如果没有等于0 的字节，则跳转结束（返回0）。	*/
/* 将对应页面的内存映像位置1。	*/
/* 页面数*4K = 相对页面起始地址。	*/
/* 再加上低端内存地址，即获得页面实际物理起始地址。	*/
/* 将页面实际起始地址?edx 寄存器。	*/
/* 寄存器ecx 置计数值1024。	*/
/* 将4092+edx 的位置?edi(该页面的末端)。	*/
/* 将edi 所指内存清零（反方向，也即将该页面清零）。	*/
/* 将页面起始地址?eax（返回值）。	*/

/*
* Free a page of memory at physical address 'addr'. Used by
* 'free_page_tables()'
*/
/*
* 释放物理地址'addr'开始的一页内存。用于函数'free_page_tables()'。
*/
/* 释放物理地址addr 开始的一页面内存。	*/
/* 物理地址1MB以下的内存空间用于内核程序和缓冲，不作为分配页面的内存空间。因此
 * 参数addr需要大于1MB。*/
void free_page (unsigned long addr)
{
/* 首先判断参数给定的物理地址addr的合理性。如果物理地址addr小于内存低端（1MB），
 * 则表示在内核程序或高速缓冲中，对此不予处理。如果物理地址addr >=系统所含物理
 * 内存最高端，则显示出错信息并且内核停止工作。
 */
	if (addr < LOW_MEM)
		return;							/* 如果物理地址addr 小于内存低端（1MB），则返回。	*/
	if (addr >= HIGH_MEMORY)			/* 如果物理地址addr>=内存最高端，则显示出错信息。	*/
		panic ("trying to free nonexistent page");
/* 如果对参数addr验证通过，那么就根据这个物理地址换算出从内存低端开始计起的内存
 * 页面号。页面号=(addr - L0w—MEM)/4096。可见页面号从0号开始计起。此时addr
 * 中存放着页面号。如果该页面号对应的页面映射字节不等于0，则减1返回。此时该映射
 * 字节值应该为0，表示页面已释放。如果对应页面字节原本就是0，表示该物理页面本来
 * 就是空闲的，说明内核代码出问题。于是显示出错信息并停机。
 */
	addr -= LOW_MEM;					/* 物理地址减去低端内存位置，再除以4KB，得页面号。	*/
	addr >>= 12;
	if (mem_map[addr]--)
		return;							/* 如果对应内存页面映射字节不等于0，则减1 返回。	*/
	mem_map[addr] = 0;					/* 否则置对应页面映射字节为0，并显示出错信息，死机。	*/
	panic ("trying to free free page");
}

/*
* This function frees a continuos block of page tables, as needed
* by 'exit()'. As does copy_page_tables(), this handles only 4Mb blocks.
*/
/*
* 下面函数释放页表连续的内存块，'exit()'需要该函数。与copy_page_tables()
* 类似，该函数仅处理4Mb 的内存块。
*/
/* 根根据指定的线性地址和限长（页表个数），释放对应内存页表指定的内存块并置表项空闲。
 * 页目录位于物理地址0开始处，共1024项，每项4字节，共占4K字节。每个目录项指定一
 * 个页表。内核页表从物理地址0x1000处开始（紧接着目录空间），共4个页表。每个页表有
 * 1024项，每项4字节。因此也占4K(1页)内存。各进程（除了在内核代码中的进程0和1）
 * 的页表所占据的页面在进程被创建时由内核为其在主内存区申请得到。每个页表项对应1页
 * 物理内存，因此一个页表最多可映射4MB的物理内存。
 * 参数：from -起始线性基地址；size -释放的字节长度。
 */
int
free_page_tables (unsigned long from, unsigned long size)
{
	unsigned long *pg_table;
	unsigned long *dir, nr;

	if (from & 0x3fffff)				/* 要释放内存块的地址需以4M 为边界。	*/
		panic ("free_page_tables called with wrong alignment");
	if (!from)							/* 出错，试图释放内核和缓冲所占空间。	*/
		panic ("Trying to free up swapper memory space");
/* 计算所占页目录项数(4M 的进位整数倍)，也即所占页表数。	*/
	size = (size + 0x3fffff) >> 22;
/* 下面一句计算起始目录项。对应的目录项号=from>>22，因每项占4 字节，并且由于页目录是从
 * 物理地址0 开始，因此实际的目录项指针=目录项号<<2，也即(from>>20)。与上0xffc 确保
 * 目录项指针范围有效。
 */
	dir = (unsigned long *) ((from >> 20) & 0xffc);	/* _pg_dir = 0 */
	for (; size-- > 0; dir++)
	{								/* size 现在是需要被释放内存的目录项数。	*/
		if (!(1 & *dir))			/* 如果该目录项无效(P 位=0)，则继续。	*/
			continue;				/* 目录项的位0(P 位)表示对应页表是否存在。	*/
		pg_table = (unsigned long *) (0xfffff000 & *dir);	/* 取目录项中页表地址。	*/
		for (nr = 0; nr < 1024; nr++)
		{							/* 每个页表有1024 个页项。	*/
			if (1 & *pg_table)		/* 若该页表项有效(P 位=1)，则释放对应内存页。	*/
				free_page (0xfffff000 & *pg_table);
			*pg_table = 0;			/* 该页表项内容清零。	*/
			pg_table++;				/* 指向页表中下一项。	*/
		}
			free_page (0xfffff000 & *dir);	/* 释放该页表所占内存页面。但由于页表在	*/
									/* 物理地址1M 以内，所以这句什么都不做。	*/
			*dir = 0;				/* 对相应页表的目录项清零。	*/
	}
	invalidate ();					/* 刷新页变换高速缓冲。	*/
	return 0;
}

/*
* Well, here is one of the most complicated functions in mm. It
* copies a range of linerar addresses by copying only the pages.
* Let's hope this is bug-free, 'cause this one I don't want to debug :-)
*
* Note! We don't copy just any chunks of memory - addresses have to
* be divisible by 4Mb (one page-directory entry), as this makes the
* function easier. It's used only by fork anyway.
*
* NOTE 2!! When from==0 we are copying kernel space for the first
* fork(). Then we DONT want to copy a full page-directory entry, as
* that would lead to some serious memory waste - we just copy the
* first 160 pages - 640kB. Even that is more than we need, but it
* doesn't take any more memory - we don't copy-on-write in the low
* 1 Mb-range, so the pages can be shared with the kernel. Thus the
* special case for nr=xxxx.
*/
/*
* 好了，下面是内存管理mm 中最为复杂的程序之一。它通过只复制内存页面
* 来拷贝一定范围内线性地址中的内容。希望代码中没有错误，因为我不想
* 再调试这块代码了?。
*
* 注意！我们并不是仅复制任何内存块 - 内存块的地址需要是4Mb 的倍数（正好
* 一个页目录项对应的内存大小），因为这样处理可使函数很简单。不管怎样，
* 它仅被fork()使用（fork.c 第56 行）。
*
* 注意2！！当from==0 时，是在为第一次fork()调用复制内核空间。此时我们
* 不想复制整个页目录项对应的内存，因为这样做会导致内存严重的浪费 - 我们
* 只复制头160 个页面 - 对应640kB。即使是复制这些页面也已经超出我们的需求，
* 但这不会占用更多的内存 - 在低1Mb 内存范围内我们不执行写时复制操作，所以
* 这些页面可以与内核共享。因此这是nr=xxxx 的特殊情况（nr 在程序中指页面数）。
*/
/* 复制指定线性地址和长度（页表个数）内存对应的页目录项和页表，从而被复制的页目录和
 * 页表对应的原物理内存区被共享使用。
 * 复制指定地址和长度的内存对应的页目录项和页表项。需申请页面来存放新页表，原内存区被共享；
 * 此后两个进程将共享内存区，直到有一个进程执行写操作时，才分配新的内存页（写时复制机制）。
 */
int
copy_page_tables (unsigned long from, unsigned long to, long size)
{
	unsigned long *from_page_table;
	unsigned long *to_page_table;
	unsigned long this_page;
	unsigned long *from_dir, *to_dir;
	unsigned long nr;

/*首先检测参数给出的源地址from和目的地址to的有效性。源地址和目的地址都需要在4Mb 
内存边界地址上。否则出错死机。作这样的要求是因为一个页表的1024项可管理4Mb内存。 
源地址from和目的地址to只有满足这个要求才能保证从一个页表的第1项开始复制页表 
项，并且新页表的最初所有项都是有效的。然后取得源地址和目的地址的起始目录项指针
(from—dir和to—dir)。再根据参数给出的长度size计算要复制的内存块占用的页表数
(即目录项数)。参见前面对114、115行的解释。*/

	/* 源地址和目的地址都需要是在4Mb 的内存边界地址上。否则出错，死机。	*/
	if ((from & 0x3fffff) || (to & 0x3fffff))
		panic ("copy_page_tables called with wrong alignment");
/* 取得源地址和目的地址的目录项(from_dir 和to_dir)。参见对115 句的注释。	*/
	from_dir = (unsigned long *) ((from >> 20) & 0xffc);	/* _pg_dir = 0 */
	to_dir = (unsigned long *) ((to >> 20) & 0xffc);
/* 计算要复制的内存块占用的页表数（也即目录项数）。	*/
	size = ((unsigned) (size + 0x3fffff)) >> 22;
/*在得到了源起始目录项指针from—dir和目的起始目录项指针to—dir以及需要复制的页表 
个数size后，下面开始对每个页目录项依次申请1页内存来保存对应的页表，并且开始 
页表项复制操作。如果目的目录项指定的页表已经存在（P=l），则出错死机。如果源目 
录项无效，即指定的页表不存在（P=0），则继续循环处理下一个页目录项。
*/
	for (; size-- > 0; from_dir++, to_dir++)
	{
/* 如果目的目录项指定的页表已经存在(P=1)，则出错，死机。	*/
		if (1 & *to_dir)
	panic ("copy_page_tables: already exist");
/* 如果此源目录项未被使用，则不用复制对应页表，跳过。	*/
		if (!(1 & *from_dir))
				continue;
/*在验证了当前源目录项和目的项正常之后，我们取源目录项中页表地址from—page—table。 
为了保存目的目录项对应的页表，需要在主内存区中申请1页空闲内存页。如果取空闲页面 
函数get—free—pageO返回0，则说明没有申请到空闲内存页面，可能是内存不够。于是返 
回-1值退出。
*/
		from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
/* 为目的页表取一页空闲内存，如果返回是0 则说明没有申请到空闲内存页面。返回值=-1，退出。	*/
		if (!(to_page_table = (unsigned long *) get_free_page ()))
			return -1;							/* Out of memory, see freeing */
/*否则我们设置目的目录项信息，把最后3位置位，即当前目的目录项“或”上7，表示对应 
页表映射的内存页面是用户级的，并且可读写、存在（Usr, R/ff, Present）。（如果U/S 
位是0，则R/W就没有作用。如果U/S是1，而R/W是0，那么运行在用户层的代码就只能 
读页面。如果U/S和R/W都置位，则就有读写的权限）。然后针对当前处理的页目录项对应 
的页表，设置需要复制的页面项数。如果是在内核空间，则仅需复制头160页对应的页表项 
 (nr = 160)，对应于开始640KB物理内存。否则需要复制一个页表中的所有1024个页表项 
 (nr = 1024) ’可映射4MB物理内存。
*/
		*to_dir = ((unsigned long) to_page_table) | 7;
	/* 针对当前处理的页表，设置需复制的页面数。如果是在内核空间，则仅需复制头160 页，否则需要	*/
/* 复制1 个页表中的所有1024 页面。	*/
		nr = (from == 0) ? 0xA0 : 1024;
/*此时对于当前页表，开始循环复制指定的nr个内存页面表项。先取出源页表项内容，如果 
当前源页面没有使用，则不用复制该表项，继续处理下一项。否则复位页表项中R/W标志 
(位1置0)，即让页表项对应的内存页面只读。然后将该页表项复制到目的页表中。
*/
		for (; nr-- > 0; from_page_table++, to_page_table++)
		{
			this_page = *from_page_table;		/* 取源页表项内容。	*/
			if (!(1 & this_page))				/* 如果当前源页面没有使用，则不用复制。	*/
				continue;
/* 复位页表项中R/W 标志(置0)。(如果U/S 位是0，则R/W 就没有作用。如果U/S 是1，而R/W 是0，	*/
/* 那么运行在用户层的代码就只能读页面。如果U/S 和R/W 都置位，则就有写的权限。)	*/
			this_page &= ~2;
			*to_page_table = this_page;			/* 将该页表项复制到目的页表中。	*/
/* 如果该页表项所指页面的地址在1M 以上，则需要设置内存页面映射数组mem_map[]，于是计算	*/
/* 页面号，并以它为索引在页面映射数组相应项中增加引用次数。	*/
/*如果该页表项所指物理页面的地址在1MB以上，则需要设置内存页面映射数组mem—map[]，
于是计算页面号，并以它为索引在页面映射数组相应项中增加引用次数。而对于位于1MB 
以下的页面，说明是内核页面，因此不需要对mem—map □进行设置。因为mem—map □仅用 
于管理主内存区中的页面使用情况。因此对于内核移动到任务0中并且调用fork()创建 
任务1时（用于运行init()），由于此时复制的页面还仍然都在内核代码区域，因此以下 
判断中的语句不会执行，任务0的页面仍然可以随时读写。只有当调用fork()的父进程 
代码处于主内存区（页面位置大于1MB）时才会执行。这种情况需要在进程调用execveO，
并装载执行了新程序代码时才会出现。
180行语句含义是令源页表项所指内存页也为只读。因为现在开始有两个进程共用内存区了。 
若其中1个进程需要进行写操作，则可以通过页异常写保护处理为执行写操作的进程分配 
1页新空闲页面，也即进行写时复制（copy on write）操作。
*/
			if (this_page > LOW_MEM)
			{
/* 下面这句的含义是令源页表项所指内存页也为只读。因为现在开始有两个进程共用内存区了。	*/
/* 若其中一个内存需要进行写操作，则可以通过页异常的写保护处理，为执行写操作的进程分配	*/
/* 一页新的空闲页面，也即进行写时复制的操作。	*/
				*from_page_table = this_page;	/* 令源页表项也只读。	*/
				this_page -= LOW_MEM;
				this_page >>= 12;
				mem_map[this_page]++;
			}
		}
	}
	invalidate ();								/* 刷新页变换高速缓冲。	*/
	return 0;
}

/*
* This function puts a page in memory at the wanted address.
* It returns the physical address of the page gotten, 0 if
* out of memory (either when trying to access page-table or
* page.)
*/
/*
* 下面函数将一内存页面放置在指定地址处。它返回页面的物理地址，如果
* 内存不够(在访问页表或页面时)，则返回0。
*/
/*把一物理内存页面映射到线性地址空间指定处。
II或者说是把线性地址空间中指定地址address处的页面映射到主内存区页面page上。主要 
工作是在相关页目录项和页表项中设置指定页面的信息。若成功则返回物理页面地址。在 
处理缺页异常的C函数do—no—pageO中会调用此函数。对于缺页引起的异常，由于任何缺 
页缘故而对页表作修改时，并不需要刷新CPU的页变换缓冲（或称Translation Lookaside
Buffer - TLB）,即使页表项中标志P被从0修改成1。因为无效页项不会被缓冲，因此当
修改了一个无效的页表项时不需要刷新。在此就表现为不用调用InvalidateO函数。
参数page是分配的主内存区中某一页面（页帧，页框）的指针；address是线性地址。
*/
unsigned long
put_page (unsigned long page, unsigned long address)
{
	unsigned long tmp, *page_table;

/* NOTE !!! This uses the fact that _pg_dir=0 */
/* 注意!!!这里使用了页目录基址_pg_dir=0 的条件 */

/*首先判断参数给定物理内存页面page的有效性。如果该页面位置低于L0W—MEM(1MB)或 
超出系统实际含有内存高端HIGH—MEMORY，贝拨出警告。L0W—MEM是主内存区可能有的最 
小起始位置。当系统物理内存小于或等于6MB时，主内存区起始于L0W—MEM处。再查看一 
下该page页面是否是已经申请的页面，即判断其在内存页面映射字节图mem—map□中相 
应字节是否已经置位。若没有则需发出警告。	
*/
	if (page < LOW_MEM || page >= HIGH_MEMORY)
		printk ("Trying to put page %p at %p\n", page, address);
/* 如果申请的页面在内存页面映射字节图中没有置位，则显示警告信息。	*/
	if (mem_map[(page - LOW_MEM) >> 12] != 1)
		printk ("mem_map disagrees with %p at %p\n", page, address);
/*然后根据参数指定的线性地址address计算其在页目录表中对应的目录项指针，并从中取得
二级页表地址。如果该目录项有效（P=l），即指定的页表在内存中，则从中取得指定页表 
地址放到page—table变量中。否则就申请一空闲页面给页表使用，并在对应目录项中置相 
应标志（7 - User、U/S、R/W）。然后将该页表地址放到page—table变量中。参见对115 
行语句的说明。
*/
	page_table = (unsigned long *) ((address >> 20) & 0xffc);
/* 如果该目录项有效(P=1)(也即指定的页表在内存中)，则从中取得指定页表的地址——>page_table。	*/
	if ((*page_table) & 1)
		page_table = (unsigned long *) (0xfffff000 & *page_table);
	else
	{
/* 否则，申请空闲页面给页表使用，并在对应目录项中置相应标志7（User, U/S, R/W）。然后将	*/
/* 该页表的地址——>page_table。	*/
		if (!(tmp = get_free_page ()))
			return 0;
		*page_table = tmp | 7;
		page_table = (unsigned long *) tmp;
	}
/*最后在找到的页表page—table中设置相关页表项内容，即把物理页面page的地址填入表 
项同时置位3个标志（UA、W/R、P）。该页表项在页表中的索引值等于线性地址位21 — 
位12组成的10比特的值。每个页表共可有1024项（0 -- 0x3ff）。
*/
	page_table[(address >> 12) & 0x3ff] = page | 7;
/* no need for invalidate */
/* 不需要刷新页变换高速缓冲 */
	return page;			/* 返回页面地址。	*/
}

/*取消写保护页面函数。用于页异常中断过程中写保护异常的处理（写时复制）。
在内核创建进程时，新进程与父进程被设置成共享代码和数据内存页面，并且所有这些页面 
均被设置成只读页面。而当新进程或原进程需要向内存页面写数据时，CPU就会检测到这个 
情况并产生页面写保护异常。于是在这个函数中内核就会首先判断要写的页面是否被共享。 
若没有则把页面设置成可写然后退出。若页面是出于共享状态，则需要重新申请一新页面并
复制被写页面内容，以供写进程单独使用。共享被取消。本函数供下面do—wp—pageO调用。 
输入参数为页表项指针，是物理地址。[un—wp—page — Un-Write Protect Page]
*/
/* 输入参数为页表项指针。	*/
/* [ un_wp_page 意思是取消页面的写保护：Un-Write Protected。]	*/
void un_wp_page (unsigned long *table_entry)
{
	unsigned long old_page, new_page;
/* 首先取参数指定的页表项中物理页面位置（地址）并判断该页面是否是共享页面。如果原 
 * 页面地址大于内存低端LOW—Mffl (表示在主内存区中)，并且其在页面映射字节图数组中 
 * 值为1(表示页面仅被引用1次，页面没有被共享)，则在该页面的页表项中置R/W标志 
 * (可写)，并刷新页变换高速缓冲，然后返回。即如果该内存页面此时只被一个进程使用， 
 * 并且不是内核中的进程，就直接把属性改为可写即可，不用再重新申请一个新页面。
 */
	old_page = 0xfffff000 & *table_entry;	/* 取指定页表项中物理页面地址。	*/
/* 如果原页面地址大于内存低端LOW_MEM(1Mb)，并且其在页面映射字节图数组中值为1（表示仅
 * 被引用1 次，页面没有被共享），则在该页面的页表项中置R/W 标志（可写），并刷新页变换
 * 高速缓冲，然后返回。	
 */
	if (old_page >= LOW_MEM && mem_map[MAP_NR (old_page)] == 1)
	{
		*table_entry |= 2;
		invalidate ();
		return;
	}
/*否则就需要在主内存区内申请一页空闲页面给执行写操作的进程单独使用，取消页面共享。 
如果原页面大于内存低端（则意味着mem—map[] > 1，页面是共享的），则将原页面的页 
面映射字节数组值递减1。然后将指定页表项内容更新为新页面地址，并置可读写等标志 
(U/S、R/W、P)。在刷新页变换高速缓冲之后，最后将原页面内容复制到新页面上。
*/
	if (!(new_page = get_free_page ()))
		oom ();			/* Out of Memory。内存不够处理。	*/
/* 如果原页面大于内存低端（则意味着mem_map[]>1，页面是共享的），则将原页面的页面映射
 * 数组值递减1。然后将指定页表项内容更新为新页面的地址，并置可读写等标志(U/S, R/W, P)。
 * 刷新页变换高速缓冲。最后将原页面内容复制到新页面。
 */
	if (old_page >= LOW_MEM)
		mem_map[MAP_NR (old_page)]--;
	*table_entry = new_page | 7;
	invalidate ();
	copy_page (old_page, new_page);
}

/*
* This routine handles present pages, when users try to write
* to a shared page. It is done by copying the page to a new address
* and decrementing the shared-page counter for the old page.
*
* If it's in code space we exit with a segment error.
*/
/*
* 当用户试图往一个共享页面上写时，该函数处理已存在的内存页面，（写时复制）
* 它是通过将页面复制到一个新地址上并递减原页面的共享页面计数值实现的。
*
* 如果它在代码空间，我们就以段错误信息退出。
*/
/* 执行写保护页面处理。
是写共享页面处理函数。是页异常中断处理过程中调用的C函数。在page.s程序中被调用。 
参数error—code是进程在写写保护页面时由CPU自动产生，address是页面线性地址。
写共享页面时，需复制页面（写时复制）。
 */
void
do_wp_page (unsigned long error_code, unsigned long address)
{
#if 0
/* we cannot do this yet: the estdio library writes to code space */
/* stupid, stupid. I really want the libc.a from GNU */
/* 我们现在还不能这样做：因为estdio 库会在代码空间执行写操作 */
/* 真是太愚蠢了。我真想从GNU 得到libc.a 库。*/
	if (CODE_SPACE (address))	/* 如果地址位于代码空间，则终止执行程序。	*/
		do_exit (SIGSEGV);
#endif
/*调用上面函数un—wp—pageO来处理取消页面保护。但首先需要为其准备好参数。参数是 
线性地址address指定页面在页表中的页表项指针，其计算方法是：
①（(address>>10) & Oxffc）：计算指定线性地址中页表项在页表中的偏移地址；因为 
根据线性地址结构，（address>>12）就是页表项中的索引，但每项占4个字节，因此乘 4后：
（address>>12）<<2 = (address?10)&0xffc就可得到页表项在表中的偏移地址。
与操作&0xffc用于限制地址范围在一个页面内。又因为只移动了 10位，因此最后2位 
是线性地址低12位中的最高2位，也应屏蔽掉。因此求线性地址中页表项在页表中偏 
移地址直观一些的表示方法是（（(address>>12) & 0x3ff）<<2 ）。
②(OxfffffOOO & *((address?20) &0xffc)):用于取目录项中页表的地址值；其中，
 ((address>>20) &0xffc)用于取线性地址中的目录索引项在目录表中的偏移位置。因为 
address>>22是目录项索引值，但每项4个字节，因此乘以4后：(address>>22)?2 
= (address>>20)就是指定项在目录表中的偏移地址。feOxffc用于屏蔽目录项索引值 
中最后2位。因为只移动了 20位，因此最后2位是页表索引的内容，应该屏蔽掉。而 
 *((address>>20) &0xffc)则是取指定目录表项内容中对应页表的物理地址。最后与上 
 OxffffffOOO用于屏蔽掉页目录项内容中的一些标志位（目录项低12位）。直观表示为 
  (OxffffffOOO & ^((unsigned long *) (((address>>22) & 0x3ff)<<2)))。
③由①中页表项在页表中偏移地址加上②中目录表项内容中对应页表的物理地址即可 
得到页表项的指针（物理地址）。这里对共享的页面进行复制。
*/
	un_wp_page((unsigned long *)
	   (((address>>10) & 0xffc) + (0xfffff000 &
	   *((unsigned long *) ((address>>20) &0xffc)))));

}

/* 写页面验证。	*/
/* 若页面不可写，则复制页面。在fork.c 第34 行被调用。	*/
void
write_verify (unsigned long address)
{
	unsigned long page;
/*首先取指定线性地址对应的页目录项，根据目录项中的存在位（P）判断目录项对应的页表 
是否存在（存在位P=l?），若不存在（P=0）则返回。这样处理是因为对于不存在的页面没 
有共享和写时复制可言，并且若程序对此不存在的页面执行写操作时，系统就会因为缺页异 
常而去执行do—no—page()，并为这个地方使用put—page()函数映射一个物理页面。
接着程序从目录项中取页表地址，加上指定页面在页表中的页表项偏移值，得对应地址的页 
表项指针。在该表项中包含着给定线性地址对应的物理页面。
*/
	if (!((page = *((unsigned long *) ((address >> 20) & 0xffc))) & 1))
		return;
/* 取页表的地址，加上指定地址的页面在页表中的页表项偏移值，得对应物理页面的页表项指针。	*/
	page &= 0xfffff000;
	page += ((address >> 10) & 0xffc);
/*然后判断该页表项中的位1(R/W)、位0 (P)标志。如果该页面不可写（R/W=0）且存在， 
那么就执行共享检验和复制页面操作（写时复制）。否则什么也不做，直接退出。
*/
	if ((3 & *(unsigned long *) page) == 1)	/* non-writeable, present */
		un_wp_page ((unsigned long *) page);
	return;
}

/* 取得一页空闲内存并映射到指定线性地址处。	*/
/* 与get_free_page()不同。get_free_page()仅是申请取得了主内存区的一页物理内存。而该函数	*/
/* 不仅是获取到一页物理内存页面，还进一步调用put_page()，将物理页面映射到指定的线性地址处。	*/
void
get_empty_page (unsigned long address)
{
	unsigned long tmp;

/* 若不能取得一空闲页面，或者不能将页面放置到指定地址处，则显示内存不够的信息。
 * 279 行上英文注释的含义是：即使执行get_free_page()返回0 也无所谓，因为put_page()
 * 中还会对此情况再次申请空闲物理页面的，见210 行。
 */
	if (!(tmp = get_free_page ()) || !put_page (tmp, address))
	{
		free_page (tmp);		/* 0 is ok - ignored */
		oom ();
	}
}

/*
* try_to_share() checks the page at address "address" in the task "p",
* to see if it exists, and if it is clean. If so, share it with the current
* task.
*
* NOTE! This assumes we have checked that p != current, and that they
* share the same executable.
*/
/*
* try_to_share()在任务"p"中检查位于地址"address"处的页面，看页面是否存在，是否干净。
* 如果是干净的话，就与当前任务共享。
*
* 注意！这里我们已假定p !=当前任务，并且它们共享同一个执行程序。
*/
/*尝试对当前进程指定地址处的页面进行共享处理。
当前进程与进程P是同一执行代码，也可以认为当前进程是由P进程执行fork操作产生的 
进程，因此它们的代码内容一样。如果未对数据段内容作过修改那么数据段内容也应一样。 
参数address是进程中的逻辑地址，即是当前进程欲与p进程共享页面的逻辑页面地址。 
进程P是将被共享页面的进程。如果p进程address处的页面存在并且没有被修改过的话， 
就让当前进程与P进程共享之。同时还需要验证指定的地址处是否已经申请了页面，若是 
则出错，死机。返回：1 -页面共享处理成功；0 -失败。
*/
static int
try_to_share (unsigned long address, struct task_struct *p)
{
	unsigned long from;
	unsigned long to;
	unsigned long from_page;
	unsigned long to_page;
	unsigned long phys_addr;
/* 首先分别求得指定进程P中和当前进程中逻辑地址address对应的页目录项。为了计算方便 
先求出指定逻辑地址address处的’逻辑’页目录项号，即以进程空间（0 - 64MB）算出的页 
目录项号。该’逻辑’页目录项号加上进程P在CPU 4G线性空间中起始地址对应的页目录项， 
即得到进程p中地址address处页面所对应的4G线性空间中的实际页目录项fwm—page。 
而’逻辑’页目录项号加上当前进程CPU 4G线性空间中起始地址对应的页目录项，即可最后 
得到当前进程中地址address处页面所对应的4G线性空间中的实际页目录项to—page。*/
	from_page = to_page = ((address >> 20) & 0xffc);
/* 计算进程p 的代码起始地址所对应的页目录项。	*/
	from_page += ((p->start_code >> 20) & 0xffc);		/* P进程中代码起始地址所对应的页目录项。	*/
	to_page += ((current->start_code >> 20) & 0xffc);	/* 当前进程中代码起始地址所对应的页目录项。	*/
/* 在得到P进程和当前进程address对应的目录项后，下面分别对进程p和当前进程进行处理。 
下面首先对P进程的表项进行操作。目标是取得P进程中address对应的物理内存页面地址， 
并且该物理页面存在，而且干净（没有被修改过，不脏）。
方法是先取目录项内容。如果该目录项无效（P=0），表示目录项对应的二级页表不存在，
于是返回。否则取该目录项对应页表地址from,从而计算出逻辑地址address对应的页表项 
指针，并取出该页表项内容临时保存在phys_addr中。*/

/* is there a page-directory at from? */
/* 在from 处是否存在页目录项？*/
/* 对p 进程页面进行操作。取页目录项内容。如果该目录项无效(P=0)，则返回。否则取该目录项对应页表地址?from。	*/
	from = *(unsigned long *) from_page;		/* p 进程目录项内容。	*/
	if (!(from & 1))
		return 0;
	from &= 0xfffff000;
/* 计算地址对应的页表项指针值，并取出该页表项内容?phys_addr。	*/
	from_page = from + ((address >> 10) & 0xffc);	/* 页表项指针。	*/
	phys_addr = *(unsigned long *) from_page;		/* 页表项内容。	*/
/* is the page clean and present? */
/* 页面干净并且存在吗？*/
/* 接着看看页表项映射的物理页面是否存在并且干净。0x41对应页表项中的D (Dirty)和 
P(PreSent)标志。如果页面不干净或无效则返回。然后我们从该表项中取出物理页面地址 
再保存在phys—addr中。最后我们再检查一下这个物理页面地址的有效性，即它不应该超过 
机器最大物理地址值，也不应该小于内存低端（1MB）。	*/
	if ((phys_addr & 0x41) != 0x01)
		return 0;
/* 取页面的地址——>hys_addr。如果该页面地址不存在或小于内存低端(1M)也返回退出。	*/
	phys_addr &= 0xfffff000;						/* 物理页面地址。	*/
	if (phys_addr >= HIGH_MEMORY || phys_addr < LOW_MEM)
		return 0;
/* 下面首先对当前进程的表项进行操作。目标是取得当前进程中address对应的页表项地址，
 * 并且该页表项还没有映射物理页面，即其P=0。
 * 首先取当前进程页目录项内容+to。如果该目录项无效（P=0），即目录项对应的二级页表
 * 不存在，贝彳申请一空闲页面来存放页表，并更新目录项to—page内容，让其指向该内存页面。
 */
	to = *(unsigned long *) to_page;
	if (!(to & 1))
		if (to = get_free_page ())
			*(unsigned long *) to_page = to | 7;
		else
			oom ();
/* 否则取目录项中的页表±—+t0，加上页表项索引值<<2，即页表项在表中偏移地址，得到 
 * 页表项地址+to—page。针对该页表项，如果此时我们检查出其对应的物理页面已经存在，
 * 即页表项的存在位P=l，贝B兑明原本我们想共享进程P中对应的物理页面，但现在我们自己 
 * 已经占有了（映射有）物理页面。于是说明内核出错，死机。
*/
	to &= 0xfffff000;
	to_page = to + ((address >> 10) & 0xffc);
	if (1 & *(unsigned long *) to_page)
		panic ("try_to_share: to_page already exists");
/* 在找到了进程p中逻辑地址address处对应的干净且存在的物理页面，而且也确定了当前 
 * 进程中逻辑地址address所对应的二级页表项地址之后，我们现在对他们进行共享处理。
 * 方法很简单，就是首先对P进程的页表项进行修改，设置其写保护（R/W=0，只读）标志，
 * 然后让当前进程复制P进程的这个页表项。此时当前进程逻辑地址address处页面即被 
 * 映射到P进程逻辑地址address处页面映射的物理页面上。
*/

/* share them: write-protect */
/* 对它们进行共享处理：写保护 */
/* 对p 进程中页面置写保护标志(置R/W=0 只读)。并且当前进程中的对应页表项指向它。	*/
	*(unsigned long *) from_page &= ~2;
	*(unsigned long *) to_page = *(unsigned long *) from_page;
/* 随后刷新页变换高速缓冲。计算所操作物理页面的页面号，并将对应页面映射字节数组项中 
 * 的引用递增1。最后返回1，表示共享处理成功。*/
	invalidate ();
/* 计算所操作页面的页面号，并将对应页面映射数组项中的引用递增1。	*/
	phys_addr -= LOW_MEM;
	phys_addr >>= 12;					/* 得页面号。	*/
	mem_map[phys_addr]++;
	return 1;
}

/*
* share_page() tries to find a process that could share a page with
* the current one. Address is the address of the wanted page relative
* to the current data space.
*
* We first check if it is at all feasible by checking executable->i_count.
* It should be >1 if there are other tasks sharing this inode.
*/
/*
* share_page()试图找到一个进程，它可以与当前进程共享页面。参数address 是
* 当前数据空间中期望共享的某页面地址。
*
* 首先我们通过检测executable->i_count 来查证是否可行。如果有其它任务已共享
* 该inode，则它应该大于1。
*/
/* 共享页面处理。
 * 在发生缺页异常时，首先看看能否与运行同一个执行文件的其他进程进行页面共享处理。
 * 该函数首先判断系统中是否有另一个进程也在运行当前进程一样的执行文件。若有，则在 
 * 系统当前所有任务中寻找这样的任务。若找到了这样的任务就尝试与其共享指定地址处的 
 * 页面。若系统中没有其他任务正在运行与当前进程相同的执行文件，那么共享页面操作的 
 * 提条件不存在，因此函数立刻退出。判断系统中是否有另一个进程也在执行同一个执行 
 * 文件的方法是利用进程任务数据结构中的executable字段。该字段指向进程正在执行程 
 * 序在内存中的i节点。根据该i节点的引用次数i—count我们可以进行这种判断。若 
 * executable-〉i—count值大于1，则表明系统中可能有两个进程在运行同一个执行文件，
 * 于是可以再对任务结构数组中所有任务比较是否有相同的executable字段来最后确定多 
 * 个进程运行着相同执行文件的情况。
 * 参数address是进程中的逻辑地址，即是当前进程欲与p进程共享页面的逻辑页面地址。
 * 返回1 -共享操作成功，0 -失败。
*/
static int
share_page (unsigned long address)
{
	struct task_struct **p;

/* 首先检查一下当前进程的executable字段是否指向某执行文件的i节点，以判断本进程 
 * 是否有对应的执行文件。如果没有，则返回0。如果executable的确指向某个i节点，
 * 则检查该i节点引用计数值。如果当前进程运行的执行文件的内存i节点引用计数等于
 * l(executable-)i—count =1}，表示当前系统中只有1个进程（即当前进程）在运行该 
 * 执行文件。因此无共享可言，直接退出函数。
*/
	if (!current->executable)
		return 0;
/* 如果只能单独执行(executable->i_count=1)，也退出。	*/
	if (current->executable->i_count < 2)
		return 0;
/* 否则搜索任务数组中所有任务。寻找与当前进程可共享页面的进程，即运行相同执行文件 
 * 的另一个进程，并尝试对指定地址的页面进行共享。如果找到某个进程p，其executable 
 * 字段值与当前进程的相同，则调用try_to_shareO尝试页面共享。若共享操作成功，则 
 * 函数返回1。否则返回0，表示共享页面操作失败。
*/
	for (p = &LAST_TASK; p > &FIRST_TASK; --p)
	{
		if (!*p)						/* 如果该任务项空闲，则继续寻找。	*/
			continue;
		if (current == *p)				/* 如果就是当前任务，也继续寻找。	*/
			continue;
		if ((*p)->executable != current->executable)
/* 如果executable不等，表示运行的不是与当前进程相同的执行文件，因此也继续寻找。*/
			continue;
		if (try_to_share (address, *p))	/* 尝试共享页面。	*/
			return 1;
	}
	return 0;
}

/* 是访问不存在页面处理函数。页异常中断处理过程中调用的函数。在page.s程序中被调用 
函数参数error_code和address是进程在访问页面时由CPU因缺页产生异常而自动生成
error—code指出出错类型，参见本章开始处的“内存页面出错异常” 一节；address是产召
异常的页面线性地址。
该函数首先尝试与已加载的相同文件进行页面共享，或者只是由于进程动态申请内存页面fl
只需映射一页物理内存页即可。若共享操作不成功，那么只能从相应文件中读入所缺的数据 
页面到指定线性地址处。
*/
void
do_no_page (unsigned long error_code, unsigned long address)
{
	int nr[4];
	unsigned long tmp;
	unsigned long page;
	int block, i;

	address &= 0xfffff000;	/* 页面地址。	*/
/* 首先取线性空间中指定地址address处页面地址。从而可算出指定线性地址在进程空间中
 * 相对于进程基址的偏移长度值tmp，即对应的逻辑地址。*/
	tmp = address - current->start_code;
/* 若当前进程的executable节点指针空，或者指定地址超出(代码+数据)长度，则申请
 * 一页物理内存，并映射到指定的线性地址处。executable是进程正在运行的执行文件的i 
 * 节点结构。由于任务0和任务1的代码在内核中，因此任务0、任务1以及任务1派生的 
 * 没有调用过execveO的所有任务的executable都为0。若该值为0，或者参数指定的线性 
 * 地址超出代码加数据长度，则表明进程在申请新的内存页面存放堆或栈中数据。因此直接 
 * 调用取空闲页面函数get—empty—pageO为进程申请一页物理内存并映射到指定线性地址 
 * 处。进程任务结构字段start—code是线性地址空间中进程代码段地址，字段end—data 
 * 是代码加数据长度。对于Linux0.11内核，它的代码段和数据段起始基址相同。
 */
	if (!current->executable || tmp >= current->end_data)
		{
			get_empty_page (address);
			return;
		}
/* 否则说明所缺页面在进程执行影像文件范围内，于是就尝试共享页面操作，若成功则退出。 
若不成功就只能申请一页物理内存页面page，然后从设备上读取执行文件中的相应页面并 
放置（映射）到进程页面逻辑地址tmp处。	*/
	if (share_page (tmp))				/* 尝试逻辑地址tmp处页面的共享。	*/
		return;
/* 取空闲页面，如果内存不够了，则显示内存不够，终止进程。	*/
	if (!(page = get_free_page ()))		/* 申请一页物理内存。	*/
		oom ();
/* remember that 1 block is used for header */
/* 记住，（程序）头要使用1 个数据块 */
/* 因为块设备上存放的执行文件映像第1块数据是程序头结构，因此在读取该文件时需要跳S 
 * 第1块数据。所以需要首先计算缺页所在的数据块号。因为每块数据长度为BLOCK_SIZE = 1KB，
 * 因此一页内存可存放4个数据块。进程逻辑地址tmp除以数据块大小再加1即可得出缺少的页面在执
 * 行映像文件中的起始块号block。根据这个块号和执行文件的i节点，我们就可以从映射位图中找
 * 到对应块设备中对应的设备逻辑块号（保存在nr □数组中）。利用 bread—page()即可把这4个
 * 逻辑块读入到物理页面page中。	*/
	block = 1 + tmp / BLOCK_SIZE;			/* 设备上对应的逻辑块号。	*/
/* 根据i 节点信息，取数据块在设备上的对应的逻辑块号。	*/
	for (i = 0; i < 4; block++, i++)		/* 读设备上 4 个逻辑块。	*/
		nr[i] = bmap (current->executable, block);
/* 读设备上一个页面的数据（4 个逻辑块）到指定物理地址page 处。	*/
	bread_page (page, current->executable->i_dev, nr);
/* 在读设备逻辑块操作时，可能会出现这样一种情况，即在执行文件中的读取页面位置可能离
 * 文件尾不到1个页面的长度。因此就可能读入一些无用的信息。下面的操作就是把这部分超
 * 出执行文件end—data以后的部分清零处理。	*/
	i = tmp + 4096 - current->end_data;		/* 超出的字节长度值。	*/
	tmp = page + 4096;						/* tmp 指向页面末端。	*/
	while (i-- > 0)							/* 页面末端i字节清零。	*/
	{
		tmp--;
		*(char *) tmp = 0;
	}
/* 最后把引起缺页异常的一页物理页面映射到指定线性地址address处。若操作成功就返回。 
 * 否则就释放内存页，显示内存不够。	*/
	if (put_page (page, address))
		return;
	free_page (page);
	oom ();
}

/* 物理内存管理初始化。
 * 该函数对1MB以上内存区域以页面为单位进行管理前的初始化设置工作。一个页面长度为
 * 4KB字节。该函数把1MB以上所有物理内存划分成一个个页面，并使用一个页面映射字节
 * 数组mem—map[]来管理所有这些页面。对于具有16MB内存容量的机器，该数组共有3840
 * 项（（16MB - 1MB）/4KB），即可管理3840个物理页面。每当一个物理内存页面被占用时就
 * 把mem—map□中对应的的字节值增1;若释放一个物理页面，就把对应字节值减1。若字
 * 节值为0，则表示对应页面空闲；若字节值大于或等于1，则表示对应页面被占用或被不
 * 同程序共享占用。
 * 在该版本的Linux内核中，最多能管理16MB的物理内存，大于16MB的内存将弃置不用。
 * 对于具有16MB内存的PC机系统，在没有设置虚拟盘RAMDISK的情况下start—mem通常
 * 是4MB，end—mem是16MB。因此此时主内存区范围是4MB—16MB，共有3072个_理页面可
 * 供分配。而范围0 - 1MB内存空间用于内核系统（其实内核只使用0 —640Kb，剩下的部
 * 分被部分高速缓冲和设备内存占用）。
 * 参数start—mem是可用作页面分配的主内存区起始地址（已去除RAMDISK所占内存空间）。
 * end—mem是实际物理内存最大地址。而地址范围start_mem到end_mem是主内存区。
 */
void
mem_init (long start_mem, long end_mem)
{
	int i;
/* 首先将1MB到16MB范围内所有内存页面对应的内存映射字节数组项置为已占用状态，即各
 * 项字节值全部设置成 USED(100)?PAGING—PAGES 被定义为(PAGING—MEM0RY>>12)，即 1MB 
 * 以上所有物理内存分页后的内存页面数（15MB/4KB = 3840）。
 */
	HIGH_MEMORY = end_mem;				/* 设置内存最高端。	*/
	for (i = 0; i < PAGING_PAGES; i++)	/* 首先置所有页面为已占用(USED=100)状态，	*/
		mem_map[i] = USED;				/* 即将页面映射数组全置成USED。	*/
/* 然后计算主内存区起始内存处页面对应内存映射字节数组中项号i和主内存区
 * 页面数。此时mem—map[]数组的第i项正对应主内存区中第1个页面。最后将主内存区中
 * 页面对应的数组项清零（表示空闲）。对于具有16MB物理内存的系统，mem—map[]中对应
 * 4Mb—16Mb主内存区的项被清零。
 */
	i = MAP_NR (start_mem);				/* 然后计算可使用起始内存的页面号。	*/
	end_mem -= start_mem;				/* 再计算可分页处理的内存块大小。	*/
	end_mem >>= 12;						/* 从而计算出可用于分页处理的页面数。	*/
	while (end_mem-- > 0)				/* 最后将这些可用页面对应的页面映射数组清零。	*/
		mem_map[i++] = 0;
}

/* 计算内存空闲页面数并显示。	*/
/* [内核中没有地方调用该函数，Linus调试过程中用的]	*/
void
calc_mem (void)
{
	int i, j, k, free = 0;
	long *pg_tbl;

/* 扫描内存页面映射数组mem—map[]，获取空闲页面数并显示。然后扫描所有页目录项（除0，
 * 1项），如果页目录项有效，则统计对应页表中有效页面数，并显示。页目录项0—3被内核
 * 使用，因此应该从第5个目录项（i=4）开始扫描。
*/
	for (i = 0; i < PAGING_PAGES; i++)
		if (!mem_map[i])
			free++;
	printk ("%d pages free (of %d)\n\r", free, PAGING_PAGES);
/* 扫描所有页目录项（除0，1 项），如果页目录项有效，则统计对应页表中有效页面数，并显示。	*/
	for(i=2 ; i<1024 ; i++) {
		if (1&pg_dir[i]) {
			pg_tbl=(long *) (0xfffff000 & pg_dir[i]);
			for(j=k=0 ; j<1024 ; j++)
				if (pg_tbl[j]&1)
					k++;
			printk("Pg-dir[%d] uses %d pages\n",i,k);
		}
	}
}
