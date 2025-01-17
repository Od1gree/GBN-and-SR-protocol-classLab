//
// Created by wangnaizheng on 2018/5/2.
//
#ifndef COMMON_H
#define COMMON_H

#include<errno.h>
#include<time.h>
#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/time.h>
#include<netinet/in.h>
#include<stdlib.h>
#include<string.h>
#include<signal.h>
#include<sys/syscall.h>
#include<unistd.h>
#include<math.h>



typedef struct FRAME{
    int inUse;
    int seqNo;
    int ack;
    struct timeval setTime;
    char *data;
} FRAME;

int FrameInit(FRAME **frame, int size, int maxSeqNo, int beginSeq);

int FrameSet(FRAME *frame, int ack, char *data);

int FrameGetData(FRAME *frame, char *ret);

int FrameGetSeq(FRAME *frame);

int FrameAck(FRAME *frame);

int nextFrame(FRAME **frames, int windowSize, FRAME **ret);

int FrameResetTimeout(FRAME *frame);

int isFrameTimeOut(FRAME *frame, long timeout);

int isFrameUsedAck(FRAME **frame, int size);

int isFrameAllAck(FRAME **frame, int size);

int encode(char *type, char *serialNo, char *message, char *ret);

int decode(char *type, char *serialNo, char *message, char *intput);

int recvUDP(int sock, char *msg, struct sockaddr_in clntaddr);

int inWindow(int maxSeqNo, int windowSize, int windowBegin, int seqNo);

#endif