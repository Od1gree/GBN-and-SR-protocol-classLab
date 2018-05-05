#include <stdio.h>
#include <stdlib.h>
#include "standard_sr.h"

int main(int argc, char *argv[])
{
//    int server_sockfd;
//    int len;
//    struct sockaddr_in my_addr;   //服务器网络地址结构体
//    struct sockaddr_in remote_addr; //客户端网络地址结构体
//    int sin_size;
//    char buf[BUFSIZ];  //数据传送的缓冲区
//    memset(&my_addr,0,sizeof(my_addr)); //数据初始化--清零
//    my_addr.sin_family=AF_INET; //设置为IP通信
//    my_addr.sin_addr.s_addr=INADDR_ANY;//服务器IP地址--允许连接到所有本地地址上
//    my_addr.sin_port=htons(8000); //服务器端口号
//
//    /*创建服务器端套接字--IPv4协议，面向无连接通信，UDP协议*/
//    if((server_sockfd=socket(PF_INET,SOCK_DGRAM,0))<0)
//    {
//        perror("socket error");
//        return 1;
//    }
//
//    /*将套接字绑定到服务器的网络地址上*/
//    if (bind(server_sockfd,(struct sockaddr *)&my_addr,sizeof(struct sockaddr))<0)
//    {
//        perror("bind error");
//        return 1;
//    }
//    sin_size=sizeof(struct sockaddr_in);
//    printf("waiting for a packet...\n");
//
//    /*接收客户端的数据并将其发送给客户端--recvfrom是无连接的*/
//    if((len=recvfrom(server_sockfd,buf,BUFSIZ,0,(struct sockaddr *)&remote_addr,&sin_size))<0)
//    {
//        perror("recvfrom error");
//        return 1;
//    }
//    printf("received packet from %s:\n",inet_ntoa(remote_addr.sin_addr));
//    buf[len]='\0';
//    printf("contents: %s\n",buf);
//
//    /*关闭套接字*/
//    close(server_sockfd);
    while(1){
        printf("Please input the number to do:\n"
               "1.create a thread to receive packages.\n" //as a server
               "2.create a thread to send packages.\n" //as a client
               "3.exit program");
        int function = 0;
        scanf("%d",&function);
        switch(function){
            case 1:{
                unsigned short port;
                unsigned short clntport;
                int maxSeqNo;
                printf("please input local port:\n");
                scanf("%hu",&port);
                printf("please input remote port:\n");
                scanf("%hu",&clntport);
                printf("please input sequence number\n");
                scanf("%d",&maxSeqNo);
                server_main(port,clntport,maxSeqNo);
                break;
            }
            case 2:{
                int clntport;
                int servport;
                int maxSeqNo;
                int packetsNo;
                int timeout;
                int droprate;
                printf("please input local port:\n");
                scanf("%d",&clntport);
                printf("please input remote port:\n");
                scanf("%d",&servport);
                printf("please input sequence number\n");
                scanf("%d",&maxSeqNo);
                printf("please input number of packs:\n");
                scanf("%d",&packetsNo);
                printf("please input time out in ms:\n");
                scanf("%d",&timeout);
                printf("please input droprate in int percent:\n");
                scanf("%d",&droprate);
                client_main(clntport,servport,maxSeqNo,packetsNo,timeout, droprate);
                break;
            }
            case 3:
                exit(0);
            default:
                printf("input error\n");
        }
    }

    return 0;
}