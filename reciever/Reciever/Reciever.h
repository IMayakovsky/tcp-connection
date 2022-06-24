#ifndef _RECIEVER_H_
#define __RECIEVER_H_

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip> 
#include <cstdint>

#define NOMINMAX

#pragma comment(lib, "Ws2_32.lib")
#include "stdafx.h"
#include <winsock2.h>
#include "ws2tcpip.h"

#define TARGET_IP "127.0.0.1"
#define LOCAL_IP "127.0.0.1"
//I 10.8.0.174
//T 10.8.0.154

#define TARGET_PORT 5555
#define TARGET_ACK_PORT 5556
#define LOCAL_PORT 8888
#define LOCAL_ACK_PORT 8889

#define BUFFERS_LEN 1024
#define OFFSET_LEN 4
#define CRC_LEN 4
#define DATA_LEN 1016

#define PACKEGES_AMOUNT 64

#define FALSE 0x15
#define TRUE 0x06

void InitWinsock();
bool SetSocket(SOCKET* socketS, struct sockaddr_in* local, struct sockaddr_in* from, int PORT);

void clearBuffer(char* buf, int size);
bool IsRightHeaderName(char* buf, std::string name);
void SaveReceivedDataToFile(std::string fileName, std::vector<std::vector<char>> recieved_data);

std::string GetHeader(char* buf);

int GetIndexFromOffset(char buf[]);

bool CheckCRC(char* buf);

bool IsHereLittleEndian();

void SendReturnMessage(char status, SOCKET socket);

void SendReturnMessage(char[OFFSET_LEN], SOCKET socket);

std::string GetFileHash(const char* fileName);

#endif _RECIEVER_H_