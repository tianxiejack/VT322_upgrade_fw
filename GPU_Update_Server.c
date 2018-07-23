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
					//printf("read end\n");
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
										else if(swap_data.buf[4]==0x32)
										{
											printf("%s,%d,it is import config file\n",__FILE__,__LINE__);
							                		impconfig(swap_data.buf,swap_data.len);
										}
										else if(swap_data.buf[4]==0x33)
										{
											printf("%s,%d,it is export config file\n",__FILE__,__LINE__);
							                		expconfig(accept_fd);
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
			//printf("Warning: Uart Recv  time  out!!\r\n");
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

int impconfig(unsigned char *swap_data_buf, unsigned int swap_data_len)
{
	static FILE *fp2;
	int write_len;
	int file_len;
	static int current_len2 = 0;
	static int filestatus2 = 0;
	int recv_len = swap_data_len-13;
	unsigned char buf[7] = {0xEB,0x53,0x07,0x00,0x35,0x00,0x00};

	memcpy(&file_len,swap_data_buf+5,4);
	if(filestatus2 == 0)
	{
		if(NULL ==(fp2 = fopen("Profile.yml","w")))
		{
			perror("fopen\r\n");
			return -1;
		}
	}
	filestatus2 = 1;
	write_len = fwrite(swap_data_buf+12,1,recv_len,fp2);
	fflush(fp2);
	if(write_len < recv_len)
	{
		printf("Write import config file error!\r\n");
		return -1;
	}
	current_len2 += recv_len;
	if(current_len2 == file_len)
	{
		current_len2 = 0;
		filestatus2 = 0;
		fclose(fp2);
		
		if(0 == system("cp Profile.yml ~/dss_pkt/"))
			printf("cp right\n");
		else
			;

	}
	
}

int expconfig(int accept_fd)
{
	printf("it is export config\n");
	int write_len = 0;
	int len = 0;
	FILE *fp;
	int filesize = 0;
	int sendsize = 0;
	int packet_flag;
	unsigned char checksum = 0;
	unsigned char usocket_send_buf[1024+256] = {0};
	unsigned char buf[1024+256] = {0};
	//write_len = write(accept_fd,usocket_send_buf,16);
	if(NULL ==(fp = fopen("/home/ubuntu/dss_vt322/Profile.yml","r")))
	{
		perror("fopen\r\n");
		return -1;
	}

	fseek(fp,0,SEEK_END);
	filesize = ftell(fp);
	fseek(fp,0,SEEK_SET);

	usocket_send_buf[0] = 0xEB;
	usocket_send_buf[1] = 0x53;
	usocket_send_buf[4] = 0x33;
	usocket_send_buf[5] = filesize&0xff;
        usocket_send_buf[6] = (filesize>>8)&0xff;
        usocket_send_buf[7] = (filesize>>16)&0xff;
        usocket_send_buf[8] = (filesize>>24)&0xff;
	packet_flag = 0;

	while(len = fread(buf,1,1024,fp))
	{
		checksum = 0;
		if(len<0)
		{
		    printf("read error!\n");
		    break;
		}
		sendsize += len;
		if(packet_flag == 0)
		{
		    usocket_send_buf[9] = 0;
		    packet_flag = 1;
		}
		else if(sendsize == filesize)
		{
		    usocket_send_buf[9] = 2;
		}
		else
		{
		  usocket_send_buf[9] = 1;
		}
		usocket_send_buf[2] = (len+8)&0xff;
		usocket_send_buf[3] = ((len+8)>>8)&0xff;
		usocket_send_buf[10] = len&0xff;
		usocket_send_buf[11] = (len>>8)&0xff;
		memcpy(usocket_send_buf+12,buf, len);
		for(int m = 1; m<12+len;m++)
		    checksum ^= usocket_send_buf[m];
		usocket_send_buf[12+len] = checksum;
		
		write_len = write(accept_fd,usocket_send_buf,len+13);
		printf("write to usocket %d bytes:\n", write_len);
		for(int n = 0; n < write_len; n++)
			printf("%02x ", usocket_send_buf[n]);
		printf("\n");
		
	}
        if(sendsize == filesize)
        {
            printf("export success\n");
        }
        else
        {
            printf("export fail\n");
        }
	fclose(fp);
	
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
