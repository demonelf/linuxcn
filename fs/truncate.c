/*
* linux/fs/truncate.c
*
* (C) 1991 Linus Torvalds
*/

#include <linux/sched.h>	/* ���ȳ���ͷ�ļ�������������ṹtask_struct����ʼ����0 �����ݣ�	*/
							/* ����һЩ�й��������������úͻ�ȡ��Ƕ��ʽ��ຯ������䡣	*/
#include <sys/stat.h>		/* �ļ�״̬ͷ�ļ��������ļ����ļ�ϵͳ״̬�ṹstat{}�ͳ�����	*/

/* �ͷ�����һ�μ�ӿ顣���ڲ�������
 * ����dev���ļ�ϵͳ�����豸���豸�ţ�block���߼���š�	*/
static void free_ind (int dev, int block)
{
	struct buffer_head *bh;
	unsigned short *p;
	int i;

/* �����жϲ�������Ч�ԡ�����߼����Ϊ0���򷵻ء�	*/
	if (!block)
		return;
/* Ȼ���ȡһ�μ�ӿ飬���ͷ����ϱ���ʹ�õ������߼��飬Ȼ���ͷŸ�һ�μ�ӿ�Ļ���顣
 * ����free_blockO�����ͷ��豸��ָ���߼���ŵĴ��̿飨fs/bitmap.c��47�У���	*/
	if (bh = bread (dev, block))
	{
		p = (unsigned short *) bh->b_data;	/* ָ�����ݻ�������	*/
		for (i = 0; i < 512; i++, p++)		/* ÿ���߼����Ͽ���512 ����š�	*/
			if (*p)
				free_block (dev, *p);		/* �ͷ�ָ�����߼��顣	*/
					brelse (bh);			/* �ͷŻ�������	*/
	}
/* ����ͷ��豸�ϵ�һ�μ�ӿ顣	*/
	free_block(dev,block);
}

/* �ͷ����ж��μ�ӿ顣
 * ����dev���ļ�ϵͳ�����豸���豸�ţ�block���߼���š�	*/
static void free_dind (int dev, int block)
{
	struct buffer_head *bh;
	unsigned short *p;
	int i;

/* �����жϲ�������Ч�ԡ�����߼����Ϊ0���򷵻ء�	*/
	if (!block)
		return;
/* ��ȡ���μ�ӿ��һ���飬���ͷ����ϱ���ʹ�õ������߼��飬Ȼ���ͷŸ�һ����Ļ�������	*/
	if (bh = bread (dev, block))
	{
		p = (unsigned short *) bh->b_data;	/* ָ�����ݻ�������	*/
		for (i = 0; i < 512; i++, p++)		/* ÿ���߼����Ͽ�����512 �������顣	*/
			if (*p)
				free_ind (dev, *p);			/* �ͷ�����һ�μ�ӿ顣	*/
		brelse (bh);						/* �ͷŶ��μ�ӿ�ռ�õĻ���顣	*/
	}
/* ����ͷ��豸�ϵĶ��μ�ӿ顣	*/
	free_block (dev, block);
}

/* �ض��ļ����ݺ�����
 * ���ڵ��Ӧ���ļ����Ƚ�Ϊ0�����ͷ�ռ�õ��豸�ռ䡣	*/
void truncate (struct m_inode *inode)
{
	int i;

/* �����ж�ָ��i�ڵ���Ч�ԡ�������ǳ����ļ�������Ŀ¼�ļ����򷵻ء�	*/
	if (!(S_ISREG (inode->i_mode) || S_ISDIR (inode->i_mode)))
		return;
/* Ȼ���ͷ�i�ڵ��7��ֱ���߼��飬������7���߼�����ȫ���㡣����free_block()����
 * �ͷ��豸��ָ���߼���ŵĴ��̿飨fs/bitmap.c��47�У���	*/
	for (i = 0; i < 7; i++)
		if (inode->i_zone[i])
		{										/* �����Ų�Ϊ0�����ͷ�֮��	*/
			free_block (inode->i_dev, inode->i_zone[i]);
			inode->i_zone[i] = 0;
		}
	free_ind (inode->i_dev, inode->i_zone[7]);	/* �ͷ�һ�μ�ӿ顣	*/
	free_dind (inode->i_dev, inode->i_zone[8]);	/* �ͷŶ��μ�ӿ顣	*/
	inode->i_zone[7] = inode->i_zone[8] = 0;	/* �߼�����7��8 ���㡣	*/
	inode->i_size = 0;							/* �ļ���С���㡣		*/
	inode->i_dirt = 1;							/* �ýڵ����޸ı�־��	*/
/* ��������ļ��޸�ʱ���i�ڵ�ı�ʱ��Ϊ��ǰʱ�䡣��CURRENT��TIME������ͷ�ļ�
 * include/linux/sched.h �� 142 �д�������Ϊ��startup��time + jiffies/HZ��������ȡ�ô�
 * 1970:0:0:0��ʼ������Ϊֹ������������	*/
	inode->i_mtime = inode->i_ctime = CURRENT_TIME;	/* �����ļ��ͽڵ��޸�ʱ��Ϊ��ǰʱ�䡣	*/
}