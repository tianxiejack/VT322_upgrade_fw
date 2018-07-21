#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>  // For stat().
#include <sys/stat.h>   // For stat().
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

static int fw_update_runtar(void);
static int SocketRecv(int m_port, void *rcv_buf,int data_len);

#define RECV_BUF_SIZE 1024

int main(int argc,char **argv)
{
	int sockfd, accept_fd;
	int write_len = 0;
	int recv_len = 0;
	socklen_t len;
	unsigned char send_buf[RECV_BUF_SIZE];
	unsigned char recv_buf[RECV_BUF_SIZE];
	int listen_number=5;
	FILE *fp;
	struct sockaddr_in addr,src_addr;

	unsigned int uartdata_pos = 0;
	unsigned char frame_head[]={0xEB, 0x53};
	static struct data_buf
	{
		unsigned int len;
		unsigned int pos;
		unsigned char reading;
		unsigned char buf[RECV_BUF_SIZE];
	}swap_data = {0, 0, 0,{0}};
	memset(&swap_data,0,sizeof(swap_data));
	memset(recv_buf,0,sizeof(recv_buf));

	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if( -1 == sockfd )   
	{  
		perror("socketfd");
		return -1;
	}  
	memset(&addr,0,sizeof(addr));	
	addr.sin_family=AF_INET;  
	addr.sin_addr.s_addr=INADDR_ANY;  
	addr.sin_port=htons(8999);  

	if(-1 == bind(sockfd,(struct sockaddr*)&addr,sizeof(struct sockaddr)))
	{
		perror("bind");
		return -1;
	}
	if( -1 == listen(sockfd,listen_number))
	{
		perror("listen");
		return -1;
	}

	while(1)
	{
		if(NULL ==(fp = fopen("dss_pkt.tar.gz","w")))
		{
			perror("fopen\r\n");
			return -1;
		}
		printf("Wait for connect......\r\n");
		len = sizeof(struct sockaddr);
		if(-1== (accept_fd = accept(sockfd,(struct sockaddr *)&src_addr,&len)))
		{
			perror("accept");
			return -1;	
		}	
		else
			printf("Now new Client connect in :socketfd = %d\r\n",accept_fd);
		
		while(1)
		{
			recv_len= SocketRecv(accept_fd,recv_buf,RECV_BUF_SIZE);
			if(recv_len <0)
				{
					printf("Recv file error!\r\n");
					break;
				}
				if(recv_len==0)
				{
					printf("read end\n");
					break;
				}
				uartdata_pos = 0;
				if(recv_len>0)
				{
					for(int j=0;j<recv_len;j++)
					{
						   printf("%02x ",recv_buf[j]);
					}
					printf("\n");
					while (uartdata_pos< recv_len)
			    		{
			        		if((0 == swap_data.reading) || (2 == swap_data.reading))
			       		{
			            			if(frame_head[swap_data.len] == recv_buf[uartdata_pos])
			            			{
			                			swap_data.buf[swap_data.pos++] =  recv_buf[uartdata_pos++];
			                			swap_data.len++;
			                			swap_data.reading = 2;
			                			if(swap_data.len == sizeof(frame_head)/sizeof(char))
			                    				swap_data.reading = 1;
			            			}
				           		 else
				            		{
				                		uartdata_pos++;
				                		if(2 == swap_data.reading)
				                    		memset(&swap_data, 0, sizeof(struct data_buf));
				            		}
			    			}
				        	else if(1 == swap_data.reading)
					        {
					      		swap_data.buf[swap_data.pos++] = recv_buf[uartdata_pos++];
								swap_data.len++;
							if(swap_data.len>=4)
							{
								if(swap_data.len==((swap_data.buf[2]|(swap_data.buf[3]<<8))+5))
								{
									if(0 == check_sum(swap_data.buf,swap_data.len))
									{
										if(swap_data.buf[4]==0x35)
										{
											printf("%s,%d,it is upgrade fw\n",__FILE__,__LINE__);
							                		upgradefw(swap_data.buf,swap_data.len);
										}
										else
										{
											printf("%s,%d,it is other command\n",__FILE__,__LINE__);
										}
									}
									memset(&swap_data, 0, sizeof(struct data_buf));
								}
							}
					        }
					}
				}
		}

		printf("Receive file: dss_pkt.tar.gz  OK !!\r\n");
		fclose(fp);	
		close(accept_fd);
	}
	close(sockfd);
	return 0;
}

int SocketRecv(int m_port, void *rcv_buf,int data_len)
{
		int fs_sel,len;
		fd_set fd_uartRead;
		struct timeval timeout;
		FD_ZERO(&fd_uartRead);
		FD_SET(m_port,&fd_uartRead);

	    	timeout.tv_sec = 0;
		timeout.tv_usec = 50000;

		fs_sel = select(m_port+1,&fd_uartRead,NULL,NULL,&timeout);
		if(fs_sel){
			len = read(m_port,rcv_buf,data_len);
			return len;
		}
		else if(-1 == fs_sel){
			printf("ERR: Uart Recv  select  Error!!\r\n");
			return -1;
		}
		else if(0 == fs_sel){
			printf("Warning: Uart Recv  time  out!!\r\n");
			return 0;
		}
}

int fw_update_runtar(void)
{
	printf("Execute cmd tar...\r\n"); 
	int iRtn=-1;
	char cmdBuf[128];	
	sprintf(cmdBuf, "tar -zxvf %s", "dss_pkt.tar.gz");
	iRtn = system(cmdBuf);
	if(iRtn != 0){
		printf(" [FWUP_NET] %s rtn=%d\n",__func__, iRtn);
	}
	else
	{
		iRtn = system("cp dss_pkt/* ~/dss_pkt");
	}
	return iRtn;
}
int upgradefw(unsigned char *swap_data_buf, unsigned int swap_data_len)
{
	static FILE *fp;
	int write_len;
	int file_len;
	static int current_len = 0;
	static int filestatus = 0;
	int recv_len = swap_data_len-13;
	unsigned char buf[7] = {0xEB,0x53,0x07,0x00,0x35,0x00,0x00};

	memcpy(&file_len,swap_data_buf+5,4);
	if(filestatus == 0)
	{
		if(NULL ==(fp = fopen("dss_pkt.tar.gz","w")))
		{
			perror("fopen\r\n");
			return -1;
		}
	}
	filestatus = 1;
	write_len = fwrite(swap_data_buf+12,1,recv_len,fp);
	fflush(fp);
	if(write_len < recv_len)
	{
		printf("Write upgrade tgz file error!\r\n");
		return -1;
	}
	current_len += recv_len;
	if(current_len == file_len)
	{
		current_len = 0;
		filestatus = 0;
		fclose(fp);
		
		if(fw_update_runtar() == 0)
			;//respupgradefw= 0x01;
		else
			;
			//respupgradefw = 0x02;
		//feedback=ACK_upgradefw;
	}
	
}

int check_sum(unsigned char *buf, int  len)
{
	int cksum = 0;
	for(int n=1;n<len-1;n++)
	{
		cksum ^= buf[n];
	}
	if(cksum == buf[len-1])
		return 0;
	else
	{
		printf("checksum error, cal is %02x, recv is %02x\n", cksum, buf[len-1]);
		return -1;
	}
}
