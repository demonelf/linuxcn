/*
 * Why isn't this a .c file?  Enquiring minds....
 */

extern	int	sys_setup();	/* ϵͳ������ʼ�����ú�����		(kernel/blk_drv/hd.c,71)	*/
extern	int	sys_exit();		/* �����˳���					(kernel/exit.c,137)	*/
extern	int	sys_fork();		/* �������̡�					(kernel/system_call.s,208)	*/
extern	int	sys_read();		/* ���ļ���					(fs/read_write.c,55)	*/
extern	int	sys_write();	/* д�ļ���					(fs/read_write.c,83)	*/
extern	int	sys_open();		/* ���ļ���					(fs/open.c,138)	*/
extern	int	sys_close();	/* �ر��ļ���					(fs/open.c,192)	*/
extern	int	sys_waitpid();	/* �ȴ�������ֹ��				(kernel/exit.c,142)	*/
extern	int	sys_creat();	/* �����ļ���					(fs/open.c,187)	*/
extern	int	sys_link();		/* ����һ���ļ���Ӳ���ӡ�		(fs/namei.c,721)	*/
extern	int	sys_unlink();	/* ɾ��һ���ļ���(��ɾ���ļ�)��	(fs/namei.c,663)	*/
extern	int	sys_execve();	/* ִ�г���					(kernel/system_call.s,200)	*/
extern	int	sys_chdir();	/* ���ĵ�ǰĿ¼��				(fs/open.c,75)	*/
extern	int	sys_time();		/* ȡ��ǰʱ�䡣				(kernel/sys.c,102)	*/
extern	int	sys_mknod();	/* ������/�ַ������ļ���		(fs/namei.c,412)	*/
extern	int	sys_chmod();	/* �޸��ļ����ԡ�				(fs/open.c,105)	*/
extern	int	sys_chown();	/* �޸��ļ������������顣		(fs/open.c,121)	*/
extern	int	sys_break();	/* 							(-kernel/sys.c,21)	*/
extern	int	sys_stat();		/* ʹ��·����ȡ�ļ���״̬��Ϣ��	(fs/stat.c,36)	*/
extern	int	sys_lseek();	/* ���¶�λ��/д�ļ�ƫ�ơ�		(fs/read_write.c,25)	*/
extern	int	sys_getpid();	/* ȡ����id��					(kernel/sched.c,348)	*/
extern	int	sys_mount();	/* ��װ�ļ�ϵͳ��				(fs/super.c,200)	*/
extern	int	sys_umount();	/* ж���ļ�ϵͳ��				(fs/super.c,167)	*/
extern	int	sys_setuid();	/* ���ý����û�id��			(kernel/sys.c,143)	*/
extern	int	sys_getuid();	/* ȡ�����û�id��				(kernel/sched.c,358)	*/
extern	int	sys_stime();	/* ����ϵͳʱ�����ڡ�			(-kernel/sys.c,148)	*/
extern	int	sys_ptrace();	/* ������ԡ�					(-kernel/sys.c,26)	*/
extern	int	sys_alarm();	/* ���ñ�����					(kernel/sched.c,338)	*/
extern	int	sys_fstat();	/* ʹ���ļ����ȡ�ļ���״̬��Ϣ��(fs/stat.c,47)	*/
extern	int	sys_pause();	/* ��ͣ�������С�				(kernel/sched.c,144)	*/
extern	int	sys_utime();	/* �ı��ļ��ķ��ʺ��޸�ʱ�䡣	(fs/open.c,24)	*/
extern	int	sys_stty();		/* �޸��ն������á�			(-kernel/sys.c,31)	*/
extern	int	sys_gtty();		/* ȡ�ն���������Ϣ��			(-kernel/sys.c,36)	*/
extern	int	sys_access();	/* ����û���һ���ļ��ķ���Ȩ�ޡ�(fs/open.c,47)	*/
extern	int	sys_nice();		/* ���ý���ִ������Ȩ��			(kernel/sched.c,378)	*/
extern	int	sys_ftime();	/* ȡ���ں�ʱ�䡣				(-kernel/sys.c,16)	*/
extern	int	sys_sync();		/* ͬ�����ٻ������豸�����ݡ�	(fs/buffer.c,44)	*/
extern	int	sys_kill();		/* ��ֹһ�����̡�				(kernel/exit.c,60)	*/
extern	int	sys_rename();	/* �����ļ�����				(-kernel/sys.c,41)	*/
extern	int	sys_mkdir();	/* ����Ŀ¼��					(fs/namei.c,463)	*/
extern	int	sys_rmdir();	/* ɾ��Ŀ¼��					(fs/namei.c,587)	*/
extern	int	sys_dup();		/* �����ļ������				(fs/fcntl.c,42)	*/
extern	int	sys_pipe();		/* �����ܵ���					(fs/pipe.c,71)	*/
extern	int	sys_times();	/* ȡ����ʱ�䡣				(kernel/sys.c,156)	*/
extern	int	sys_prof();		/* ����ִ��ʱ������			(-kernel/sys.c,46)	*/
extern	int	sys_brk();		/* �޸����ݶγ��ȡ�			(kernel/sys.c,168)	*/
extern	int	sys_setgid();	/* ���ý�����id��				(kernel/sys.c,72)	*/
extern	int	sys_getgid();	/* ȡ������id��				(kernel/sched.c,368)	*/
extern	int	sys_signal();	/* �źŴ�����					(kernel/signal.c,48)	*/
extern	int	sys_geteuid();	/* ȡ������Ч�û�id��			(kenrl/sched.c,363)	*/
extern	int	sys_getegid();	/* ȡ������Ч��id��			(kenrl/sched.c,373)	*/
extern	int	sys_acct();		/* ���̼��ʡ�					(-kernel/sys.c,77)	*/
extern	int	sys_phys();		/*							(-kernel/sys.c,82)	*/
extern	int	sys_lock();		/*							(-kernel/sys.c,87)	*/
extern	int	sys_ioctl();	/* �豸���ơ�					(fs/ioctl.c,30)	*/
extern	int	sys_fcntl();	/* �ļ����������				(fs/fcntl.c,47)	*/
extern	int	sys_mpx();		/*							(-kernel/sys.c,92)	*/
extern	int	sys_setpgid();	/* ���ý�����id��				(kernel/sys.c,181)	*/
extern	int	sys_ulimit();	/*							(-kernel/sys.c,97)	*/
extern	int	sys_uname();	/* ��ʾϵͳ��Ϣ��				(kernel/sys.c,216)	*/
extern	int	sys_umask();	/* ȡĬ���ļ����������롣		(kernel/sys.c,230)	*/
extern	int	sys_chroot();	/* �ı��ϵͳ��				(fs/open.c,90)	*/
extern	int	sys_ustat();	/* ȡ�ļ�ϵͳ��Ϣ��			(fs/open.c,19)	*/
extern	int	sys_dup2();		/* �����ļ������				(fs/fcntl.c,36)	*/
extern	int	sys_getppid();	/* ȡ������id��				(kernel/sched.c,353)	*/
extern	int	sys_getpgrp();	/* ȡ������id������getpgid(0)��(kernel/sys.c,201)	*/
extern	int	sys_setsid();	/* ���»Ự�����г���			(kernel/sys.c,206)	*/
extern	int	sys_sigaction(); /* �ı��źŴ������̡�			(kernel/signal.c,63)	*/
extern	int	sys_sgetmask();	/* ȡ�ź������롣				(kernel/signal.c,15)	*/
extern	int	sys_ssetmask();	/* �����ź������롣			(kernel/signal.c,20)	*/
extern	int	sys_setreuid();	/* ������ʵ��/����Ч�û�id��	(kernel/sys.c,118)	*/
extern	int	sys_setregid();	/* ������ʵ��/����Ч��id��		(kernel/sys.c,51)	*/

