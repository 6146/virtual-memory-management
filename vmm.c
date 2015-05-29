#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include "vmm.h"

/* 页表 */
PageTableItem pageTable[4][16];
OuterPageTableItem outerpageTable[4];
/* 实存空间 */
BYTE actMem[ACTUAL_MEMORY_SIZE];
/* 用文件模拟辅存空间 */
FILE *ptr_auxMem;
/* 物理块使用标识 */
BOOL blockStatus[BLOCK_SUM];
/* 访存请求 */
Ptr_MemoryAccessRequest ptr_memAccReq;
/* FIFO */
int fifo = 0;


/* 初始化环境 */
void do_init()
{
	int i, j,k,m,n,l=0;
	srand(time(NULL));
	for (k = 0; k < 4;k++)
        {
                outerpageTable[k].pageNum = k;
                switch(k)
                {
                case 0:
			{
                        outerpageTable[k].pageIndex = 0;
                        break;
	               	}
                case 1:
                    	{
                        outerpageTable[k].pageIndex = 64;
                        break;
                    	}
                case 2:
                    	{
                        outerpageTable[k].pageIndex = 128;
                        break;
                    	}
                case 3:
                    	{
                        outerpageTable[k].pageIndex = 192;
                        break;
                    	}
                default:
                    break;
                }

        }
    	for(k=0;k<4;k++)
		for (i = 0; i < 16; i++)
		{
			pageTable[k][i].pageNum = k*16+i;
			pageTable[k][i].filled = FALSE;
			pageTable[k][i].edited = FALSE;
			pageTable[k][i].count = 0;
		/* 使用随机数设置该页的保护类型 */
			switch (rand() % 7)
			{
				case 0:
					{
						pageTable[k][i].proType = READABLE;
						break;
					}
				case 1:
					{
						pageTable[k][i].proType = WRITABLE;
						break;
					}
				case 2:
					{
						pageTable[k][i].proType = EXECUTABLE;
						break;
					}
				case 3:
					{
						pageTable[k][i].proType = READABLE | WRITABLE;
						break;
					}
				case 4:
					{
						pageTable[k][i].proType = READABLE | EXECUTABLE;
						break;
					}
				case 5:
					{
						pageTable[k][i].proType = WRITABLE | EXECUTABLE;
						break;
					}
				case 6:
					{
						pageTable[k][i].proType = READABLE | WRITABLE | EXECUTABLE;
						break;
					}
				default:
						break;
		}
		/* 设置该页对应的辅存地址 */
		pageTable[k][i].auxAddr = outerpageTable[k].pageIndex+i*PAGE_SIZE;
	}
	/* 随机设置进程号*/
	for (m = 0;m<4;m++)
        	for(n=0;n<16;n++)
    		{
      			if(l<32)
      			{
          			pageTable[m][n].proccessNum=0;
          			l++;
      			}
      			else
             			pageTable[m][n].proccessNum=1;
    		}
		for (j = 0; j < BLOCK_SUM; j++)
		{
			/* 随机选择一些物理块进行页面装入 */
			if (rand() % 2 == 0)
			{
				do_page_in(&pageTable[j/16][j%16], j);
				pageTable[j/16][j%16].blockNum = j;
				pageTable[j/16][j%16].filled = TRUE;
				blockStatus[j] = TRUE;
			}
			else
				blockStatus[j] = FALSE;
		}
}


/* 响应请求 */
void do_response()
{
	Ptr_PageTableItem ptr_pageTabIt;
	unsigned int pageCat, pageNum, offAddr;
	unsigned int actAddr;

	/* 检查地址是否越界 */
	if (ptr_memAccReq->virAddr < 0 || ptr_memAccReq->virAddr >= VIRTUAL_MEMORY_SIZE)
	{
		do_error(ERROR_OVER_BOUNDARY);
		return;
	}

	/* 计算页号和页内偏移值 */
	pageCat = ptr_memAccReq->virAddr / 64;
	pageNum = ptr_memAccReq->virAddr % 64 / 8;
	offAddr = ptr_memAccReq->virAddr % PAGE_SIZE;
	printf("页目录为： %u\t页号为：%u\t页内偏移为：%u\n", pageCat,pageNum, offAddr);

	/* 获取对应页表项 */
	ptr_pageTabIt = &pageTable[pageCat][pageNum];
    /* 检查进程号是否匹配 */
    	if(ptr_memAccReq->proccessNum!=ptr_pageTabIt->proccessNum)
    	{
        	printf("进程号不匹配，操作不符合权限\n");
        	return;
    	}
	/* 根据特征位决定是否产生缺页中断 */
	if (!ptr_pageTabIt->filled)
	{
		do_page_fault(ptr_pageTabIt);
	}

	actAddr = ptr_pageTabIt->auxAddr+offAddr;//blockNum * PAGE_SIZE + offAddr;
	printf("实地址为：%u\n", actAddr);

	/* 检查页面访问权限并处理访存请求 */
	switch (ptr_memAccReq->reqType)
	{
		case REQUEST_READ: //读请求
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & READABLE)) //页面不可读
			{
				do_error(ERROR_READ_DENY);
				return;
			}
			/* 读取实存中的内容 */
			printf("读操作成功：值为%02X\n", actMem[actAddr]);
			break;
		}
		case REQUEST_WRITE: //写请求
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & WRITABLE)) //页面不可写
			{
				do_error(ERROR_WRITE_DENY);
				return;
			}
			/* 向实存中写入请求的内容 */
			actMem[actAddr] = ptr_memAccReq->value;
			ptr_pageTabIt->edited = TRUE;
			printf("写操作成功\n");
			break;
		}
		case REQUEST_EXECUTE: //执行请求
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & EXECUTABLE)) //页面不可执行
			{
				do_error(ERROR_EXECUTE_DENY);
				return;
			}
			printf("执行成功\n");
			break;
		}
		default: //非法请求类型
		{
			do_error(ERROR_INVALID_REQUEST);
			return;
		}
	}
}

