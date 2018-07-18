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

int main(int argc,char **argv)
{
	int pid;
	int sockfd, new_fd,recv_len,write_len;
	socklen_t len;
	char buf[1024];
	int datalen, maxfd;
	int i,j;
	fd_set read_list,read_list_val;
	int flag,listen_number=5;
	FILE *fp;

	struct sockaddr_in addr,src_addr;

	int addrlen;

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
	printf("Wait for new connect......\r\n");

	while(1)
	{
		if(NULL ==(fp = fopen("dss_pkt.tar.gz","w")))
		{
			perror("fopen\r\n");
			return -1;
		}
		printf("Wait for next connect......\r\n");
		len = sizeof(struct sockaddr);
		if(-1== (new_fd = accept(sockfd,(struct sockaddr *)&src_addr,&len)))
		{
			perror("accept");
			return -1;	
		}	
		else
			printf("Now new Client connect in :socketfd = %d\r\n",new_fd);

		strcpy(buf,"Please Click the Select PushButton to select your *.tar.gz file:");  
		strcat(buf,"\n");  

		//    send(new_fd,buf,1024,0); 

		bzero(buf,1024);
		int flagg=1;
		while(flagg)
		{
			printf("waiting recv...\n");
			recv_len = read(new_fd,buf,1024);
			printf("read return recv_len=%d\n");
			{	
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
				printf(">>>> Receiving packets : datalen = %d\r\n",recv_len);

				write_len = fwrite(buf,1,recv_len,fp);
				printf("write_len=%d!!!\n",write_len);
				if(write_len < recv_len)
				{
					printf("Write file error!\r\n");
					break;
				}		
				bzero(buf,1024);
				printf("bzero end...\n");
			}
		}		

		printf("Receive file: dss_pkt.tar.gz  OK !!\r\n");
		fclose(fp);	
		fw_update_runtar();
//		system("ls -al /home/ubuntu/dss_pkt");
//		bzero(buf,1024);
//		strcpy(buf,"upgrade success!");  
//		strcat(buf,"\n");  
//		send(new_fd,buf,1024,0); 
		close(new_fd);
	}
	close(sockfd);
	return 0;
}

static int fw_update_runtar(void)
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
		printf(" command tar execute success!\r\n");
	return iRtn;
}


