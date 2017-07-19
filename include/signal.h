#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys/types.h>			/* 类型头文件。定义了基本的系统数据类型。	*/

typedef int sig_atomic_t;		/* 定义信号原子操作类型。	*/
typedef unsigned int sigset_t;	/* 32 bits */	/* 定义信号集类型。	*/

#define _NSIG 32				/* 定义信号种类 -- 32 种。	*/
#define NSIG _NSIG				/* NSIG = _NSIG	*/

/* 以下这些是Linux0.11内核中定义的信号。其中包括了 P0SIX.1要求的所有20个信号。	*/
#define SIGHUP		1			/* Hang Up		-- 挂断控制终端或进程。	*/
#define SIGINT		2			/* Interrupt		-- 来自键盘的中断。	*/
#define SIGQUIT		3			/* Quit			-- 来自键盘的退出。	*/
#define SIGILL		4			/* Illeagle		-- 非法指令。	*/
#define SIGTRAP		5			/* Trap			-- 跟踪断点。	*/
#define SIGABRT		6			/* Abort		-- 异常结束。	*/
#define SIGIOT		6			/* IO Trap		-- 同上。	*/
#define SIGUNUSED	7			/* Unused		-- 没有使用。	*/
#define SIGFPE		8			/* FPE			-- 协处理器出错。	*/
#define SIGKILL		9			/* Kill			-- 强迫进程终止。	*/
#define SIGUSR1		10			/* User1		-- 用户信号1，进程可使用。	*/
#define SIGSEGV		11			/* Segment Violation -- 无效内存引用。	*/
#define SIGUSR2		12			/* User2		-- 用户信号2，进程可使用。	*/
#define SIGPIPE		13			/* Pipe			-- 管道写出错，无读者。	*/
#define SIGALRM		14			/* Alarm		-- 实时定时器报警。	*/
#define SIGTERM		15			/* Terminate	-- 进程终止。	*/
#define SIGSTKFLT	16			/* Stack Fault	-- 栈出错（协处理器）。	*/
#define SIGCHLD		17			/* Child		-- 子进程停止或被终止。	*/
#define SIGCONT		18			/* Continue		-- 恢复进程继续执行。	*/
#define SIGSTOP		19			/* Stop			-- 停止进程的执行。	*/
#define SIGTSTP		20			/* TTY Stop		-- tty 发出停止进程，可忽略。	*/
#define SIGTTIN		21			/* TTY In		-- 后台进程请求输入。	*/
#define SIGTTOU		22			/* TTY Out		-- 后台进程请求输出。	*/

/* Ok, I haven't implemented sigactions, but trying to keep headers POSIX */
/* OK，我还没有实现sigactions 的编制，但在头文件中仍希望遵守POSIX 标准 */

/* 上面原注释已经过时，因为在0.11内核中已经实现了 sigaction()。	*/
#define SA_NOCLDSTOP	1			/* 当子进程处于停止状态，就不对SIGCHLD 处理。	*/
#define SA_NOMASK		0x40000000	/* 不阻止在指定的信号处理程序(信号句柄)中再收到该信号。	*/
#define SA_ONESHOT		0x80000000	/* 信号句柄一旦被调用过就恢复到默认处理句柄。	*/

/* 以下参数用于sigprocmask()-- 改变阻塞信号集(屏蔽码)。这些参数可以改变该函数的行为。	*/
#define SIG_BLOCK	0	/* for blocking signals */			/* 在阻塞信号集中加上给定的信号集。	*/
#define SIG_UNBLOCK	1	/* for unblocking signals */		/* 从阻塞信号集中删除指定的信号集。	*/
#define SIG_SETMASK	2	/* for setting the signal mask */	/* 设置阻塞信号集（信号屏蔽码）。	*/

/* 以下两个常数符号都表示指向无返回值的函数指针，且都有一个int整型参数。这两个指针
 * 值是逻辑上讲实际上不可能出现的函数地址值。可作为下面signal函数的第二个参数。用
 * 于告知内核，让内核处理信号或忽略对信号的处理。使用方法参见kernel/signal.c程序，
 * 第 94一96 行。	*/
