/*
 *  linux/lib/_exit.c
 *
 *  (C) 1991  Linus Torvalds
 */
#define __LIBRARY__		/* 定义一个符号常量，见下行说明。	*/
#include <unistd.h>		/* Linux 标准头文件。定义了各种符号常数和类型，并申明了各种函数。	*/
						/* 如定义了__LIBRARY__，则还包括系统调用号和内嵌汇编_syscall0()等。	*/

/* 内核使用的程序(退出)终止函数。
 * 直接调用系统中断int 0x80，功能号—NR—exit。
 * 参数：exit—code —退出码。
 * 函数名前的关键字volatile用于告诉编译器gcc该函数不会返回。这样可让gcc产生更好一
 * 些的代码，更重要的是使用这个关键字可以避免产生某些（未初始化变量的）假警告信息。
 * 等同于 gcc 的函数属性说明：void do_exit(int error—code) _attribute_ ((noreturn));
 */
volatile void _exit(int exit_code)
{
	/* %0 - eax(系统调用号__NR_exit)；	*/
	/* %1 - ebx(退出码exit_code)。	*/
	__asm__("int $0x80"::"a" (__NR_exit),"b" (exit_code));
}
