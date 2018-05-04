//
// Created by wangnaizheng on 2018/5/2.
//

#include"common.h"


char SEPARATOR = 30;

int FrameInit(FRAME **frame, int size, int maxSeqNo, int beginSeq){
    int i;
    for(i = 0; i < size; i++){
        frame[i]->inUse = 0;
        frame[i]->ack = 0;
        frame[i]->seqNo = (beginSeq + i) % (maxSeqNo + 1);
        frame[i]->data[0] = '\0';
    }
    return 0;
}

int FrameSet(FRAME *frame, int ack, char *data){
    frame->inUse = 1;
    //frame->seqNo = seqNo;
    frame->ack = ack;
    gettimeofday(&frame->setTime, NULL);
    strcpy(frame->data, data);
    return 0;
}

int FrameGetData(FRAME *frame, char *ret){
    ret = frame->data;
    return 0;
}

int FrameGetSeq(FRAME *frame){
    return frame->seqNo;
}

int FrameAck(FRAME *frame){
    frame->inUse = 1;
    frame->ack = 1;
    return 0;
}

int nextFrame(FRAME **frames, int windowSize, FRAME **ret){
    int i;
    for(i = 0; i < windowSize; i++){
        if(!frames[i]->inUse){
            *ret = frames[i];
            return 1;
        }
    }
    return 0;
}

int FrameResetTimeout(FRAME *frame){
    gettimeofday(&frame->setTime, NULL);
    return 0;
}

int isFrameTimeOut(FRAME *frame, long timeout){
    struct timeval currentTime;
    long currentTimeInms, frameTimeInms;
    gettimeofday(&currentTime, NULL);
    currentTimeInms = (currentTime.tv_sec) * 1000 + (currentTime.tv_usec) /1000;
    frameTimeInms = (frame->setTime.tv_sec) * 1000 + (frame->setTime.tv_usec) /1000;
    return (currentTimeInms - frameTimeInms) > timeout;
}

int isFrameUsedAck(FRAME **frame, int size){
    int i;
    for(i = 0; i < size; i++){
        if(frame[i]->inUse && !frame[i]->ack)
            return 0;
    }
    return 1;
}

int isFrameAllAck(FRAME **frame, int size){
    int i;
    for(i = 0; i < size; i++){
        if(!frame[i]->ack)
            return 0;
    }
    return 1;
}

int encode(char *type, char *seqNo, char *message, char *ret){
    ret[0] = '\0';
    if(type != NULL)
        strcat(ret, type);
    else
        return -1;
    strncat(ret, &SEPARATOR, 1);
    if(seqNo != NULL)
        strcat(ret, seqNo);
    else
        return -1;
    strncat(ret, &SEPARATOR, 1);
    if(message != NULL)
        strcat(ret, message);
    strncat(ret, &SEPARATOR, 1);
    return 0;
}

int decode(char *type, char *seqNo, char *message, char *input){
    char *token;
    type[0] = '\0';
    seqNo[0] = '\0';
    message[0] = '\0';

    token = strsep(&input, &SEPARATOR);
    strcat(type, token);
    token = strsep(&input, &SEPARATOR);
    strcat(seqNo, token);
    token = strsep(&input, &SEPARATOR);
    strcat(message, token);

    return 0;
}

int recvUDP(int sock, char *msg, struct sockaddr_in clntaddr){
    socklen_t addrlen = sizeof(clntaddr);
    int recvlen;
    unsigned char buf[100];

    //printf("Waiting on port\n");
    recvlen = recvfrom(sock, buf, 100, 0, (struct sockaddr *)&clntaddr, &addrlen);
    if (recvlen > 0) {
        buf[recvlen] = '\0';
        //printf("received message: %s\n", buf);
        strcat(msg, buf);
    }
    else{
        //printf("recvfrom len is 0\n");
        return 0;
    }
    return ntohs(clntaddr.sin_port);
}

int inWindow(int maxSeqNo, int windowSize, int windowBegin, int seqNo){
    int exceedSeq = windowSize + windowBegin - maxSeqNo;
    if(seqNo >= windowBegin && seqNo < windowBegin + windowSize)
        return 1;
    if(exceedSeq > 0 && seqNo < exceedSeq)
        return 1;
    return 0;
}
