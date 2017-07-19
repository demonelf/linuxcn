/*
* linux/kernel/exit.c
*
* (C) 1991 Linus Torvalds
*/

#include <errno.h>			/* 错误号头文件。包含系统中各种出错号。(Linus 从minix 中引进的)	*/
#include <signal.h>			/* 信号头文件。定义信号符号常量，信号结构以及信号操作函数原型。	*/
#include <sys/wait.h>		/* 等待调用头文件。定义系统调用wait()和waitpid()及相关常数符号。	*/
#include <linux/sched.h>	/* 调度程序头文件，定义了任务结构task_struct、初始任务0 的数据，	*/
							/* 还有一些有关描述符参数设置和获取的嵌入式汇编函数宏语句。	*/
#include <linux/kernel.h>	/* 内核头文件。含有一些内核常用函数的原形定义。	*/
#include <linux/tty.h>		/* tty 头文件，定义了有关tty_io，串行通信方面的参数、常数。	*/
#include <asm/segment.h>	/* 段操作头文件。定义了有关段寄存器操作的嵌入式汇编函数。	*/
int sys_pause (void);
int sys_close (int fd);

/* 释放指定进程占用的任务槽及其任务数据结构占用的内存页面。
参数P是任务数据结构指针。该函数在后面的sys—killO和SyS—Waitpid()函数中被调用。
扫描任务指针数组表task[]以寻找指定的任务。如果找到，则首先清空该任务槽，然后释放
该任务数据结构所占用的内存页面，最后执行调度函数并在返回时立即退出。如果在任务数组
表中没有找到指定任务对应的项，则内核panic。
*/
void release (struct task_struct *p)
{
	int i;

	if (!p)							/* 如果进程数据结构指针是NULL，则什么也不做，退出。	*/
		return;
	for (i = 1; i < NR_TASKS; i++)	/* 扫描任务数组，寻找指定任务。	*/
	if (task[i] == p)
	{
		task[i] = NULL;				/* 置空该任务项并释放相关内存页。	*/
		free_page ((long) p);
		schedule ();				/* 重新调度。	*/
		return;
	}
	panic ("trying to release non-existent task");	/* 指定任务若不存在则死机。	*/
}

/* 向指定任务P发送信号sig,权限为priv。
参数：sig -信号值；p -指定任务的指针；priv -强制发送信号的标志。即不需要考虑进程
用户属性或级别而能发送信号的权利。该函数首先判断参数的正确性，然后判断条件是否满足。
如果满足就向指定进程发送信号sig并退出，否则返回未许可错误号。
*/
static inline int
send_sig (long sig, struct task_struct *p, int priv)
{
/* 若信号不正确或任务指针为空则出错退出。	*/
	if (!p || sig < 1 || sig > 32)
		return -EINVAL;
/* 如果强制发送标志置位，或者当前进程的有效用户标识符(euid)就是指定进程的euid(也即是自己)’
或者当前进程是超级用户，则向进程p发送信号sig，即在进程p位图中添加该信号，否则出错退出。
其中suser()定义为(current-)euid==0}，用于判断是否是超级用户。
*/
	if (priv || (current->euid == p->euid) || suser ())
		p->signal |= (1 << (sig - 1));
	else
		return -EPERM;
	return 0;
}

/* 终止会话(session)。
* 进程会话的概念请参见第1章中有关进程组和会话的说明。	*/
static void
kill_session (void)
{
	struct task_struct **p = NR_TASKS + task;	/* 指针*p 首先指向任务数组最末端。	*/
/* 扫描任务指针数组，对于所有的任务（除任务0以外），如果其会话号session等于当前进程的
会话号就向它发送挂断进程信号SIGHUP。	*/
	while (--p > &FIRST_TASK)
	{
		if (*p && (*p)->session == current->session)
			(*p)->signal |= 1 << (SIGHUP - 1);	/* 发送挂断进程信号。	*/
	}
}

