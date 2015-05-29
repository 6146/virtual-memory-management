#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include "vmm.h"

/* ҳ�� */
PageTableItem pageTable[4][16];
OuterPageTableItem outerpageTable[4];
/* ʵ��ռ� */
BYTE actMem[ACTUAL_MEMORY_SIZE];
/* ���ļ�ģ�⸨��ռ� */
FILE *ptr_auxMem;
/* �����ʹ�ñ�ʶ */
BOOL blockStatus[BLOCK_SUM];
/* �ô����� */
Ptr_MemoryAccessRequest ptr_memAccReq;
/* FIFO */
int fifo = 0;


/* ��ʼ������ */
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
		/* ʹ����������ø�ҳ�ı������� */
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
		/* ���ø�ҳ��Ӧ�ĸ����ַ */
		pageTable[k][i].auxAddr = outerpageTable[k].pageIndex+i*PAGE_SIZE;
	}
	/* ������ý��̺�*/
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
			/* ���ѡ��һЩ��������ҳ��װ�� */
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


/* ��Ӧ���� */
void do_response()
{
	Ptr_PageTableItem ptr_pageTabIt;
	unsigned int pageCat, pageNum, offAddr;
	unsigned int actAddr;

	/* ����ַ�Ƿ�Խ�� */
	if (ptr_memAccReq->virAddr < 0 || ptr_memAccReq->virAddr >= VIRTUAL_MEMORY_SIZE)
	{
		do_error(ERROR_OVER_BOUNDARY);
		return;
	}

	/* ����ҳ�ź�ҳ��ƫ��ֵ */
	pageCat = ptr_memAccReq->virAddr / 64;
	pageNum = ptr_memAccReq->virAddr % 64 / 8;
	offAddr = ptr_memAccReq->virAddr % PAGE_SIZE;
	printf("ҳĿ¼Ϊ�� %u\tҳ��Ϊ��%u\tҳ��ƫ��Ϊ��%u\n", pageCat,pageNum, offAddr);

	/* ��ȡ��Ӧҳ���� */
	ptr_pageTabIt = &pageTable[pageCat][pageNum];
    /* �����̺��Ƿ�ƥ�� */
    	if(ptr_memAccReq->proccessNum!=ptr_pageTabIt->proccessNum)
    	{
        	printf("���̺Ų�ƥ�䣬����������Ȩ��\n");
        	return;
    	}
	/* ��������λ�����Ƿ����ȱҳ�ж� */
	if (!ptr_pageTabIt->filled)
	{
		do_page_fault(ptr_pageTabIt);
	}

	actAddr = ptr_pageTabIt->auxAddr+offAddr;//blockNum * PAGE_SIZE + offAddr;
	printf("ʵ��ַΪ��%u\n", actAddr);

	/* ���ҳ�����Ȩ�޲�����ô����� */
	switch (ptr_memAccReq->reqType)
	{
		case REQUEST_READ: //������
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & READABLE)) //ҳ�治�ɶ�
			{
				do_error(ERROR_READ_DENY);
				return;
			}
			/* ��ȡʵ���е����� */
			printf("�������ɹ���ֵΪ%02X\n", actMem[actAddr]);
			break;
		}
		case REQUEST_WRITE: //д����
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & WRITABLE)) //ҳ�治��д
			{
				do_error(ERROR_WRITE_DENY);
				return;
			}
			/* ��ʵ����д����������� */
			actMem[actAddr] = ptr_memAccReq->value;
			ptr_pageTabIt->edited = TRUE;
			printf("д�����ɹ�\n");
			break;
		}
		case REQUEST_EXECUTE: //ִ������
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & EXECUTABLE)) //ҳ�治��ִ��
			{
				do_error(ERROR_EXECUTE_DENY);
				return;
			}
			printf("ִ�гɹ�\n");
			break;
		}
		default: //�Ƿ���������
		{
			do_error(ERROR_INVALID_REQUEST);
			return;
		}
	}
}

/* ����ȱҳ�ж� */
void do_page_fault(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i;
	printf("����ȱҳ�жϣ���ʼ���е�ҳ...\n");
	for (i = 0; i < BLOCK_SUM; i++)
	{
		if (!blockStatus[i])
		{
			/* ���������ݣ�д�뵽ʵ�� */
			do_page_in(ptr_pageTabIt, i);

			/* ����ҳ������ */
			ptr_pageTabIt->blockNum = i;
			ptr_pageTabIt->filled = TRUE;
			ptr_pageTabIt->edited = FALSE;
			ptr_pageTabIt->count = 0;

			blockStatus[i] = TRUE;
			return;
		}
	}
	/* û�п�������飬����ҳ���滻 */
	do_LFU(ptr_pageTabIt);
}

