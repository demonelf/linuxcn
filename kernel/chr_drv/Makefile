#
# Makefile for the FREAX-kernel character device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# FREAX(Linux)内核字符设备驱动程序的Makefile 文件。
# 注意！依赖关系是由'make dep'自动进行的，它也会自动去除原来的依赖信息。不要把你自己的
# 依赖关系信息放在这里，除非是特别文件的（也即不是一个.c 文件的信息）。

AR	=gar 			# GNU 的二进制文件处理程序，用于创建、修改以及从归档文件中抽取文件。
AS	=gas 			# GNU 的汇编程序。
LD	=gld 			# GNU 的连接程序。
LDFLAGS	=-x 			#-s # 连接程序所有的参数，-s 输出文件中省略所有符号信息。-x 删除所有局部符号。
CC	=gcc 			# GNU C 语言编译器。
# 下一行是C 编译程序选项。-Wall 显示所有的警告信息；-O 优化选项，优化代码长度和执行时间；
# -fstrength-reduce 优化循环执行代码，排除重复变量；-fomit-frame-pointer 省略保存不必要
# 的框架指针；-fcombine-regs 合并寄存器，减少寄存器类的使用；-finline-functions 将所有简
# 单短小的函数代码嵌入调用程序中；-mstring-insns Linus 自己填加的优化选项，以后不再使用；
# -nostdinc -I../include 不使用默认路径中的包含文件，而使用指定目录中的(../../include)。
CFLAGS	=-Wall -O -g -fstrength-reduce -fomit-frame-pointer -fcombine-regs \
	-finline-functions -nostdinc -I../../include
# C 前处理选项。-E 只运行C 前处理，对所有指定的C 程序进行预处理并将处理结果输出到标准输
# 出设备或指定的输出文件中；-nostdinc -I../../include 同前。
CPP	=gcc -E -nostdinc -I../../include

# 下面的规则指示make 利用下面的命令将所有的.c 文件编译生成.s 汇编程序。该规则的命令
# 指使gcc 采用CFLAGS 所指定的选项对C 代码编译后不进行汇编就停止（-S），从而产生与
# 输入的各个C 文件对应的汇编代码文件。默认情况下所产生的汇编程序文件名是原C 文件名
# 去掉.c 而加上.s 后缀。-o 表示其后是输出文件的名称。其中$*.s（或$@）是自动目标变量，
# $<代表第一个先决条件，这里即是符合条件*.c 的文件。
.c.s:
	$(CC) $(CFLAGS) \
	-S -o $*.s $<
.s.o:
	$(AS) -c -o $*.o $<
.c.o:
	$(CC) $(CFLAGS) \
	-c -o $*.o $<

OBJS  = tty_io.o console.o keyboard.o serial.o rs_io.o \
	tty_ioctl.o

chr_drv.a: $(OBJS)
	$(AR) rcs chr_drv.a $(OBJS)
	sync

keyboard.s: keyboard.S ../../include/linux/config.h
	$(CPP) -traditional keyboard.S -o keyboard.s

clean:
	rm -f core *.o *.a tmp_make keyboard.s
	for i in *.c;do rm -f `basename $$i .c`.s;done

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c;do echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
console.s console.o : console.c ../../include/linux/sched.h \
  ../../include/linux/head.h ../../include/linux/fs.h \
  ../../include/sys/types.h ../../include/linux/mm.h ../../include/signal.h \
  ../../include/linux/tty.h ../../include/termios.h ../../include/asm/io.h \
  ../../include/asm/system.h 
serial.s serial.o : serial.c ../../include/linux/tty.h ../../include/termios.h \
  ../../include/linux/sched.h ../../include/linux/head.h \
  ../../include/linux/fs.h ../../include/sys/types.h ../../include/linux/mm.h \
  ../../include/signal.h ../../include/asm/system.h ../../include/asm/io.h 
tty_io.s tty_io.o : tty_io.c ../../include/ctype.h ../../include/errno.h \
  ../../include/signal.h ../../include/sys/types.h \
  ../../include/linux/sched.h ../../include/linux/head.h \
  ../../include/linux/fs.h ../../include/linux/mm.h ../../include/linux/tty.h \
  ../../include/termios.h ../../include/asm/segment.h \
  ../../include/asm/system.h 
tty_ioctl.s tty_ioctl.o : tty_ioctl.c ../../include/errno.h ../../include/termios.h \
  ../../include/linux/sched.h ../../include/linux/head.h \
  ../../include/linux/fs.h ../../include/sys/types.h ../../include/linux/mm.h \
  ../../include/signal.h ../../include/linux/kernel.h \
  ../../include/linux/tty.h ../../include/asm/io.h \
  ../../include/asm/segment.h ../../include/asm/system.h 