/*
* XXX need to check permissions needed to send signals to process
* groups, etc. etc. kill() permissions semantics are tricky!
*/
/*
* 为了向进程组等发送信号，XXX 需要检查许可。kill()的许可机制非常巧妙!
*/
/* 系统调用kiii()可用于向任何进程或进程组发送任何信号，而并非只是杀死进程?。
* 参数pid是进程号；Sig是需要发送的信号。
* 如果pid值>0，则信号被发送给进程号是pid的进程。
* 如果pid=0，那么信号就会被发送给当前进程的进程组中的所有进程。
* 如果Pid=-1，则信号sig就会发送给除第一个进程（初始进程init）外的所有进程。
* 如果pid < -1，则信号sig将发送给进程组-pid的所有进程。
* 如果信号sig为0，则不发送信号，但仍会进行错误检查。如果成功则返回0。
* 该函数扫描任务数组表，并根据pid的值对满足条件的进程发送指定的信号sig。若pid等于0，
* 表明当前进程是进程组组长，因此需要向所有组内的进程强制发送信号sig。
*/
int
sys_kill (int pid, int sig)
{
	struct task_struct **p = NR_TASKS + task;
	int err, retval = 0;

	if (!pid)
		while (--p > &FIRST_TASK)
		{
			if (*p && (*p)->pgrp == current->pid)
				if (err = send_sig (sig, *p, 1))	/* 强制发送信号。	*/
					retval = err;
		}else if (pid > 0) 
			while (--p > &FIRST_TASK)
			{
				if (*p && (*p)->pid == pid)
					if (err = send_sig (sig, *p, 0))
						retval = err;
			}else if (pid == -1)
				while (--p > &FIRST_TASK)
					if (err = send_sig (sig, *p, 0))
						retval = err;
			else
				while (--p > &FIRST_TASK)
					if (*p && (*p)->pgrp == -pid)
						if (err = send_sig (sig, *p, 0))
							retval = err;
	return retval;
}

/* 通知父进程一向进程pid发送信号SIGCHLD:默认情况下子进程将停止或终止。
如果没有找到父进程，则自己释放。但根据rosix.i要求，若父进程已先行终止，则子进程应该
被初始进程1收容。
*/
static void
tell_father (int pid)
{
	int i;

	if (pid)
/* 扫描进程数组表，寻找指定进程pid，并向其发送子进程将停止或终止信号SIGCHLD。	*/
		for (i = 0; i < NR_TASKS; i++)
			{
	if (!task[i])
		continue;
	if (task[i]->pid != pid)
		continue;
	task[i]->signal |= (1 << (SIGCHLD - 1));
	return;
			}
/* if we don't find any fathers, we just release ourselves */
/* This is not really OK. Must change it to make father 1 */
/* 如果没有找到父进程，则进程就自己释放。这样做并不好，必须改成由进程1充当其父进程。	*/
	printk ("BAD BAD - no father found\n\r");
	release (current);	/* 如果没有找到父进程，则自己释放。	*/
}

