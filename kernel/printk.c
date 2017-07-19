/*
* linux/kernel/printk.c
*
* (C) 1991 Linus Torvalds
*/

/*
* When in kernel-mode, we cannot use printf, as fs is liable to
* point to 'interesting' things. Make a printf with fs-saving, and
* all is well.
*/
/*
* 当处于内核模式时，我们不能使用printf，因为寄存器fs 指向其它不感兴趣的地方。
* 自己编制一个printf 并在使用前保存fs，一切就解决了。
*/
#include <stdarg.h>			/* 标准参数头文件。以宏的形式定义变量参数列表。主要说明了一个型
							* (va_list)和三个宏(va_start, va_arg 和va_end)，用于
							* vsprintf、vprintf、vfprintf 函数。	*/
#include <stddef.h>			/* 标准定义头文件。定义了NULL, offsetof(TYPE, MEMBER)。	*/
#include <linux/kernel.h>	/* 内核头文件。含有一些内核常用函数的原形定义。	*/
static char buf[1024];

/* 下面该函数vsprintf()在linux/kernel/vsprintf.c 中92 行开始。	*/
extern int vsprintf (char *buf, const char *fmt, va_list args);

/* 内核使用的显示函数。
 * 只能在内核代码中使用。由于实际调用的显示函数tty—writeO默认使用的显示数据在段寄
 * 存器fs所指向的用户数据区中，因此这里需要暂时保存fs，并让fs指向内核数据段。在显
 * 示完之后再恢复原fs段的内容。
 */
int
printk (const char *fmt, ...)
{
	va_list args;					/* va_list 实际上是一个字符指针类型。	*/
	int i;

/* 运行参数处理开始函数。然后使用格式串fmt将参数列表args输出到buf中。返回值i等于
 * 输出字符串的长度。再运行参数处理结束函数。	*/
	va_start (args, fmt);			/* 在（include/stdarg.h,13）	*/
	i = vsprintf (buf, fmt, args);	/* 使用格式串fmt 将参数列表args 输出到buf 中。	*/
/* 返回值i 等于输出字符串的长度。	*/
	va_end (args);					/* 参数处理结束函数。	*/

	__asm__("push %%fs\n\t"
			"push %%ds\n\t"
			"pop %%fs\n\t"
			"pushl %0\n\t"
			"pushl $_buf\n\t"
			"pushl $0\n\t"
			"call _tty_write\n\t"
			"addl $8,%%esp\n\t"
			"popl %0\n\t"
			"pop %%fs"
			::"r" (i):"ax","cx","dx");
/*
	__asm__("push %%fs\n\t"				 保存fs。
			"push %%ds\n\t"
			"pop %%fs\n\t"				 令fs = ds。
			"pushl %0\n\t"				 将字符串长度压入堆栈(这三个入栈是调用参数)。
			"pushl $_buf\n\t"			 将buf 的地址压入堆栈。	
			"pushl $0\n\t"				 将数值0 压入堆栈。是通道号channel。
			"call _tty_write\n\t"		 调用tty_write 函数。(kernel/chr_drv/tty_io.c,290)。
			"addl $8,%%esp\n\t"			 跳过（丢弃）两个入栈参数(buf,channel)。
			"popl %0\n\t"				 弹出字符串长度值，作为返回值。
			"pop %%fs"					 恢复原fs 寄存器。
			::"r" (i):"ax","cx","dx");	 通知编译器，寄存器ax,cx,dx 值可能已经改变。
*/
	return i;			/* 返回字符串长度。	*/
}
