//
// Created by wangnaizheng on 2018/5/5.
//

#ifndef UDP_SERVER_STANDARD_SR_H
#define UDP_SERVER_STANDARD_SR_H
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>



int client_main(int clntport, int servport, int maxSeqNo, int packetsNo, int timeout, int droprate);


int server_main(unsigned short port, unsigned short clntport, int maxSeqNo);


#endif //UDP_SERVER_STANDARD_SR_H
