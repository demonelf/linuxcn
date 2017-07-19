#ifndef _STDARG_H
#define _STDARG_H

typedef char *va_list;		/* 定义va_list 是一个字符指针类型。	*/

/* Amount of space required in an argument list for an arg of type TYPE.
TYPE may alternatively be an expression whose type is used. */
/* 下面给出了类型为TYPE 的arg 参数列表所要求的空间容量。
 * TYPE 也可以是使用该类型的一个表达式 */

/* 下面这句定义了取整后的TYPE 类型的字节长度值。是int 长度(4)的倍数。	*/
#define __va_rounded_size(TYPE) \
(((sizeof (TYPE) + sizeof (int) - 1) / sizeof (int)) * sizeof (int))

/* 下面这个宏初始化指针AP，使其指向传给函数的可变参数表的第一个参数。
 * 在第一次调用va_arg或va_end之前，必须首先调用var_start宏。参数LASTARG是函数定义
 * 中最右边参数的标识符,即‘...’左边的一个标识符。AP是可变参数表参数指针，LASTARG是最
 * 后一个指定的参数。&(LASTARG)用于取其地址（即其指针），并且该指针是字符类型。加上
 * LASTARG的宽度值后AP就是可变参数表中第一个参数的指针。该宏没有返回值。
 * 第17行上的函数—builtin—saveregsO是在gcc的库程序libgcc2.c 中定义的，用于保存寄
 * 存器。相关说明参见 gcc 手册 “Target Description Macros” 章中 “Implementing
 * the Varargs Macros，，小节。
 */
#ifndef __sparc__
#define va_start(AP, LASTARG) \
(AP = ((char *) &(LASTARG) + __va_rounded_size (LASTARG)))
#else
#define va_start(AP, LASTARG) \
(__builtin_saveregs (), \
AP = ((char *) &(LASTARG) + __va_rounded_size (LASTARG)))
#endif

/* 下面该宏用于被调用函数完成一次正常返回。va_end 可以修改AP 使其在重新调用	*/
/* va_start 之前不能被使用。va_end 必须在va_arg 读完所有的参数后再被调用。	*/
void va_end (va_list);		/* Defined in gnulib *//* 在gnulib 中定义 */
#define va_end(AP)

/* 下面宏用于扩展表达式使其与下一个被传递参数具有相同的类型和值。
 * v_arg宏会扩展成函数参数列表中下一个参数的类型和值。AP应该与va_start初始化的
 * va_list AP相同。每次调用va_arg时都会修改AP，使得下一个参数的值被返回。TYPE
 * 是一个类型名。在va_start初始化之后，第1次调用va_arg会返回LASTARG指定参数后
 * 的参数值。随后的调用会返回随后参数的值。
 */
#define va_arg(AP, TYPE) \
(AP += __va_rounded_size (TYPE), \
*((TYPE *) (AP - __va_rounded_size (TYPE))))

#endif /* _STDARG_H */