/* 程序退出处理函数。在下面137行处的系统调用处理函数sys_exit()中被调用。
该函数将把当前进程置为TASK_ZOMBIE状态，然后去执行调度函数scheduleO，不再返回。
参数code是退出状态码，或称为错误码。
*/
int
do_exit (long code)		/* code 是错误码。	*/
{
	int i;

/* 首先释放当前进程代码段和数据段所占的内存页。函数free—page—tablesO的第1个参数
(get—baseO返回值)指明在CPU线性地址空间中起始基地址，第2个（get—limit()返回值）
说明欲释放的字节长度值。get—base()宏中的current-}ldt[l]给出进程代码段描述符的位置
(current->ldt[2]给出进程代码段描述符的位置)；get—limit()中的0x0f是进程代码段的
选择符（0x17是进程数据段的选择符）。即在取段基地址时使用该段的描述符所处地址作为
参数，取段长度时使用该段的选择符作为参数。free—page—tables()函数位于mm/memory.c
文件的 105 行，get—base()和 get—limit()宏位于 include/linux/sched.h 头文件的 213 行处。
*/
	free_page_tables (get_base (current->ldt[1]), get_limit (0x0f));
	free_page_tables (get_base (current->ldt[2]), get_limit (0x17));
/* 如果当前进程有子进程，就将子进程的father 置为1(其父进程改为进程1)。如果该子进程已经	*/
/* 处于僵死(ZOMBIE)状态，则向进程1 发送子进程终止信号SIGCHLD。	*/
	for (i = 0; i < NR_TASKS; i++)
		if (task[i] && task[i]->father == current->pid)
			{
	task[i]->father = 1;
	if (task[i]->state == TASK_ZOMBIE)
/* assumption task[1] is always init */	/* 这里假设 task[l]肯定是进程 init 	*/
		(void) send_sig (SIGCHLD, task[1], 1);
			}
/* 关闭当前进程打开着的所有文件。	*/
	for (i = 0; i < NR_OPEN; i++)
		if (current->filp[i])
			sys_close (i);
/* 对当前进程工作目录pwd、根目录root 以及运行程序的i 节点进行同步操作，
放回各个i节点并分别置空（释放）。
*/
	iput (current->pwd);
	current->pwd = NULL;
	iput (current->root);
	current->root = NULL;
	iput (current->executable);
	current->executable = NULL;
/* 如果当前进程是领头(leader)进程并且其有控制的终端，则释放该终端。	*/
	if (current->leader && current->tty >= 0)
		tty_table[current->tty].pgrp = 0;
/* 如果当前进程上次使用过协处理器，则将last_task_used_math 置空。	*/
	if (last_task_used_math == current)
		last_task_used_math = NULL;
/* 如果当前进程是leader 进程，则终止所有相关进程。	*/
	if (current->leader)
		kill_session ();
/* 把当前进程置为僵死状态，表明当前进程已经释放了资源。并保存将由父进程读取的退出码。	*/
	current->state = TASK_ZOMBIE;
	current->exit_code = code;
/* 通知父进程，也即向父进程发送信号SIGCHLD -- 子进程将停止或终止。	*/
	tell_father (current->father);
	schedule ();	/* 重新调度进程运行，以让父进程处理僵死进程其他的善后事宜。
下面return语句仅用于去掉警告信息。因为这个函数不返回，所以若在函数名前加关键字
volatile,就可以告诉gcc编译器本函数不会返回的特殊情况。这样可让gcc产生更好一
些的代码，并且可以不用再写这条return语句也不会产生假警告信息。
*/
	return (-1);	/* just to suppress warnings */	/* 仅用于去掉警告信息	*/
}

/* 系统调用exit()。终止进程。
参数error—code是用户程序提供的退出状态信息，只有低字节有效。把error—code左移8
比特是wait()或waitpidO函数的要求。低字节中将用来保存wait()的状态信息。例如，
如果进程处于暂停状态（TASK—STOPPED），那么其低字节就等于0x7f。参见sys/wait.h
文件第13—18行。wait()或waitpidO利用这些宏就可以取得子进程的退出状态码或子
进程终止的原因（信号）。
*/
int
sys_exit (int error_code)
{
	return do_exit ((error_code & 0xff) << 8);
}

