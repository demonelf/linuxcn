/*
* linux/fs/truncate.c
*
* (C) 1991 Linus Torvalds
*/

#include <linux/sched.h>	/* 调度程序头文件，定义了任务结构task_struct、初始任务0 的数据，	*/
							/* 还有一些有关描述符参数设置和获取的嵌入式汇编函数宏语句。	*/
#include <sys/stat.h>		/* 文件状态头文件。含有文件或文件系统状态结构stat{}和常量。	*/

/* 释放所有一次间接块。（内部函数）
 * 参数dev是文件系统所在设备的设备号；block是逻辑块号。	*/
static void free_ind (int dev, int block)
{
	struct buffer_head *bh;
	unsigned short *p;
	int i;

/* 首先判断参数的有效性。如果逻辑块号为0，则返回。	*/
	if (!block)
		return;
/* 然后读取一次间接块，并释放其上表明使用的所有逻辑块，然后释放该一次间接块的缓冲块。
 * 函数free_blockO用于释放设备上指定逻辑块号的磁盘块（fs/bitmap.c第47行）。	*/
	if (bh = bread (dev, block))
	{
		p = (unsigned short *) bh->b_data;	/* 指向数据缓冲区。	*/
		for (i = 0; i < 512; i++, p++)		/* 每个逻辑块上可有512 个块号。	*/
			if (*p)
				free_block (dev, *p);		/* 释放指定的逻辑块。	*/
					brelse (bh);			/* 释放缓冲区。	*/
	}
/* 最后释放设备上的一次间接块。	*/
	free_block(dev,block);
}

/* 释放所有二次间接块。
 * 参数dev是文件系统所在设备的设备号；block是逻辑块号。	*/
static void free_dind (int dev, int block)
{
	struct buffer_head *bh;
	unsigned short *p;
	int i;

/* 首先判断参数的有效性。如果逻辑块号为0，则返回。	*/
	if (!block)
		return;
/* 读取二次间接块的一级块，并释放其上表明使用的所有逻辑块，然后释放该一级块的缓冲区。	*/
	if (bh = bread (dev, block))
	{
		p = (unsigned short *) bh->b_data;	/* 指向数据缓冲区。	*/
		for (i = 0; i < 512; i++, p++)		/* 每个逻辑块上可连接512 个二级块。	*/
			if (*p)
				free_ind (dev, *p);			/* 释放所有一次间接块。	*/
		brelse (bh);						/* 释放二次间接块占用的缓冲块。	*/
	}
/* 最后释放设备上的二次间接块。	*/
	free_block (dev, block);
}

/* 截断文件数据函数。
 * 将节点对应的文件长度截为0，并释放占用的设备空间。	*/
void truncate (struct m_inode *inode)
{
	int i;

/* 首先判断指定i节点有效性。如果不是常规文件或者是目录文件，则返回。	*/
	if (!(S_ISREG (inode->i_mode) || S_ISDIR (inode->i_mode)))
		return;
/* 然后释放i节点的7个直接逻辑块，并将这7个逻辑块项全置零。函数free_block()用于
 * 释放设备上指定逻辑块号的磁盘块（fs/bitmap.c第47行）。	*/
	for (i = 0; i < 7; i++)
		if (inode->i_zone[i])
		{										/* 如果块号不为0，则释放之。	*/
			free_block (inode->i_dev, inode->i_zone[i]);
			inode->i_zone[i] = 0;
		}
	free_ind (inode->i_dev, inode->i_zone[7]);	/* 释放一次间接块。	*/
	free_dind (inode->i_dev, inode->i_zone[8]);	/* 释放二次间接块。	*/
	inode->i_zone[7] = inode->i_zone[8] = 0;	/* 逻辑块项7、8 置零。	*/
	inode->i_size = 0;							/* 文件大小置零。		*/
	inode->i_dirt = 1;							/* 置节点已修改标志。	*/
/* 最后重置文件修改时间和i节点改变时间为当前时间。宏CURRENT—TIME定义在头文件
 * include/linux/sched.h 第 142 行处，定义为（startup—time + jiffies/HZ）。用于取得从
 * 1970:0:0:0开始到现在为止经过的秒数。	*/
	inode->i_mtime = inode->i_ctime = CURRENT_TIME;	/* 重置文件和节点修改时间为当前时间。	*/
}