/* 处理缺页中断 */
void do_page_fault(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i;
	printf("产生缺页中断，开始进行调页...\n");
	for (i = 0; i < BLOCK_SUM; i++)
	{
		if (!blockStatus[i])
		{
			/* 读辅存内容，写入到实存 */
			do_page_in(ptr_pageTabIt, i);

			/* 更新页表内容 */
			ptr_pageTabIt->blockNum = i;
			ptr_pageTabIt->filled = TRUE;
			ptr_pageTabIt->edited = FALSE;
			ptr_pageTabIt->count = 0;

			blockStatus[i] = TRUE;
			return;
		}
	}
	/* 没有空闲物理块，进行页面替换 */
	do_LFU(ptr_pageTabIt);
}

/* 根据LFU算法进行页面替换 */
void do_LFU(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i, min, page;
	printf("没有空闲物理块，开始进行LFU页面替换...\n");
	for (i = 0, min = 0xFFFFFFFF, page = 0; i < PAGE_SUM; i++)
	{
		if (pageTable[i/16][i%16].count < min)
		{
			min = pageTable[i/16][i%16].count;
			page = i;
		}
	}
	printf("选择页目录%u的第%u页进行替换\n", page/16,page%16);
	if (pageTable[page/16][page%16].edited)
	{
		/* 页面内容有修改，需要写回至辅存 */
		printf("该页内容有修改，写回至辅存\n");
		do_page_out(&pageTable[page/16][page%16]);
	}
	pageTable[page/16][page%16].filled = FALSE;
	pageTable[page/16][page%16].count = 0;


	/* 读辅存内容，写入到实存 */
	do_page_in(ptr_pageTabIt, pageTable[page/16][page%16].blockNum);

	/* 更新页表内容 */
	ptr_pageTabIt->blockNum = pageTable[page/16][page%16].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->count = 0;
	printf("页面替换成功\n");
}