extern int sys_sigpending();
extern int sys_sigsuspend();
extern int sys_sethostname();
extern int sys_setrlimit();
extern int sys_getrlimit();
extern int sys_getrusage();
extern int sys_gettimeofday();
extern int sys_settimeofday();
extern int sys_getgroups();
extern int sys_setgroups();
extern int sys_select();
extern int sys_symlink();
extern int sys_lstat();
extern int sys_readlink();
extern int sys_uselib();

/*  ϵͳ���ú���ָ���������ϵͳ�����жϴ�������(int 0x80)����Ϊ��ת����	*/

fn_ptr sys_call_table[] = { sys_setup, sys_exit, sys_fork, sys_read,
	sys_write, sys_open, sys_close, sys_waitpid, sys_creat, sys_link,
	sys_unlink, sys_execve, sys_chdir, sys_time, sys_mknod, sys_chmod,
	sys_chown, sys_break, sys_stat, sys_lseek, sys_getpid, sys_mount,
	sys_umount, sys_setuid, sys_getuid, sys_stime, sys_ptrace, sys_alarm,
	sys_fstat, sys_pause, sys_utime, sys_stty, sys_gtty, sys_access,
	sys_nice, sys_ftime, sys_sync, sys_kill, sys_rename, sys_mkdir,
	sys_rmdir, sys_dup, sys_pipe, sys_times, sys_prof, sys_brk, sys_setgid,
	sys_getgid, sys_signal, sys_geteuid, sys_getegid, sys_acct, sys_phys,
	sys_lock, sys_ioctl, sys_fcntl, sys_mpx, sys_setpgid, sys_ulimit,
	sys_uname, sys_umask, sys_chroot, sys_ustat, sys_dup2, sys_getppid,
	sys_getpgrp, sys_setsid, sys_sigaction, sys_sgetmask, sys_ssetmask,
	sys_setreuid,sys_setregid, sys_sigsuspend, sys_sigpending, sys_sethostname,
	sys_setrlimit, sys_getrlimit, sys_getrusage, sys_gettimeofday, 
	sys_settimeofday, sys_getgroups, sys_setgroups, sys_select, sys_symlink,
	sys_lstat, sys_readlink, sys_uselib };

/* So we don't have to do any more manual updating.... */
int NR_syscalls = sizeof(sys_call_table)/sizeof(fn_ptr);