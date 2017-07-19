/* 硬件端口字节输出函数。	*/
/* 参数：value - 欲输出字节；	*/
/* port - 端口。	*/
#define outb(value,port) \
__asm__ ( "outb %%al,%%dx":: "a" (value), "d" (port))


/* 硬件端口字节输入函数。	*/
/* 参数：port - 端口。	*/
/* 返回读取的字节。	*/
#define inb(port) ({ \
unsigned char _v; \
__asm__ volatile ( "inb %%dx,%%al": "=a" (_v): "d" (port)); \
_v; \
})

/* 带延迟的硬件端口字节输出函数。使用两条跳转语句来延迟一会。	*/
/* 参数：value - 欲输出字节；	*/
/* port - 端口。	*/
#define outb_p(value,port) \
							__asm__ ( "outb %%al,%%dx\n" \
/* 向前跳转到标号1处(即下一条语句处)。*/	"\tjmp 1f\n" \
/* 向前跳转到标号1处。				*/	"1:\tjmp 1f\n" \
									"1:":: "a" (value), "d" (port))

/* 带延迟的硬件端口字节输入函数。使用两条跳转语句来延迟一会。	*/
/* 参数：port - 端口。	*/
/* 返回读取的字节。	*/
#define inb_p(port) ({ \
unsigned char _v; \
							__asm__ volatile ( "inb %%dx,%%al\n" \
/* 向前跳转到标号1处(即下一条语句处)。*/	"\tjmp 1f\n" \
/* 向前跳转到标号1处。				*/	"1:\tjmp 1f\n" \
									"1:": "=a" (_v): "d" (port)); \
									_v; \
})
