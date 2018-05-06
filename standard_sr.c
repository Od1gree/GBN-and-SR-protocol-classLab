//
// Created by wangnaizheng on 2018/5/5.
//

#include "standard_sr.h"


#include "common.h"
#define STDIN 0

volatile sig_atomic_t running_standard = 1;

//用于在命令行执行时处理用户中断程序
void sig_handler_standard(int signum){
    printf("\n");
    running_standard = 0;
}
//客户端(作为发送方)
int client_main(int clntport, int servport, int maxSeqNo, int packetsNo, int timeout, int droprate){
    //丢包随机函数
    srand( (unsigned)time( NULL ) );
    struct sigaction a;
    a.sa_handler = sig_handler_standard;
    a.sa_flags = 0;
    sigemptyset(&a.sa_mask);
    sigaction(SIGINT, &a, NULL);

    struct sockaddr_in servaddr;
    struct sockaddr_in clntaddr;
    int sock;
    size_t size;

    int  missingSeq, windowSize, windowBegin;
    char *encodedMsg, *currentSeqChar;
    //初始化变量
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
    //为了避免乱序传输造成的麻烦,窗口大小为序列号总大小的一半
    //windowSize = (maxSeqNo + 1) / 2;
    windowSize = 1;
    windowBegin = 0;
    printf("Window size: %d\n", windowSize);

    int i;
    FRAME **frames;
    FRAME *currentFrame;
    //创建本地缓存空间
    frames = (FRAME **)malloc(sizeof(FRAME *) * windowSize);
    for(i = 0; i< windowSize; i++){
        frames[i] = malloc(sizeof(FRAME));
        frames[i]->data = malloc(sizeof(char)*100);
    }
    //初始化窗口
    FrameInit(frames, windowSize, maxSeqNo, windowBegin);
    //数据包存储空间
    packets = malloc(sizeof(char *)*packetsNo);
    for(i = 0; i < packetsNo; i++){
        packets[i] = malloc(sizeof(char)*10);
        sprintf(packets[i], "packet %d", i);
    }
    //Socket初始化等操作
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
    //发送数据包的循环
    while(running_standard){
        //如果已经成功接收,则滑动窗口
        if(isFrameAllAck(frames, windowSize)){
            windowBegin = (windowBegin + windowSize) % (maxSeqNo + 1);
            FrameInit(frames, windowSize, maxSeqNo, windowBegin);
            printf("WINDOW SLIDED!!!\n");
        }
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        //使用非阻塞套接字
        FD_ZERO(&readfds);
        FD_SET(STDIN, &readfds);
        FD_SET(sock, &readfds);
        //如果在特定时间内没有消息则返回0
        if(select(sock + 1, &readfds, NULL, NULL, &tv) != 0){
            if (FD_ISSET(STDIN, &readfds)){
                break;
            }
            //描述符是否准备好
            if (FD_ISSET(sock, &readfds)){
                //recv
                //check if nak
                //resend nak
                printf("Reply received\n");
                recvMsg[0] = '\0';
                type[0] = '\0';
                seqNo[0] = '\0';
                //接收到数据包
                if(recvUDP(sock, recvMsg, clntaddr) != 0){
                    statrecv++;
                    decode(type, seqNo, msg, recvMsg);
                    //匹配状态码
                    if(strcmp(type, "a") == 0){
                        statack++;
                        printf("ack received: %s\n",seqNo);
                        //窗口之外的直接丢弃
                        if(inWindow(maxSeqNo, windowSize, windowBegin, atoi(seqNo)))
                            for(i = 0; i < windowSize; i++){
//                                if(!frames[i]->ack){
//                                    currentFrame = frames[i];
//                                    printf("frame[%d] ACK\n", frames[i]->seqNo);
//                                    FrameAck(currentFrame);
//                                }
                                //确认收到的seq
                                if(FrameGetSeq(frames[i]) == atoi(seqNo) && !frames[i]->ack){
                                    currentFrame = frames[i];
                                    printf("frame[%d] ACK\n", frames[i]->seqNo);
                                    FrameAck(currentFrame);
                                    i = windowSize;
                                }
                            }
                    }
                    else if(strcmp(type, "n") == 0){
                        statnak++;
                        printf("nak received: %s\n", seqNo);
                        for(i = 0; i < windowSize; i++){
                            if(FrameGetSeq(frames[i]) == atoi(seqNo)){
//                                int j;
//                                for(j = 0; j < i; j++){
//                                    if(!frames[j]->ack){
//                                        FrameAck(frames[j]);
//                                        printf("frame[%d] ACK\n", frames[j]->seqNo);
//                                    }
//                                }
                                currentFrame = frames[i];
                                printf("frame[%d] NAK\n", frames[i]->seqNo);
                                i = windowSize;
                            }
                        }

                        printf("RESENDING NAK seqtofind: %s seq found: %d\n", seqNo, currentFrame->seqNo);
                        //重新传输nak的frame
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
            //随机丢弃
            if((rand()%100 < droprate)){
                printf("build a timeout packet");
            }
            else if(sendto(sock, encodedMsg, strlen(encodedMsg), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
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

    //所有数据包传输完成后展示统计数据
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


int server_main(unsigned short port, unsigned short clntport, int maxSeqNo){
    pthread_t threadID;
    struct sigaction a;
    a.sa_handler = sig_handler_standard;
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

    //windowSize = (maxSeqNo + 1) / 2;
    windowSize = 1;
    windowBegin = 0;
    printf("Window size: %d\n", windowSize);
    //初始化滑动窗口
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

    while(running_standard){
        //检测窗口是否该滑动
        if(isFrameAllAck(frames, windowSize)){
            windowBegin = (windowBegin + windowSize) % (maxSeqNo + 1);
            FrameInit(frames, windowSize, maxSeqNo, windowBegin);
            framecount = 0;
            //printf("WINDOW SLIDED!!!\n");
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
        //接收内容在滑动窗口内
        if(inWindow(maxSeqNo, windowSize, windowBegin, currentSeqNo)){
            for(i = 0; i < windowSize; i++){
                if(FrameGetSeq(frames[i]) == currentSeqNo){
                    frameNo = i;
                    FrameSet(frames[i], 1, msg);
                    break;
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
            //被阻塞的情况
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
            //重新确认已经收到
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