#define SIG_DFL ((void (*)(int))0)	/* default signal handling */	/* 默认的信号处理程序（信号句柄）。	*/
#define SIG_IGN ((void (*)(int))1)	/* ignore signal */				/* 忽略信号的处理程序。	*/

/* 下面是sigaction 的数据结构。
 * sa_handler 是对应某信号指定要采取的行动。可以是上面的SIG_DFL，或者是SIG_IGN 来忽略
 * 该信号，也可以是指向处理该信号函数的一个指针。
 * sa_mask 给出了对信号的屏蔽码，在信号程序执行时将阻塞对这些信号的处理。
 * sa_flags 指定改变信号处理过程的信号集。它是由37-39 行的位标志定义的。
 * sa_restorer 恢复过程指针，是用于保存原返回的过程指针。
 * 另外，引起触发信号处理的信号也将被阻塞，除非使用了SA_NOMASK 标志。
 */
struct sigaction
{
  void (*sa_handler) (int);
  sigset_t sa_mask;
  int sa_flags;
  void (*sa_restorer) (void);
};

/* 下面signal函数用于是为信号—sig安装一新的信号处理程序（信号句柄），与sigaction()类似。
 * 该函数含有两个参数：指定需要捕获的信号_sig;具有一个参数且无返回值的函数指针_func。该函
 * 数返回值也是具有一个int参数（最后一个(int)）且无返回值的函数指针， 它是处理该信号的原处
 * 理句柄。	*/
void (*signal (int _sig, void (*_func) (int))) (int);

/* 下面两函数用于发送信号。kilio用于向任何进程或进程组发送信号。raiseO用于向当前进
 * 程自身发送信号。其作用等价于kill(getpid()，sig)。参见kernel/exit.c，60行。
 */
int raise (int sig);				
int kill (pid_t pid, int sig);		/* 可用于向任何进程组或进程发送任何信号。	*/
/* 在进程的任务结构中，除有一个以比特位表示当前进程待处理的32位信号字段signal以外，还
 * 有一个同样以比特位表示的用于屏蔽进程当前阻塞信号集（屏蔽信号集）的字段blocked，也是
 * 32位，每个比特代表一个对应的阻塞信号。修改进程的屏蔽信号集可以阻塞或解除阻塞所指定的
 * 信号。以下五个函数就是用于操作进程屏蔽信号集，虽然简单实现起来很简单，但本版本内核中
 * 还未实现。函数 sigaddset() 和 sigdelset() 用于对信号集中的信号进行增、删修改。
 * sigaddset()用于向 mask指向的信号集中增加指定的信号signoosigdelset则反之。函数
 * sigemptyset()和sigfillset()用于初始化进程屏蔽信号集。每个程序在使用信号集前，都
 * 需要使用这两个函数之一对屏蔽信号集进行初始化。sigemptyset() 用于清空屏蔽的所有信号，
 * 也即响应所有的信号。sigfillset_+向信号集中置入所有信号，也即屏蔽所有信号。当然
 * SIGINT和SIGSTOP是不能被屏蔽的。	*/




int sigaddset (sigset_t * mask, int signo);	/* 向信号集中添加信号。	*/
int sigdelset (sigset_t * mask, int signo);	/* 从信号集中去除指定的信号。	*/
int sigemptyset (sigset_t * mask);			/* 从信号集中清除指定信号集。	*/
int sigfillset (sigset_t * mask);			/* 向信号集中置入所有信号。	*/


int sigismember (sigset_t * mask, int signo);	/* 1 --is, 0 - not, -1 error */
					/* 判断一个信号是否是信号集中的。    1 - 是， 0 - 不是，-1 - 出错。	*/

int sigpending (sigset_t * set);			/* 对set 中的信号进行检测，看是否有挂起的信号。	*/

/* 改变目前的被阻塞信号集（信号屏蔽码）。	*/
int sigprocmask (int how, sigset_t * set, sigset_t * oldset);

/* 用sigmask 临时替换进程的信号屏蔽码,然后暂停该进程直到收到一个信号。	*/
int sigsuspend (sigset_t * sigmask);

/* 用于改变进程在收到指定信号时所采取的行动。	*/
int sigaction (int sig, struct sigaction *act, struct sigaction *oldact);

#endif /* _SIGNAL_H */
