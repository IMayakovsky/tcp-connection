#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>
#include <numeric>
#include <iomanip>
#include <map>

#include <time.h>

#pragma comment(lib, "ws2_32.lib")
#include "../stdafx/stdafx.h"
#include "../digestpp/digestpp.hpp"
#include "crc.h"
#include <winsock2.h>
#include "ws2tcpip.h"

#define TARGET_IP "127.0.0.1"
#define LOCAL_IP "0.0.0.0"
//I 10.8.0.174
//T 10.8.0.154

#define PACKET_LEN 1024L
#define DATA_LEN 1016L
#define OFFSET_LEN 4L
#define CRC_LEN 4L
#define PACKET_RECEIVED 0x06
#define PACKET_NOT_RECEIVED 0x15
#define TIMEOUT 500             // 1 second
#define WINDOW_SIZE 16 // 

#define SENDER

#define TARGET_PORT 8888
#define LOCAL_PORT 5555
#define LOCAL_ACK_PORT 5556



void InitWinsock();
void clearBuffer(char* buf, int size);
int getFileSize(const char* fileName);
std::string getFileHash(const char* fileName);
std::string parseFileName(const char* fileName);
char* unsignedInt32ToCharBytes(unsigned int offset);
void sendPacket(const char* bytestr);
void sendWindow(const std::map<int, std::vector<char>>& packetMap);
std::map<int, std::vector<char>>  sendWindowAndProcessResponse(std::map<int, std::vector<char>> packetMap);
bool sendHeader(std::string header, std::string content);
bool sendFileContents(const char* fileName, int fileSize);
std::vector<char> generateHeaderPacket(std::vector<char> header);
std::vector<char> generatePacket(unsigned int offset, std::vector<char> data);