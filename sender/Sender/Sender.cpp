#include "Sender.h"


// SOCKET GLOBALS, DON'T TOUCH
SOCKET socketS;
SOCKET socketAck;
struct sockaddr_in local;
struct sockaddr_in localAck;
struct sockaddr_in from;
sockaddr_in addrDest;
int fromlen = sizeof(from);


//**********************************************************************
int main()
{
	

	InitWinsock();
	
	bool packetSentCheck;
	int fromlen = sizeof(from);

	local.sin_family = AF_INET;
	local.sin_port = htons(LOCAL_PORT);
	InetPton(AF_INET, _T(LOCAL_IP), &local.sin_addr.s_addr);

	localAck.sin_family = AF_INET;
	localAck.sin_port = htons(LOCAL_ACK_PORT);
	InetPton(AF_INET, _T(LOCAL_IP), &localAck.sin_addr.s_addr);

	socketAck = socket(AF_INET, SOCK_DGRAM, 0);

	socketS = socket(AF_INET, SOCK_DGRAM, 0);

	DWORD timeout = TIMEOUT;
	
	setsockopt(socketAck, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);

	if (bind(socketS, (sockaddr*)&local, sizeof(local)) != 0) {
		printf("data socket binding error!\n");
		int iError = WSAGetLastError();
		printf("Socket error! %d\n", iError);
		return 1;
	}

	if (bind(socketAck, (sockaddr*)&localAck, sizeof(localAck)) != 0) {
		printf("ack socket binding error!\n");
		int iError = WSAGetLastError();
		printf("Socket error! %d\n", iError);
		return 1;
	}

	//**********************************************************************

	char tmp_buffer[DATA_LEN];
	int bufferSize = 0;

	
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);

	const char* pathToFile = "cropped.png";

	
	std::string tmpHashStr = getFileHash(pathToFile);


	// Getting file properties

	int fileSize = getFileSize(pathToFile);

	std::string fileName = parseFileName(pathToFile);

	// Sending initial headers

	clock_t tStart = clock();

	sendHeader("NAME=", fileName);
	sendHeader("SIZE=", std::to_string(fileSize));
	sendHeader("HASH=", tmpHashStr);

	printf("Chunk number=%d\n", fileSize / DATA_LEN);

	// sending file

	int max_oks = 20;
	char fileAck;
	fileAck = PACKET_NOT_RECEIVED;
	while (fileAck != PACKET_RECEIVED) {
		if (fileAck == PACKET_NOT_RECEIVED) sendFileContents(pathToFile, fileSize);
		int ok_count = 0;
		while (fileAck != PACKET_RECEIVED) {
			++ok_count;
			if (ok_count == max_oks) {
				printf("OK? ack timeout. The file was probably delievered with success\n");
				return 1;
			}
			std::string OK = "OK?";
			std::vector<char> okPacket(OK.begin(), OK.end());
			okPacket = generateHeaderPacket(okPacket);
			sendPacket(okPacket.data());
			if (recvfrom(socketAck, &fileAck, 1, 0, (sockaddr*)&from, &fromlen) == SOCKET_ERROR) {
				int iError = WSAGetLastError();
				printf("Socket error! %d\n", iError);
				if (iError != 10060) return 1;  // timeout
				else printf("Timeout has occured on OK? ack receiving\n");
			}
		}

	}
	
	printf("YES!\n");
	printf("Time taken: %.2fs\n", (double)(clock() - tStart) / CLOCKS_PER_SEC);
	closesocket(socketS);
	closesocket(socketAck);

	//**********************************************************************

	//getchar(); //wait for press Enter
	return 0;
}


void InitWinsock()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}


void clearBuffer(char* buf, int size)
{
	for (int i = 0; i < size; i++) {
		buf[i] = '\0';
	}
}


int getFileSize(const char* fileName) {
	std::ifstream file(fileName, std::ios::binary);
	file.seekg(0, std::ios::end);
	int size = file.tellg();
	file.close();
	return size;
}


char* unsignedInt32ToCharBytes(unsigned int a) {
	char bytes[4];
	bytes[0] = a >> 24 & 0xFF;
	bytes[1] = a >> 16 & 0xFF;
	bytes[2] = a >> 8 & 0xFF;
	bytes[3] = a & 0xFF;
	return bytes;
}


std::vector<char> generatePacket(unsigned int offset, std::vector<char> data) {
	char offsetBytes[4];
	char crcBytes[4];
	data.reserve(PACKET_LEN);
	memcpy_s(offsetBytes, CRC_LEN, &offset, sizeof(int));
	data.insert(data.begin(), offsetBytes, offsetBytes + OFFSET_LEN);
	data.resize(OFFSET_LEN + DATA_LEN);
	std::uint32_t crc = CRC::Calculate(data.data(), data.size(), CRC::CRC_32());
	memcpy_s(crcBytes, CRC_LEN, &crc, sizeof(std::uint32_t));
	data.insert(data.end(), crcBytes, crcBytes + CRC_LEN);
	return data;
}


