//
// Created by wangnaizheng on 2018/5/5.
//

#include "improved_sr.h"
#include "common.h"
#define STDIN 0
volatile sig_atomic_t running_imporved = 1;

//用于在命令行执行时处理用户中断程序
void sig_handler_improved(int signum){
    printf("\n");
    running_imporved = 0;
}

int combined_main_init(int clntport, int servport, int maxSeqNo,  int packetsNo, int droprate, int timeout){
    srand( (unsigned)time( NULL ) );
    struct sigaction a;
    a.sa_handler = sig_handler_improved;
    a.sa_flags = 0;
    sigemptyset(&a.sa_mask);
    sigaction(SIGINT, &a, NULL);

    struct sockaddr_in servaddr;
    struct sockaddr_in clntaddr;
    int sock;
    size_t size;

    int  missingSeq, windowSize, windowBegin, currentSeqNo;
    char *encodedMsg, *currentSeqChar;
    //初始化变量
    encodedMsg = malloc(sizeof(char)*100);
    currentSeqChar = malloc(sizeof(char)*3);

    int packetCount = 0;
    char **packets;
    char *recvMsg = malloc(sizeof(char)*100);
    char *type_recv = malloc(sizeof(char));
    char *seqNo_recv = malloc(sizeof(char)*3);
    char *type_send = malloc(sizeof(char));
    char *seqNo_send = malloc(sizeof(char)*3);
    char *msg = malloc(sizeof(char)*100);

    int statsend = 0;
    int statrecv = 0;
    int statack = 0;
    int statnak = 0;
    int stattimeout = 0;
    //为了避免乱序传输造成的麻烦,窗口大小为序列号总大小的一半
    windowSize = (maxSeqNo + 1) / 2;
    windowBegin = 0;
    printf("Window size: %d\n", windowSize);

    int i;
    FRAME **frames_recv;
    FRAME *currentFrame_recv;
    struct FRAME **frames_send;
    //创建本地需要发送的消息的窗口
    frames_recv = (FRAME **)malloc(sizeof(FRAME *) * windowSize);
    for(i = 0; i< windowSize; i++){
        frames_recv[i] = malloc(sizeof(FRAME));
        frames_recv[i]->data = malloc(sizeof(char)*100);
    }
    //初始化窗口
    FrameInit(frames_recv, windowSize, maxSeqNo, windowBegin);
    //创建接收消息的缓存空间
    frames_send = (FRAME **)malloc(sizeof(FRAME *) * windowSize);
    for(i = 0; i < windowSize; i++){
        frames_send[i] = malloc(sizeof(FRAME));
        frames_send[i]->data = malloc(sizeof(char)*100);
    }
    FrameInit(frames_send, windowSize, maxSeqNo, windowBegin);
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

    currentSeqNo = -1;
    int lastSeqNo = -1;
    int block = 0;
    int framecount = 0;
    int frameNo;
    printf("testclient running\n");
    //发送数据包的循环
    while(running_imporved){
        //如果已经成功接收,则滑动窗口
        if(isFrameAllAck(frames_recv, windowSize)){
            windowBegin = (windowBegin + windowSize) % (maxSeqNo + 1);
            FrameInit(frames_recv, windowSize, maxSeqNo, windowBegin);
            printf("WINDOW SLIDED!!!\n");
        }
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        char *data_to_be_send = NULL;
        char *ack_to_be_send = NULL;
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
                type_recv[0] = '\0';
                seqNo_recv[0] = '\0';
                type_send[0] = '\0';
                seqNo_send[0] = '\0';
                //接收到数据包
                if(recvUDP(sock, recvMsg, clntaddr) != 0){
                    statrecv++;
                    //recv为接收数据的状态码, send为发送的数据报文
                    decode_improve(type_recv,seqNo_recv,type_send,seqNo_send,msg,recvMsg);
                    //匹配状态码
                    if(strcmp(type_recv, "a") == 0){
                        statack++;
                        printf("ack received: %s\n",seqNo_recv);
                        //窗口之外的直接丢弃
                        if(inWindow(maxSeqNo, windowSize, windowBegin, atoi(seqNo_recv)))
                            for(i = 0; i < windowSize; i++){
//                                if(!frames[i]->ack){
//                                    currentFrame = frames[i];
//                                    printf("frame[%d] ACK\n", frames[i]->seqNo);
//                                    FrameAck(currentFrame);
//                                }
                                //确认收到的seq
                                if(FrameGetSeq(frames_recv[i]) == atoi(seqNo_recv) && !frames_recv[i]->ack){
                                    currentFrame_recv = frames_recv[i];
                                    printf("frame[%d] ACK\n", frames_recv[i]->seqNo);
                                    FrameAck(currentFrame_recv);
                                    i = windowSize;
                                }
                            }
                    }
                    else if(strcmp(type_recv, "n") == 0){
                        statnak++;
                        printf("nak received: %s\n", seqNo_recv);
                        for(i = 0; i < windowSize; i++){
                            if(FrameGetSeq(frames_recv[i]) == atoi(seqNo_recv)){
//                                int j;
//                                for(j = 0; j < i; j++){
//                                    if(!frames[j]->ack){
//                                        FrameAck(frames[j]);
//                                        printf("frame[%d] ACK\n", frames[j]->seqNo);
//                                    }
//                                }
                                currentFrame_recv = frames_recv[i];
                                printf("frame[%d] NAK\n", frames_recv[i]->seqNo);
                                i = windowSize;
                            }
                        }

                        printf("RESENDING NAK seqtofind: %s seq found: %d\n", seqNo_recv, currentFrame_recv->seqNo);
                        //重新传输nak的frame
//                        if (sendto(sock, currentFrame_recv->data, strlen(currentFrame_recv->data), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
//                            perror("sendto failed\n");
//                            exit(1);
//                        }
                        data_to_be_send = currentFrame_recv->data;
                        statsend++;
                        printf("Nak resent to port: %i\n", htons(servaddr.sin_port));
                    }


                    char *replyMsg = malloc(sizeof(char) * 100);
                    currentSeqNo = atoi(seqNo_send);
                    //接收内容在滑动窗口内
                    if(inWindow(maxSeqNo, windowSize, windowBegin, currentSeqNo)){
                        for(i = 0; i < windowSize; i++){
                            if(FrameGetSeq(frames_send[i]) == currentSeqNo){
                                frameNo = i;
                                FrameSet(frames_send[i], 1, msg);
                                break;
                            }
                        }
                        if(frameNo == -1){
                            fprintf(stderr, "frame not found in window\n");
                            fprintf(stderr, "Seq Recved: %s", seqNo_send);
                            exit(1);
                        }

                        printf("Message received - frameNo: %d seqNo: %d\n", frameNo, currentSeqNo);
                        printf("type: %s seqNo: %d msg: %s\n", type_send, currentSeqNo, msg);

                        if(frameNo - framecount <= 0){
                            if(frameNo < framecount || !block){
                                //send ack

                                encode_combine("a", seqNo_send, replyMsg);
                                statsend++;
                                statack++;
//                                if(sendto(servSock, replyMsg, strlen(replyMsg), 0, (struct sockaddr *)&clntaddr, sizeof(clntaddr)) < 0){
//                                    fprintf(stderr, "server send ack failed: %s\n", strerror(errno));
//                                    exit(1);
//                                }
                                ack_to_be_send = replyMsg;
                                //else
                                printf("Ack[%s] sent to client: %d\n", seqNo_send, ntohs(clntaddr.sin_port));
                                if(frameNo == framecount){
                                    printf("TO NETWORK LAYER: %s\n", frames_send[framecount]->data);
                                    framecount++;
                                    lastSeqNo = currentSeqNo;
                                }
                            }
                            else if(block && frameNo == framecount){
                                printf("TO NETWORK LAYER: %s\n", frames_send[framecount]->data);
                                lastSeqNo = frames_send[framecount]->seqNo;
                                framecount++;
                                while(framecount < windowSize && frames_send[framecount]->inUse){
                                    printf("TO NETWORK LAYER: %s\n", frames_send[framecount]->data);
                                    lastSeqNo = frames_send[framecount]->seqNo;
                                    framecount++;
                                }
                                if(framecount == windowSize){
                                    //send ack
                                    statsend++;
                                    statack++;
                                    sprintf(seqNo_send, "%d", lastSeqNo);
                                    encode_combine("a", seqNo_send, replyMsg);
//                                    if(sendto(servSock, replyMsg, strlen(replyMsg), 0, (struct sockaddr *)&clntaddr, sizeof(clntaddr)) < 0){
//                                        fprintf(stderr, "server send ack failed: %s\n", strerror(errno));
//                                        exit(1);
//                                    }
                                    ack_to_be_send = replyMsg;
                                    //else
                                    printf("Ack[%s] sent to client: %d\n", seqNo_send, ntohs(clntaddr.sin_port));
                                    block = 0;
                                }
                                else{
                                    //send nak
                                    statsend++;
                                    statnak++;
                                    if(lastSeqNo < maxSeqNo)
                                        sprintf(missingSeq, "%d", lastSeqNo+1);
                                    else
                                        sprintf(missingSeq, "0");
                                    encode_combine("n", missingSeq, replyMsg);
//                                    if(sendto(servSock, replyMsg, strlen(replyMsg), 0, (struct sockaddr *)&clntaddr, sizeof(clntaddr)) < 0){
//                                        fprintf(stderr, "server send ack failed: %s\n", strerror(errno));
//                                        exit(1);
//                                    }
                                    ack_to_be_send = replyMsg;
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
                                encode_combine("n", missingSeq, replyMsg);
//                                if(sendto(servSock, replyMsg, strlen(replyMsg), 0, (struct sockaddr *)&clntaddr, sizeof(clntaddr)) < 0){
//                                    fprintf(stderr, "server send ack failed: %s\n", strerror(errno));
//                                    exit(1);
//                                }
                                ack_to_be_send = replyMsg;
                                //else
                                printf("Nck[%s] sent to client: %d\n", missingSeq, ntohs(clntaddr.sin_port));
                            }
                        }
                    }
                    else{
                        //重新确认已经收到
                        encode_combine("a", seqNo_send, replyMsg);
                        //send ack
                        statsend++;
                        statack++;
//                        if(sendto(servSock, replyMsg, strlen(replyMsg), 0, (struct sockaddr *)&clntaddr, sizeof(clntaddr)) < 0){
//                            fprintf(stderr, "server send ack failed: %s\n", strerror(errno));
//                            exit(1);
//                        }
                        ack_to_be_send = replyMsg;
                        //else
                        printf("Ack sent to client: %d\n", ntohs(clntaddr.sin_port));
                    }
                    free(replyMsg);



                }
                else
                    break;
            }
        }
        //send new packets when frames available
        if(packetCount < packetsNo && data_to_be_send == NULL &&
           nextFrame(frames_recv, windowSize, &currentFrame_recv) == 1 ){
            sprintf(currentSeqChar, "%d", FrameGetSeq(currentFrame_recv));
            encodedMsg[0] = '\0';
            encode("d", currentSeqChar, packets[packetCount], encodedMsg);
            FrameSet(currentFrame_recv, 0, encodedMsg);
            data_to_be_send = encodedMsg;
            //随机丢弃
            if((rand()%100 < droprate)){
                printf("build a timeout packet");
            }
//            else if(sendto(sock, encodedMsg, strlen(encodedMsg), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
//                perror("sendto failed\n");
//                exit(1);
//            }
            data_to_be_send = encodedMsg;
            statsend++;
            printf("Frame[%d] sent to port: %i\n", FrameGetSeq(currentFrame_recv), htons(servaddr.sin_port));
            packetCount ++;
        }
        //Check frames timeout
        for(i = 0; i < windowSize; i++){
            if(frames_recv[i]->inUse &&
               !frames_recv[i]->ack &&
               isFrameTimeOut(frames_recv[i], timeout)){
                stattimeout++;
                statsend++;
                printf("Resend timeout frames[%d]\n", frames_recv[i]->seqNo);
                //超时的数据直接发送,不需要等待,但是需要构造前两项为空
                if(ack_to_be_send == NULL) {
                    char *recvMsg = malloc(sizeof(char) * 10);
                    encode_combine("", "", recvMsg);
                    ack_to_be_send = recvMsg;
                }
//                if (sendto(sock, frames_recv[i]->data, strlen(frames_recv[i]->data), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
//                    perror("sendto failed\n");
//                    exit(1);
//                }
                data_to_be_send = frames_recv[i]->data;
                FrameResetTimeout(frames_recv[i]);
                break;
            }
        }
        //check if last packet
        if(packetCount >= packetsNo && isFrameUsedAck(frames_recv, windowSize))
            break;
        char *final_data = malloc(sizeof(char)*100);
        encode_combine(ack_to_be_send,data_to_be_send,final_data);
        if (sendto(sock, final_data, strlen(final_data), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
            perror("sendto failed\n");
            exit(1);
        }

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
        free(frames_recv[i]->data);
        free(frames_recv[i]);
    }
    free(packets);
    free(frames_recv);
    free(frames_send);

    free(encodedMsg);
    free(currentSeqChar);
    free(recvMsg);
    free(type_recv);
    free(seqNo_recv);
    free(type_send);
    free(type_send);
    free(msg);
    close(sock);
    return 0;
}