/* ����LFU�㷨����ҳ���滻 */
void do_LFU(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i, min, page;
	printf("û�п�������飬��ʼ����LFUҳ���滻...\n");
	for (i = 0, min = 0xFFFFFFFF, page = 0; i < PAGE_SUM; i++)
	{
		if (pageTable[i/16][i%16].count < min)
		{
			min = pageTable[i/16][i%16].count;
			page = i;
		}
	}
	printf("ѡ��ҳĿ¼%u�ĵ�%uҳ�����滻\n", page/16,page%16);
	if (pageTable[page/16][page%16].edited)
	{
		/* ҳ���������޸ģ���Ҫд�������� */
		printf("��ҳ�������޸ģ�д��������\n");
		do_page_out(&pageTable[page/16][page%16]);
	}
	pageTable[page/16][page%16].filled = FALSE;
	pageTable[page/16][page%16].count = 0;


	/* ���������ݣ�д�뵽ʵ�� */
	do_page_in(ptr_pageTabIt, pageTable[page/16][page%16].blockNum);

	/* ����ҳ������ */
	ptr_pageTabIt->blockNum = pageTable[page/16][page%16].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->count = 0;
	printf("ҳ���滻�ɹ�\n");
}

/* ����������д��ʵ�� */
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
	printf("��ҳ�ɹ��������ַ%u-->>�����%u\n", ptr_pageTabIt->auxAddr, blockNum);
}

/* �����滻ҳ�������д�ظ��� */
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
	printf("д�سɹ��������%u-->>�����ַ%03X\n", ptr_pageTabIt->auxAddr, ptr_pageTabIt->blockNum);
}

/* ������ */
void do_error(ERROR_CODE code)
{
	switch (code)
	{
		case ERROR_READ_DENY:
		{
			printf("�ô�ʧ�ܣ��õ�ַ���ݲ��ɶ�\n");
			break;
		}
		case ERROR_WRITE_DENY:
		{
			printf("�ô�ʧ�ܣ��õ�ַ���ݲ���д\n");
			break;
		}
		case ERROR_EXECUTE_DENY:
		{
			printf("�ô�ʧ�ܣ��õ�ַ���ݲ���ִ��\n");
			break;
		}
		case ERROR_INVALID_REQUEST:
		{
			printf("�ô�ʧ�ܣ��Ƿ��ô�����\n");
			break;
		}
		case ERROR_OVER_BOUNDARY:
		{
			printf("�ô�ʧ�ܣ���ַԽ��\n");
			break;
		}
		case ERROR_FILE_OPEN_FAILED:
		{
			printf("ϵͳ���󣺴��ļ�ʧ��\n");
			break;
		}
		case ERROR_FILE_CLOSE_FAILED:
		{
			printf("ϵͳ���󣺹ر��ļ�ʧ��\n");
			break;
		}
		case ERROR_FILE_SEEK_FAILED:
		{
			printf("ϵͳ�����ļ�ָ�붨λʧ��\n");
			break;
		}
		case ERROR_FILE_READ_FAILED:
		{
			printf("ϵͳ���󣺶�ȡ�ļ�ʧ��\n");
			break;
		}
		case ERROR_FILE_WRITE_FAILED:
		{
			printf("ϵͳ����д���ļ�ʧ��\n");
			break;
		}
		case ERROR_FIFO_REMOVE_FAILED:
		{
			printf("ϵͳ����FIFO�ļ��Ƴ�ʧ��\n");
			break;
		}
		case ERROR_FIFO_MAKE_FAILED:
		{
			printf("ϵͳ����FIFO�ļ�����ʧ��\n");
			break;
		}
		case ERROR_FIFO_OPEN_FAILED:
		{
			printf("ϵͳ����FIFO�ļ���ʧ��\n");
			break;
		}
		case ERROR_FIFO_READ_FAILED:
		{
			printf("ϵͳ����FIFO�ļ���ȡʧ��\n");
			break;
		}
		default:
		{
			printf("δ֪����û������������\n");
		}
	}
}


/* ��ӡҳ�� */
void do_print_info()
{
	unsigned int i, j, k;
	char str[4];
	printf("ҳĿ¼\tҳ��\t���̺�\t���\tװ��\t�޸�\t����\t����\t����\n");
	for (i = 0; i < PAGE_SUM; i++)
	{
		printf("%u\t%u\t%u\t%u\t%u\t%u\t%s\t%u\t%u\n", i/16,i%16,pageTable[i/16][i%16].proccessNum, pageTable[i/16][i%16].blockNum, pageTable[i/16][i%16].filled,
			pageTable[i/16][i%16].edited, get_proType_str(str, pageTable[i/16][i%16].proType),
			pageTable[i/16][i%16].count, pageTable[i/16][i%16].auxAddr);
	}
}

/* ��ȡҳ�汣�������ַ��� */
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
    	//�������256λ�ַ���
    	fwrite(buffer,sizeof(BYTE),1000,ptr_auxMem);
    	/*
    	size_t fwrite(const void* buffer,size_t size,size_t count, FILE* stream)
    	*/
    	printf("ϵͳ��ʾ����ʼ������ģ���ļ����\n");
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
	if(stat(FIFO_FILE,&statbuf)==0)//ͨ���ļ�Ŀ¼��ȡ�ļ���Ϣ��������statbuf��
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
	/* ��ѭ����ģ��ô������봦����� */
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
                    	printf("�������ݣ�");
                    	fprintf(stdout,"%s",str);
                    	printf("\n");
                    	printf("ʵ�����ݣ�");
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
