#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"
#define STDIN 0

volatile sig_atomic_t running = 1;

void sig_handler(int signum){
    printf("\n");
    running = 0;
}

int client_main(int clntport, int servport, int maxSeqNo, int packetsNo, int timeout){
    struct sigaction a;
    a.sa_handler = sig_handler;
    a.sa_flags = 0;
    sigemptyset(&a.sa_mask);
    sigaction(SIGINT, &a, NULL);

    struct sockaddr_in servaddr;
    struct sockaddr_in clntaddr;
    int sock;
    size_t size;

    int  missingSeq, windowSize, windowBegin;
    char *encodedMsg, *currentSeqChar;

    encodedMsg = malloc(sizeof(char)*100);
    currentSeqChar = malloc(sizeof(char)*3);

    int packetCount = 0;
    char **packets;
    char *recvMsg = malloc(sizeof(char)*100);
    char *type = malloc(sizeof(char));
    char *seqNo = malloc(sizeof(char)*3);
    char *msg = malloc(sizeof(char)*100);

    int statsend = 0;
    int statrecv = 0;
    int statack = 0;
    int statnak = 0;
    int stattimeout = 0;

    windowSize = (maxSeqNo + 1) / 2;
    windowBegin = 0;
    printf("Window size: %d\n", windowSize);


    int i;
    FRAME **frames;
    FRAME *currentFrame;
    frames = (FRAME **)malloc(sizeof(FRAME *) * windowSize);
    for(i = 0; i< windowSize; i++){
        frames[i] = malloc(sizeof(FRAME));
        frames[i]->data = malloc(sizeof(char)*100);
    }
    FrameInit(frames, windowSize, maxSeqNo, windowBegin);

    packets = malloc(sizeof(char *)*packetsNo);
    for(i = 0; i < packetsNo; i++){
        packets[i] = malloc(sizeof(char)*10);
        sprintf(packets[i], "packet %d", i);
    }

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("cannot create socket");
        exit(1);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(servport);

    memset(&clntaddr, 0, sizeof(clntaddr));
    clntaddr.sin_family = AF_INET;
    clntaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    clntaddr.sin_port = htons(clntport);

    if(bind(sock, (struct sockaddr *)&clntaddr, sizeof(clntaddr)) < 0){
        fprintf(stderr, "bind failed\n");
        exit(1);
    }

    struct timeval tv;
    fd_set readfds;

    printf("testclient running\n");

    while(running){
        if(isFrameAllAck(frames, windowSize)){
            windowBegin = (windowBegin + windowSize) % (maxSeqNo + 1);
            FrameInit(frames, windowSize, maxSeqNo, windowBegin);
            printf("WINDOW SLIDED!!!\n");
        }
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        FD_ZERO(&readfds);
        FD_SET(STDIN, &readfds);
        FD_SET(sock, &readfds);

        if(select(sock + 1, &readfds, NULL, NULL, &tv) != 0){
            if (FD_ISSET(STDIN, &readfds)){
                break;
            }
            if (FD_ISSET(sock, &readfds)){
                //recv
                //check if nak
                //resend nak
                printf("Reply received\n");
                recvMsg[0] = '\0';
                type[0] = '\0';
                seqNo[0] = '\0';
                if(recvUDP(sock, recvMsg, clntaddr) != 0){
                    statrecv++;
                    decode(type, seqNo, msg, recvMsg);
                    if(strcmp(type, "a") == 0){
                        statack++;
                        printf("ack received: %s\n",seqNo);
                        if(inWindow(maxSeqNo, windowSize, windowBegin, atoi(seqNo)))
                            for(i = 0; i < windowSize; i++){
                                if(!frames[i]->ack){
                                    currentFrame = frames[i];
                                    printf("frame[%d] ACK\n", frames[i]->seqNo);
                                    FrameAck(currentFrame);
                                }
                                if(FrameGetSeq(frames[i]) == atoi(seqNo)){
                                    i = windowSize;
                                }
                            }
                    }
                    else if(strcmp(type, "n") == 0){
                        statnak++;
                        printf("nak received: %s\n", seqNo);
                        for(i = 0; i < windowSize; i++){
                            if(FrameGetSeq(frames[i]) == atoi(seqNo)){
                                int j;
                                for(j = 0; j < i; j++){
                                    if(!frames[j]->ack){
                                        FrameAck(frames[j]);
                                        printf("frame[%d] ACK\n", frames[j]->seqNo);
                                    }
                                }
                                currentFrame = frames[i];
                                i = windowSize;
                                printf("frame[%d] NAK\n", frames[i]->seqNo);
                            }
                        }

                        printf("RESENDING NAK seqtofind: %s seq found: %d\n", seqNo, currentFrame->seqNo);

                        if (sendto(sock, currentFrame->data, strlen(currentFrame->data), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
                            perror("sendto failed\n");
                            exit(1);
                        }
                        statsend++;
                        printf("Nak resent to port: %i\n", htons(servaddr.sin_port));
                    }
                }
                else
                    break;
            }
        }
        //send new packets when frames available
        if(packetCount < packetsNo &&
           nextFrame(frames, windowSize, &currentFrame) == 1){
            sprintf(currentSeqChar, "%d", FrameGetSeq(currentFrame));
            encodedMsg[0] = '\0';
            encode("d", currentSeqChar, packets[packetCount], encodedMsg);
            FrameSet(currentFrame, 0, encodedMsg);
            if (sendto(sock, encodedMsg, strlen(encodedMsg), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
                perror("sendto failed\n");
                exit(1);
            }
            statsend++;
            printf("Frame[%d] sent to port: %i\n", FrameGetSeq(currentFrame), htons(servaddr.sin_port));
            packetCount ++;
        }
        //Check frames timeout
        for(i = 0; i < windowSize; i++){
            if(frames[i]->inUse &&
               !frames[i]->ack &&
               isFrameTimeOut(frames[i], timeout)){
                stattimeout++;
                statsend++;
                printf("Resend timeout frames[%d]\n", frames[i]->seqNo);
                if (sendto(sock, frames[i]->data, strlen(frames[i]->data), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
                    perror("sendto failed\n");
                    exit(1);
                }
                FrameResetTimeout(frames[i]);
            }
        }
        //check if last packet
        if(packetCount >= packetsNo && isFrameUsedAck(frames, windowSize))
            break;
    }
    printf("Shutting down testclient\n");

    printf("Statistics:\n");
    printf("Total sends: %d\n", statsend);
    printf("Total receives: %d\n", statrecv);
    printf("Total ack: %d\n", statack);
    printf("Total nak: %d\n", statnak);
    printf("Total timeouts: %d\n", stattimeout);

    for(i = 0; i < packetsNo; i++){
        free(packets[i]);
    }
    for(i = 0; i < windowSize; i++){
        free(frames[i]->data);
        free(frames[i]);
    }
    free(packets);
    free(frames);

    free(encodedMsg);
    free(currentSeqChar);
    free(recvMsg);
    free(type);
    free(seqNo);
    free(msg);
    close(sock);
    return 0;
}

int CreateUDPSocket(unsigned short port){
    int sock;
    struct sockaddr_in servAddr;

    if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        fprintf(stderr, "socket create failed\n");
        exit(1);
    }

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);

    if(bind(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0){
        fprintf(stderr, "socket bind failed\n");
        exit(1);
    }

    return sock;
}


int server_main(unsigned short port, unsigned short clntport, int maxSeqNo){
    pthread_t threadID;
    struct sigaction a;
    a.sa_handler = sig_handler;
    a.sa_flags = 0;
    sigemptyset(&a.sa_mask);
    sigaction(SIGINT, &a, NULL);

    int servSock;
    int toPort;
    struct sockaddr_in clntaddr;

    int windowSize, windowBegin, currentSeqNo, lastSeqNo, block;
    char *recvMsg = malloc(sizeof(char)*100);
    char *type = malloc(sizeof(char));
    char *seqNo = malloc(sizeof(char)*3);
    char *missingSeq = malloc(sizeof(char)*3);
    char *msg = malloc(sizeof(char)*100);
    char *replyMsg = malloc(sizeof(char)*100);
    struct FRAME **frames;
    struct FRAME *currentFrame;

    int statsend = 0;
    int statrecv = 0;
    int statack = 0;
    int statnak = 0;

    windowSize = (maxSeqNo + 1) / 2;
    windowBegin = 0;
    printf("Window size: %d\n", windowSize);
    frames = (FRAME **)malloc(sizeof(FRAME *) * windowSize);
    int i;
    for(i = 0; i < windowSize; i++){
        frames[i] = malloc(sizeof(FRAME));
        frames[i]->data = malloc(sizeof(char)*100);
    }
    FrameInit(frames, windowSize, maxSeqNo, windowBegin);
    currentFrame = frames[0];

    servSock = CreateUDPSocket(port);

    printf("testserver running\n");

    currentSeqNo = -1;
    lastSeqNo = -1;
    block = 0;
    int framecount = 0;
    int frameNo;

    while(running){
        if(isFrameAllAck(frames, windowSize)){
            windowBegin = (windowBegin + windowSize) % (maxSeqNo + 1);
            FrameInit(frames, windowSize, maxSeqNo, windowBegin);
            framecount = 0;
            printf("WINDOW SLIDED!!!\n");
        }
        frameNo = -1;
        recvMsg[0] = '\0';
        type[0] = '\0';
        seqNo[0] = '\0';
        msg[0] = '\0';
        toPort = recvUDP(servSock, recvMsg, clntaddr);
        if(toPort == 0)
            break;
        statrecv++;
        clntaddr.sin_family = AF_INET;
        clntaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        clntaddr.sin_port = htons(clntport);
        decode(type, seqNo, msg, recvMsg);
        currentSeqNo = atoi(seqNo);
        if(inWindow(maxSeqNo, windowSize, windowBegin, currentSeqNo)){
            for(i = 0; i < windowSize; i++){
                if(FrameGetSeq(frames[i]) == currentSeqNo){
                    frameNo = i;
                    FrameSet(frames[i], 1, msg);
                }
            }
            if(frameNo == -1){
                fprintf(stderr, "frame not found in window\n");
                fprintf(stderr, "Seq Recved: %s", seqNo);
                exit(1);
            }

            printf("Message received - frameNo: %d seqNo: %d\n", frameNo, currentSeqNo);
            printf("type: %s seqNo: %d msg: %s\n", type, currentSeqNo, msg);

            if(frameNo - framecount <= 0){
                if(frameNo < framecount || !block){
                    //send ack
                    encode("a", seqNo, "", replyMsg);
                    statsend++;
                    statack++;
                    if(sendto(servSock, replyMsg, strlen(replyMsg), 0, (struct sockaddr *)&clntaddr, sizeof(clntaddr)) < 0){
                        fprintf(stderr, "server send ack failed: %s\n", strerror(errno));
                        exit(1);
                    }
                    //else
                    printf("Ack[%s] sent to client: %d\n", seqNo, ntohs(clntaddr.sin_port));
                    if(frameNo == framecount){
                        printf("TO NETWORK LAYER: %s\n", frames[framecount]->data);
                        framecount++;
                        lastSeqNo = currentSeqNo;
                    }
                }
                else if(block && frameNo == framecount){
                    printf("TO NETWORK LAYER: %s\n", frames[framecount]->data);
                    lastSeqNo = frames[framecount]->seqNo;
                    framecount++;
                    while(framecount < windowSize && frames[framecount]->inUse){
                        printf("TO NETWORK LAYER: %s\n", frames[framecount]->data);
                        lastSeqNo = frames[framecount]->seqNo;
                        framecount++;
                    }
                    if(framecount == windowSize){
                        //send ack
                        statsend++;
                        statack++;
                        sprintf(seqNo, "%d", lastSeqNo);
                        encode("a", seqNo, "", replyMsg);
                        if(sendto(servSock, replyMsg, strlen(replyMsg), 0, (struct sockaddr *)&clntaddr, sizeof(clntaddr)) < 0){
                            fprintf(stderr, "server send ack failed: %s\n", strerror(errno));
                            exit(1);
                        }
                        //else
                        printf("Ack[%s] sent to client: %d\n", seqNo, ntohs(clntaddr.sin_port));
                        block = 0;
                    }else{
                        //send nak
                        statsend++;
                        statnak++;
                        if(lastSeqNo < maxSeqNo)
                            sprintf(missingSeq, "%d", lastSeqNo+1);
                        else
                            sprintf(missingSeq, "0");
                        encode("n", missingSeq, "", replyMsg);
                        if(sendto(servSock, replyMsg, strlen(replyMsg), 0, (struct sockaddr *)&clntaddr, sizeof(clntaddr)) < 0){
                            fprintf(stderr, "server send ack failed: %s\n", strerror(errno));
                            exit(1);
                        }
                        //else
                        printf("Nck[%s] sent to client: %d\n",missingSeq, ntohs(clntaddr.sin_port));
                    }

                }
            }
            else if(!block){
                block = 1;
                if(frameNo > framecount){
                    //send nak
                    statnak++;
                    statsend++;
                    if(lastSeqNo < maxSeqNo)
                        sprintf(missingSeq, "%d", lastSeqNo+1);
                    else
                        sprintf(missingSeq, "0");
                    encode("n", missingSeq, "", replyMsg);
                    if(sendto(servSock, replyMsg, strlen(replyMsg), 0, (struct sockaddr *)&clntaddr, sizeof(clntaddr)) < 0){
                        fprintf(stderr, "server send ack failed: %s\n", strerror(errno));
                        exit(1);
                    }
                    //else
                    printf("Nck[%s] sent to client: %d\n", missingSeq, ntohs(clntaddr.sin_port));
                }
            }
        }
        else{
            encode("a", seqNo, "", replyMsg);
            //send ack
            statsend++;
            statack++;
            if(sendto(servSock, replyMsg, strlen(replyMsg), 0, (struct sockaddr *)&clntaddr, sizeof(clntaddr)) < 0){
                fprintf(stderr, "server send ack failed: %s\n", strerror(errno));
                exit(1);
            }
            //else
            printf("Ack sent to client: %d\n", ntohs(clntaddr.sin_port));
        }
    }

    printf("Shutting down testserver\n");

    printf("Statistics:\n");
    printf("Total receives: %d\n", statrecv);
    printf("Total sends: %d\n", statsend);
    printf("Total ack sent: %d\n", statack);
    printf("Total nak sent: %d\n", statnak);

    for(i = 0; i < windowSize; i++){
        free(frames[i]->data);
        free(frames[i]);
    }
    free(frames);
    free(recvMsg);
    free(type);
    free(seqNo);
    free(msg);
    free(replyMsg);
    close(servSock);
    exit(0);
}

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
                int flag = 0;
                printf("please input local port:\n");
                scanf("%hu",&port);
                printf("please input remote port:\n");
                scanf("%hu",&clntport);
                printf("please input sequence number\n");
                scanf("%d",&maxSeqNo);
                if(fork() == 0){
                    server_main(port,clntport,maxSeqNo);
                    flag = 1;
                }
                if(flag == 1)
                    exit(0);
                break;
            }
            case 2:{
                int clntport;
                int servport;
                int maxSeqNo;
                int packetsNo;
                int timeout;
                int flag = 0;
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
                if(fork() == 0){
                    client_main(clntport,servport,maxSeqNo,packetsNo,timeout);
                    flag = 1;
                }
                if(flag == 1)
                    exit(0);
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