void sendPacketSafely(char* bytestr) {
	char ack;
	ack = PACKET_NOT_RECEIVED;
	while (ack != PACKET_RECEIVED) {
		sendPacket(bytestr);
		if (recvfrom(socketAck, &ack, 1, 0, (sockaddr*)&from, &fromlen) == SOCKET_ERROR) {
			int iError = WSAGetLastError();
			printf("Socket error! %d\n", iError);
			if (iError != 10060) return;
			else {
				printf("Timeout has occured on packet ack recieving\n");
				printf("Packet content:%s\n", bytestr);
			}
		}
	}

}


void sendPacket(const char* bytestr) {
	sendto(socketS, bytestr, PACKET_LEN, 0, (sockaddr*)&addrDest, sizeof(addrDest));
}


void sendWindow(const std::map<int, std::vector<char>>& packetMap) {

	for (const auto& p : packetMap) {
		sendPacket(p.second.data());
	}
}


std::map<int, std::vector<char>>  sendWindowAndProcessResponse(std::map<int, std::vector<char>> packetMap) {
	std::cout << "Window sent: " << std::endl;
	for (const auto& p : packetMap) {
		std::cout << p.first << "-> data" << std::endl;
	}


	char buffer[OFFSET_LEN];
	clearBuffer(buffer, OFFSET_LEN);
	sendWindow(packetMap);
	int count = 0;
	while (count != WINDOW_SIZE) {
		if (recvfrom(socketAck, buffer, OFFSET_LEN, 0, (sockaddr*)&from, &fromlen) == SOCKET_ERROR) {
			int iError = WSAGetLastError();
			printf("Socket error! %d\n", iError);
			break;
		}
		count++;
		int offset;
		memcpy_s(&offset, sizeof(offset), buffer, sizeof(buffer));
		if (offset % DATA_LEN == 0)
		    packetMap.erase(offset);
	}


	std::cout << "Window after processing: " << std::endl;
	for (const auto& p : packetMap) {
		std::cout << p.first << "-> data" << std::endl;
	}


	return packetMap;
}


bool sendFileContents(const char* fileName, int fileSize) {

	std::map<int, std::vector<char>> packetMap;

	std::string startHeaderStr = "START";
	std::vector<char> startHeader(startHeaderStr.begin(), startHeaderStr.end());
	startHeader = generateHeaderPacket(startHeader);
	sendPacketSafely(startHeader.data());
	std::cout << "Sent: START" << std::endl;

	std::ifstream file(fileName, std::ios::binary);
	int readDataSize;
	char tmp_buffer[DATA_LEN];
	do {
		clearBuffer(tmp_buffer, DATA_LEN);
		unsigned int startOffset = file.tellg();
		readDataSize = fileSize - startOffset < DATA_LEN ? fileSize - startOffset : DATA_LEN;
		file.read(tmp_buffer, readDataSize);
		std::vector<char> readData(tmp_buffer, tmp_buffer + readDataSize);

		readData = generatePacket(startOffset, readData);

		packetMap.insert(std::pair<int, std::vector<char>>(startOffset, readData));

		while (packetMap.size() == WINDOW_SIZE || (readDataSize < DATA_LEN && packetMap.size() != 0))
			packetMap = sendWindowAndProcessResponse(packetMap);


	} while (readDataSize == DATA_LEN);
	file.close();

	std::string stopHeaderStr = "STOP";
	std::vector<char> stopHeader(stopHeaderStr.begin(), stopHeaderStr.end());
	stopHeader = generateHeaderPacket(stopHeader);
	sendPacketSafely(stopHeader.data());
	std::cout << "Sent: STOP" << std::endl;

	return true;
}


std::vector<char> generateHeaderPacket(std::vector<char> header) {
	char crcBytes[CRC_LEN];
	header.resize(DATA_LEN + OFFSET_LEN);
	std::uint32_t crc = CRC::Calculate(header.data(), header.size(), CRC::CRC_32());
	
	memcpy_s(crcBytes, CRC_LEN, &crc, sizeof(std::uint32_t));
	header.insert(header.end(), crcBytes, crcBytes + CRC_LEN);
	
	return header;
}


std::string parseFileName(const char* pathToFile) {
	int len = strlen(pathToFile);
	int i = 0;
	std::vector<char> fileName;
	for (i = len - 1; i > -1; --i) {
		if (pathToFile[i] == '/' || pathToFile[i] == '\\') { // Unix + Windows
			break;
		}
		fileName.emplace(fileName.begin(), pathToFile[i]);
	}
	fileName.push_back('\0');
	std::string fileNameStr(fileName.data());
	return fileNameStr;
}


std::string getFileHash(const char* fileName) {
	std::ifstream tmpFile(fileName, std::ios::binary);
	std::string tmpHashStr = digestpp::blake2b(512).absorb(tmpFile).hexdigest();
	tmpFile.close();
	return tmpHashStr;
}


bool sendHeader(std::string header, std::string content) {
	std::string headerStr = header;
	headerStr += content;
	std::vector<char> headerVec(headerStr.begin(), headerStr.end());
	headerVec = generateHeaderPacket(headerVec);
	sendPacketSafely(headerVec.data());

	std::cout << "Sent: " << headerVec.data() << std::endl;

	return true;
}