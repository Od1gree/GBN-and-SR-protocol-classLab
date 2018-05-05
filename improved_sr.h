//
// Created by wangnaizheng on 2018/5/5.
//

#ifndef UDP_SERVER_IMPROVED_SR_H
#define UDP_SERVER_IMPROVED_SR_H
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int combined_main_init(int clntport, int servport, int maxSeqNo, int packetNo, int droprate, int timeout);

int combined_main_pass(int port, int clntport, int maxSeqNo, int packetNo, int droprate, int timeout);

#endif //UDP_SERVER_IMPROVED_SR_H