/* 将辅存内容写入实存 */
void do_page_in(Ptr_PageTableItem ptr_pageTabIt, unsigned int blockNum)
{
	unsigned int readNum;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
#endif
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((readNum = fread(actMem + blockNum * PAGE_SIZE,
		sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)

	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
		printf("DEBUG: blockNum=%u\treadNum=%u\n", blockNum, readNum);
		printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
#endif
		do_error(ERROR_FILE_READ_FAILED);
		exit(1);
	}
	printf("调页成功：辅存地址%u-->>物理块%u\n", ptr_pageTabIt->auxAddr, blockNum);
}

/* 将被替换页面的内容写回辅存 */
void do_page_out(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int writeNum;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt, ftell(ptr_auxMem));
#endif
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((writeNum = fwrite(actMem + ptr_pageTabIt->blockNum * PAGE_SIZE,
		sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
		printf("DEBUG: writeNum=%u\n", writeNum);
		printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
#endif
		do_error(ERROR_FILE_WRITE_FAILED);
		exit(1);
	}
	printf("写回成功：物理块%u-->>辅存地址%03X\n", ptr_pageTabIt->auxAddr, ptr_pageTabIt->blockNum);
}

/* 错误处理 */
void do_error(ERROR_CODE code)
{
	switch (code)
	{
		case ERROR_READ_DENY:
		{
			printf("访存失败：该地址内容不可读\n");
			break;
		}
		case ERROR_WRITE_DENY:
		{
			printf("访存失败：该地址内容不可写\n");
			break;
		}
		case ERROR_EXECUTE_DENY:
		{
			printf("访存失败：该地址内容不可执行\n");
			break;
		}
		case ERROR_INVALID_REQUEST:
		{
			printf("访存失败：非法访存请求\n");
			break;
		}
		case ERROR_OVER_BOUNDARY:
		{
			printf("访存失败：地址越界\n");
			break;
		}
		case ERROR_FILE_OPEN_FAILED:
		{
			printf("系统错误：打开文件失败\n");
			break;
		}
		case ERROR_FILE_CLOSE_FAILED:
		{
			printf("系统错误：关闭文件失败\n");
			break;
		}
		case ERROR_FILE_SEEK_FAILED:
		{
			printf("系统错误：文件指针定位失败\n");
			break;
		}
		case ERROR_FILE_READ_FAILED:
		{
			printf("系统错误：读取文件失败\n");
			break;
		}
		case ERROR_FILE_WRITE_FAILED:
		{
			printf("系统错误：写入文件失败\n");
			break;
		}
		case ERROR_FIFO_REMOVE_FAILED:
		{
			printf("系统错误：FIFO文件移出失败\n");
			break;
		}
		case ERROR_FIFO_MAKE_FAILED:
		{
			printf("系统错误：FIFO文件创建失败\n");
			break;
		}
		case ERROR_FIFO_OPEN_FAILED:
		{
			printf("系统错误：FIFO文件打开失败\n");
			break;
		}
		case ERROR_FIFO_READ_FAILED:
		{
			printf("系统错误：FIFO文件读取失败\n");
			break;
		}
		default:
		{
			printf("未知错误：没有这个错误代码\n");
		}
	}
}


/* 打印页表 */
void do_print_info()
{
	unsigned int i, j, k;
	char str[4];
	printf("页目录\t页号\t进程号\t块号\t装入\t修改\t保护\t计数\t辅存\n");
	for (i = 0; i < PAGE_SUM; i++)
	{
		printf("%u\t%u\t%u\t%u\t%u\t%u\t%s\t%u\t%u\n", i/16,i%16,pageTable[i/16][i%16].proccessNum, pageTable[i/16][i%16].blockNum, pageTable[i/16][i%16].filled,
			pageTable[i/16][i%16].edited, get_proType_str(str, pageTable[i/16][i%16].proType),
			pageTable[i/16][i%16].count, pageTable[i/16][i%16].auxAddr);
	}
}

/* 获取页面保护类型字符串 */
char *get_proType_str(char *str, BYTE type)
{
	if (type & READABLE)
		str[0] = 'r';
	else
		str[0] = '-';
	if (type & WRITABLE)
		str[1] = 'w';
	else
		str[1] = '-';
	if (type & EXECUTABLE)
		str[2] = 'x';
	else
		str[2] = '-';
	str[3] = '\0';
	return str;
}
void initFile()
{
    	int i;
    	char* key="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    	char buffer[1000];


    	ptr_auxMem=fopen(AUXILIARY_MEMORY,"w+");
    	for(i=0;i<1000;i++)
    	{
        	buffer[i] = key[rand()%62];
    	}
    	buffer[1000] = '\0';
    	//随机生成256位字符串
    	fwrite(buffer,sizeof(BYTE),1000,ptr_auxMem);
    	/*
    	size_t fwrite(const void* buffer,size_t size,size_t count, FILE* stream)
    	*/
    	printf("系统提示：初始化辅存模拟文件完成\n");
    	fclose(ptr_auxMem);
}
int main(int argc, char* argv[])
{
    	char str[2000]={0};
	char c;
	int i,j;
	command order;//
	struct stat statbuf;//
        int num;
	FILE *fp;
    	initFile();
	if (!(ptr_auxMem = fopen(AUXILIARY_MEMORY, "r+")))
	{
		do_error(ERROR_FILE_OPEN_FAILED);
		exit(1);
	}
	do_init();
	do_print_info();
	ptr_memAccReq = (Ptr_MemoryAccessRequest) malloc(sizeof(MemoryAccessRequest));
	if(stat(FIFO_FILE,&statbuf)==0)//通过文件目录获取文件信息，保存在statbuf中
	{
		if(remove(FIFO_FILE)<0)
		{
			do_error(ERROR_FIFO_REMOVE_FAILED);
			exit(1);
		}
	}

	if(mkfifo(FIFO_FILE,0666)<0)
	{
		do_error(ERROR_FIFO_MAKE_FAILED);
		exit(1);
	}
	
	if((fifo = open(FIFO_FILE,O_WRONLY | O_NONBLOCK))<0)
	{
		do_error(ERROR_FIFO_OPEN_FAILED);
		exit(1);
	}
	/* 在循环中模拟访存请求与处理过程 */
	while (TRUE)
	{
		bzero(&order,LEN);		
		if((num=read(fifo,&order,LEN))<0)
		{
			do_error(ERROR_FIFO_READ_FAILED);
			exit(1);
		}
		if(num==0)
			continue;
		c = order.c;
		if(c=='Y'||c=='y')
		{
			do_print_info();
                    	fp=fopen(AUXILIARY_MEMORY, "r+");
                    	fscanf(fp, "%s", str);
                    	printf("辅存内容：");
                    	fprintf(stdout,"%s",str);
                    	printf("\n");
                    	printf("实存内容：");
                    	for(j=0;j<128;j++)
                    	printf("%c",actMem[j]);
                    	printf("\n");
		}
		else if(c=='1'||c=='2')
		{
			ptr_memAccReq = &(order.memAccReq);
			do_response();
		}
		else if(c=='x'||c=='X')
			break;		
	}
	if (fclose(ptr_auxMem) == EOF)
	{
		do_error(ERROR_FILE_CLOSE_FAILED);
		exit(1);
	}
	close(fifo);
	return (0);
}