int combined_main_pass(int port, int clntport, int maxSeqNo, int packetNo, int droprate){
    pthread_t threadID;
    struct sigaction a;
    a.sa_handler = sig_handler_improved;
    a.sa_flags = 0;
    sigemptyset(&a.sa_mask);
    sigaction(SIGINT, &a, NULL);

    int servSock;
    int toPort;
    struct sockaddr_in clntaddr;

    int windowSize, windowBegin, currentSeqNo, lastSeqNo, block;
    char *recvMsg = malloc(sizeof(char)*100);
    char *type_send = malloc(sizeof(char));
    char *seqNo_send = malloc(sizeof(char)*3);
    char *seqNo_recv = malloc(sizeof(char)*3);
    char *missingSeq = malloc(sizeof(char)*3);
    char *msg = malloc(sizeof(char)*100);
    char *replyMsg = malloc(sizeof(char)*100);
    struct FRAME **frames_send;
    struct FRAME *currentFrame;

    int statsend = 0;
    int statrecv = 0;
    int statack = 0;
    int statnak = 0;

    windowSize = (maxSeqNo + 1) / 2;
    windowBegin = 0;
    printf("Window size: %d\n", windowSize);
    //初始化滑动窗口
    frames_send = (FRAME **)malloc(sizeof(FRAME *) * windowSize);
    int i;
    for(i = 0; i < windowSize; i++){
        frames_send[i] = malloc(sizeof(FRAME));
        frames_send[i]->data = malloc(sizeof(char)*100);
    }
    FrameInit(frames_send, windowSize, maxSeqNo, windowBegin);
    currentFrame = frames_send[0];

    char **packets = malloc(sizeof(char *)*packetNo);
    for(i = 0; i < packetNo; i++){
        packets[i] = malloc(sizeof(char)*10);
        sprintf(packets[i], "data content, server packet %d", i);
    }
    int packet_count = -0;

    servSock = CreateUDPSocket(port);

    printf("testserver running\n");

    currentSeqNo = -1;
    lastSeqNo = -1;
    block = 0;
    int framecount = 0;
    int frameNo;

    while(1){
        //检测窗口是否该滑动
        if(isFrameAllAck(frames_send, windowSize)){
            windowBegin = (windowBegin + windowSize) % (maxSeqNo + 1);
            FrameInit(frames_send, windowSize, maxSeqNo, windowBegin);
            framecount = 0;
            printf("WINDOW SLIDED!!!\n");
        }
        frameNo = -1;
        recvMsg[0] = '\0';
        type_send[0] = '\0';
        seqNo_send[0] = '\0';
        msg[0] = '\0';
        toPort = recvUDP(servSock, recvMsg, clntaddr);
        if(toPort == 0)
            break;
        statrecv++;
        clntaddr.sin_family = AF_INET;
        clntaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        clntaddr.sin_port = htons(clntport);
        decode(type_send, seqNo_send, msg, recvMsg);
        currentSeqNo = atoi(seqNo_send);
        //接收内容在滑动窗口内
        if(inWindow(maxSeqNo, windowSize, windowBegin, currentSeqNo)){
            for(i = 0; i < windowSize; i++){
                if(FrameGetSeq(frames_send[i]) == currentSeqNo){
                    frameNo = i;
                    FrameSet(frames_send[i], 1, msg);
                    break;
                }
            }
            if(frameNo == -1){
                fprintf(stderr, "frame not found in window\n");
                fprintf(stderr, "Seq Recved: %s", seqNo_send);
                exit(1);
            }

            printf("Message received - frameNo: %d seqNo: %d\n", frameNo, currentSeqNo);
            printf("type: %s seqNo: %d msg: %s\n", type_send, currentSeqNo, msg);

            if(frameNo - framecount <= 0){
                if(frameNo < framecount || !block){
                    //send ack

                    char *content_tmp = NULL;
                    if(++packet_count<packetNo)
                        content_tmp = packets[packet_count];
                    else
                        content_tmp = "";
                    sprintf(seqNo_recv,"%d",packet_count%maxSeqNo);
                    encode_improve("a", seqNo_send,"d",seqNo_recv, content_tmp, replyMsg);
                    statsend++;
                    statack++;
                    if(sendto(servSock, replyMsg, strlen(replyMsg), 0, (struct sockaddr *)&clntaddr, sizeof(clntaddr)) < 0){
                        fprintf(stderr, "server send ack failed: %s\n", strerror(errno));
                        exit(1);
                    }
                    //else
                    printf("Ack[%s] sent to client: %d, with server package %s\n", seqNo_send, ntohs(clntaddr.sin_port), content_tmp);
                    if(frameNo == framecount){
                        printf("TO NETWORK LAYER: %s\n", frames_send[framecount]->data);
                        framecount++;
                        lastSeqNo = currentSeqNo;
                    }
                }
                else if(block && frameNo == framecount){
                    printf("TO NETWORK LAYER: %s\n", frames_send[framecount]->data);
                    lastSeqNo = frames_send[framecount]->seqNo;
                    framecount++;
                    while(framecount < windowSize && frames_send[framecount]->inUse){
                        printf("TO NETWORK LAYER: %s\n", frames_send[framecount]->data);
                        lastSeqNo = frames_send[framecount]->seqNo;
                        framecount++;
                    }
                    if(framecount == windowSize){
                        //send ack
                        statsend++;
                        statack++;
                        sprintf(seqNo_send, "%d", lastSeqNo);
                        char *content_tmp = NULL;
                        if(++packet_count<packetNo)
                            content_tmp = packets[packet_count];
                        else
                            content_tmp = "";
                        sprintf(seqNo_recv,"%d",packet_count%maxSeqNo);
                        encode_improve("a", seqNo_send,"d",seqNo_recv, content_tmp, replyMsg);
                        if(sendto(servSock, replyMsg, strlen(replyMsg), 0, (struct sockaddr *)&clntaddr, sizeof(clntaddr)) < 0){
                            fprintf(stderr, "server send ack failed: %s\n", strerror(errno));
                            exit(1);
                        }
                        //else
                        printf("Ack[%s] sent to client: %d, with server package %s\n", seqNo_send, ntohs(clntaddr.sin_port),content_tmp);
                        block = 0;
                    }
                    else{
                        //send nak
                        statsend++;
                        statnak++;
                        if(lastSeqNo < maxSeqNo)
                            sprintf(missingSeq, "%d", lastSeqNo+1);
                        else
                            sprintf(missingSeq, "0");
                        char *content_tmp = NULL;
                        if(++packet_count<packetNo)
                            content_tmp = packets[packet_count];
                        else
                            content_tmp = "";
                        sprintf(seqNo_recv,"%d",packet_count%maxSeqNo);
                        encode_improve("n", missingSeq,"d",seqNo_recv, content_tmp, replyMsg);
                        if(sendto(servSock, replyMsg, strlen(replyMsg), 0, (struct sockaddr *)&clntaddr, sizeof(clntaddr)) < 0){
                            fprintf(stderr, "server send ack failed: %s\n", strerror(errno));
                            exit(1);
                        }
                        //else
                        printf("Nck[%s] sent to client: %d, with server package %s\n",missingSeq, ntohs(clntaddr.sin_port),content_tmp);
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
                    char *content_tmp = NULL;
                    if(++packet_count<packetNo)
                        content_tmp = packets[packet_count];
                    else
                        content_tmp = "";
                    sprintf(seqNo_recv,"%d",packet_count%maxSeqNo);
                    encode_improve("n", missingSeq,"d",seqNo_recv, content_tmp, replyMsg);
                    if(sendto(servSock, replyMsg, strlen(replyMsg), 0, (struct sockaddr *)&clntaddr, sizeof(clntaddr)) < 0){
                        fprintf(stderr, "server send ack failed: %s\n", strerror(errno));
                        exit(1);
                    }
                    //else
                    printf("Nck[%s] sent to client: %d, with server package %s\n", missingSeq, ntohs(clntaddr.sin_port),content_tmp);
                }
            }
        }
        else{
            //重新确认已经收到
            sprintf(seqNo_recv,"%d",packet_count%maxSeqNo);
            encode_improve("a", seqNo_send,"z","", "", replyMsg);
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
        free(frames_send[i]->data);
        free(frames_send[i]);
    }
    free(frames_send);
    free(recvMsg);
    free(type_send);
    free(seqNo_send);
    free(seqNo_recv);
    free(msg);
    free(replyMsg);
    close(servSock);
    exit(0);
}