/* 系统调用waitpidO。挂起当前进程，直到pid指定的子进程退出（终止）或者收到要求终止
该进程的信号，或者是需要调用一个信号句柄（信号处理程序）。如果pid所指的子进程早已
退出（已成所谓的僵死进程），则本调用将立刻返回。子进程使用的所有资源将释放。
如果pid > 0，表示等待进程号等于pid的子进程。
如果pid = 0，表示等待进程组号等于当前进程组号的任何子进程。
如果pid < -1，表示等待进程组号等于pid绝对值的任何子进程。
如果pid = -1，表示等待任何子进程。
若options = WUNTRACED，表示如果子进程是停止的，也马上返回（无须跟踪）。
若options = WN0HANG，表示如果没有子进程退出或终止就马上返回。
如果返回状态指针stat—addr不为空，则就将状态信息保存到那里。
参数pid是进程号；*stat—addr是保存状态信息位置的指针；options是waitpid选项。
*/
int
sys_waitpid (pid_t pid, unsigned long *stat_addr, int options)
{
	int flag, code;		/* flag标志用于后面表示所选出的子进程处于就绪或睡眠态。	*/
	struct task_struct **p;

	verify_area (stat_addr, 4);
repeat:
	flag = 0;
	/* 从任务数组末端开始扫描所有任务，跳过空项、本进程项以及非当前进程的子进程项。	*/
	for (p = &LAST_TASK; p > &FIRST_TASK; --p)
	{									/* 从任务数组末端开始扫描所有任务。	*/
		if (!*p || *p == current)			/* 跳过空项和本进程项。	*/
			continue;
		if ((*p)->father != current->pid)	/* 如果不是当前进程的子进程则跳过。	*/
			continue;
/* 此时扫描选择到的进程P肯定是当前进程的子进程。
如果等待的子进程号pid>0，但与被扫描子进程P的pid不相等，说明它是当前进程另外的子
进程，于是跳过该进程，接着扫描下一个进程。
*/
		if (pid > 0)
		{									/* 如果指定的pid>0，但扫描的进程pid	*/
			if ((*p)->pid != pid)				/* 与之不等，则跳过。	*/
				continue;
/* 否则，如果指定等待进程的pid=0，表示正在等待进程组号等于当前进程组号的任何子进程。
如果此时被扫描进程P的进程组号与当前进程的组号不等，则跳过。
*/
		}
		else if (!pid)
		{									/* 如果指定的pid=0，但扫描的进程组号	*/
			if ((*p)->pgrp != current->pgrp)	/* 与当前进程的组号不等，则跳过。	*/
				continue;
/* 否则，如果指定的Pid<-1，表示正在等待进程组号等于pid绝对值的任何子进程。如果此时
被扫描进程P的组号与pid的绝对值不等，则跳过。
*/
		}
		else if (pid != -1)
		{									/* 如果指定的pid<-1，但扫描的进程组	*/
			if ((*p)->pgrp != -pid)			/* 与其绝对值不等，则跳过。	*/
				continue;
		}
/* 如果前3个对pid的判断都不符合，则表示当前进程正在等待其任何子进程，也即pid =-1
的情况。此时所选择到的进程p或者是其进程号等于指定pid，或者是当前进程组中的任何
子进程，或者是进程号等于指定pid绝对值的子进程，或者是任何子进程（此时指定的pid
等于-1）。接下来根据这个子进程p所处的状态来处理。
*/
		switch ((*p)->state)
		{
/* 如果子进程P处于僵死状态，则首先把它在用户态和内核态运行的时间分别累计到当前进程
(父进程)中，然后取出子进程的pid和退出码，并释放该子进程。最后返回子进程的退出码和pid。
*/
			case TASK_STOPPED:
				if (!(options & WUNTRACED))
					continue;
				put_fs_long (0x7f, stat_addr);	/* 置状态信息为0x7f。	*/
				return (*p)->pid;					/* 退出，返回子进程的进程号。	*/
			case TASK_ZOMBIE:
				current->cutime += (*p)->utime;	/* 更新当前进程的子进程用户	*/
				current->cstime += (*p)->stime;	/* 态和核心态运行时间。	*/
				flag = (*p)->pid;				/* 临时保存子进程pid。	*/
				code = (*p)->exit_code;			/* 取子进程的退出码。	*/
				release (*p);					/* 释放该子进程。	*/
				put_fs_long (code, stat_addr);	/* 置状态信息为退出码值。	*/
				return flag;					/* 退出，返回子进程的pid.	*/
/* 如果这个子进程p的状态既不是停止也不是僵死，那么就置flag=l。表示找到过一个符合
要求的子进程，但是它处于运行态或睡眠态。
*/
			default:
				flag = 1;		/* 如果子进程不在停止或僵死状态，则flag=1。	*/
				continue;
		}
	}
/* 在上面对任务数组扫描结束后，如果flag被置位，说明有符合等待要求的子进程并没有处
于退出或僵死状态。如果此时已设置WN0HANG选项（表示若没有子进程处于退出或终止态就
立刻返回），就立刻返回0，退出。否则把当前进程置为可中断等待状态并重新执行调度。
当又开始执行本进程时，如果本进程没有收到除SIGCHLD以外的信号，则还是重复处理。
否则，返回出错码‘中断的系统调用’并退出。针对这个出错号用户程序应该再继续调用本
函数等待子进程。
*/

	if (flag)
	{									/* 如果子进程没有处于退出或僵死状态，	*/
		if (options & WNOHANG)			/* 并且options = WNOHANG，则立刻返回。	*/
			return 0;
		current->state = TASK_INTERRUPTIBLE;	/* 置当前进程为可中断等待状态。	*/
		schedule ();					/* 重新调度。	*/
		if (!(current->signal &= ~(1 << (SIGCHLD - 1))))	/* 又开始执行本进程时，	*/
			goto repeat;				/* 如果进程没有收到除SIGCHLD 的信号，则还是重复处理。	*/
		else
			return -EINTR;				/* 退出，返回出错码。	*/
	}
/* 若没有找到符合要求的子进程，则返回出错码（子进程不存在）。	*/
	return -ECHILD;
}
