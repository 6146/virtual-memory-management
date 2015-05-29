#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include "vmm.h"
#define FIFO_FILE  "FIFO"//在此处定义FIFO文件的目录 
command order;
Ptr_MemoryAccessRequest ptr_memAccReq =  &(order.memAccReq);

int main()
{

	int fd;
	char c;
	while (TRUE)
	{
		bzero(&order,LEN);
		printf("输入1手动输入请求，输入2自动生成请求,按Y打印页表、辅存和实存内容，按其他键不打印,按按X退出程序...\n");
		scanf("%c",&c);
		if(c=='1')
		{
			order.c = c;
			printf("请输入请求进程号:");
			scanf("%d",&ptr_memAccReq->proccessNum);
			printf("请输入请求地址:");
			scanf("%lu",&ptr_memAccReq->virAddr);
			printf("请输入请求类型(0.读请求；1.写请求；2.执行请求):");
			int type;
            		scanf("%d",&type);
			switch (type % 3)
			{
				case 0: //读请求
				{
					ptr_memAccReq->reqType = REQUEST_READ;
					printf("产生请求：\n进程号：%u\t地址：%lu\t类型：读取\n",ptr_memAccReq->proccessNum, ptr_memAccReq->virAddr);
					break;
				}
				case 1: //写请求
				{
					ptr_memAccReq->reqType = REQUEST_WRITE;
					printf("请输入待写入的值:");
					int key;
					scanf("%d",&key);
					ptr_memAccReq->value = key % 0xFFu;
					printf("产生请求：\n进程号：%u\t地址：%lu\t类型：写入\t值：%02X\n",ptr_memAccReq->proccessNum, ptr_memAccReq->virAddr, ptr_memAccReq->value);
					break;
				}
				case 2:
				{
					ptr_memAccReq->reqType = REQUEST_EXECUTE;
					printf("产生请求：\n进程号：%u\t地址：%lu\t类型：执行\n",ptr_memAccReq->proccessNum, ptr_memAccReq->virAddr);
					break;
				}
				default:
					break;
			}
		}
		else if(c=='2')
		{
			order.c = c ; 
			do_request();
		}
		else if(c=='y'||c=='Y')
		{
			order.c = c ;
		}
		else if(c=='X'||c=='x')
			order.c = c ;
		
		if((fd = open(FIFO_FILE,O_WRONLY)) < 0)
		{
			printf("open FIFO_FILE error!\n");
			exit(1);
		}
		if(write(fd,&order,LEN)<0)
		{
			printf("write FIFO_FILE error!\n");
			exit(1);
		}
		close(fd);
		if(c=='x'||c=='X')
			break;
		while((c=getchar())!='\n');
	}
	return 0;
}
void do_request()
{
    /* 随机产生请求进程号*/
    ptr_memAccReq->proccessNum = rand()%2;
	/* 随机产生请求地址 */
	ptr_memAccReq->virAddr = rand() % VIRTUAL_MEMORY_SIZE;
	/* 随机产生请求类型 */
	switch (rand() % 3)
	{
		case 0: //读请求
		{
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("产生请求：\n进程号：%u\t地址：%lu\t类型：读取\n",ptr_memAccReq->proccessNum, ptr_memAccReq->virAddr);
			break;
		}
		case 1: //写请求
		{
			ptr_memAccReq->reqType = REQUEST_WRITE;
			/* 随机产生待写入的值 */
			ptr_memAccReq->value = rand() % 0xFFu;
			printf("产生请求：\n进程号：%u\t地址：%lu\t类型：写入\t值：%02X\n",ptr_memAccReq->proccessNum, ptr_memAccReq->virAddr, ptr_memAccReq->value);
			break;
		}
		case 2:
		{
			ptr_memAccReq->reqType = REQUEST_EXECUTE;
			printf("产生请求：\n进程号：%u\t地址：%lu\t类型：执行\n",ptr_memAccReq->proccessNum, ptr_memAccReq->virAddr);
			break;
		}
		default:
			break;
	}
}
