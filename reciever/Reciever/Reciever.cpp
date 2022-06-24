#include "Reciever.h"
#include "../CRCpp/inc/CRC.h" 
#include "../digestpp/digestpp.hpp"

int main() {

#pragma region SOCKET SETTER

	SOCKET socketS;
	struct sockaddr_in local;
	struct sockaddr_in from;
	
	SOCKET socketAck;
	struct sockaddr_in localAck;
	struct sockaddr_in fromAck;

	InitWinsock();

	if (!SetSocket(&socketS, &local, &from, LOCAL_PORT) 
		|| !SetSocket(&socketAck, &localAck, &fromAck, LOCAL_ACK_PORT))
	{
		return 1;
	}

	int fromlen = sizeof(from);

#pragma endregion //SOCKET SETTER

#pragma region VARIABLES SETTER

	char buffer[BUFFERS_LEN];

	std::string main_hash;
	std::string fileName;
	int file_size = 0;

	std::vector<std::vector<char>> recieved_data;
	std::vector<bool> recieved_data_status;
	int last_data_size;
	int amount_of_success = 0;

	char return_mes[OFFSET_LEN];

	bool isHashRight = false;
	bool wasStop = false;
	
#pragma endregion //VARIABLES SETTER

#pragma region HEADERS RECEIVING

	printf("Waiting for datagram ...\n");

	std::string headers[4] = { "NAME", "SIZE", "HASH", "START" };

	for (int i = 0; i < 4; i++) 
	{
		clearBuffer(buffer, BUFFERS_LEN);

		if (recvfrom(socketS, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromlen) == SOCKET_ERROR) {
			printf("Socket error!\n");
			getchar();
			return 1;
		}

		if (!CheckCRC(buffer)) {
			SendReturnMessage(FALSE, socketAck);
			i--;
			continue;
		}
		else {
			SendReturnMessage(TRUE, socketAck);
		}

		if (!IsRightHeaderName(buffer, headers[i]))
		{
			i--;
			continue;
		}

		std::string header_data = GetHeader(buffer);

		std::cout << headers[i] << ": " << header_data << std::endl;

		switch (i)
		{
		case 0:
			fileName = header_data;
			break;
		case 1:
			try
			{
				file_size = std::stoi(header_data);
			}
			catch (const std::exception&)
			{
				return 1;
			}
			break;
		case 2:
			main_hash = header_data;
			break;
		default:
			break;
		}
	}

#pragma endregion //HEADERS RECEIVING

#pragma region Receiving Data Containers Setter

	for (int i = 0; i < file_size / DATA_LEN; i++)
	{
		recieved_data.push_back(std::vector<char>());
		recieved_data_status.push_back(false);
	}

	last_data_size = DATA_LEN;

	if (file_size % DATA_LEN != 0)
	{
		last_data_size = file_size % DATA_LEN;
		recieved_data.push_back(std::vector<char>());
		recieved_data_status.push_back(false);
	}

#pragma endregion //Receiving Data Containers Setter

#pragma region DATA RECEIVING
	
	do 
	{
		clearBuffer(buffer, BUFFERS_LEN);
		if (recvfrom(socketS, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromlen) == SOCKET_ERROR) {
			printf("Socket error!\n");
			getchar();
			return 1;
		}

		if (CheckCRC(buffer))
		{
			if (amount_of_success == recieved_data_status.size())
			{
				SendReturnMessage(TRUE, socketAck);
				if (wasStop && IsRightHeaderName(buffer, "OK?"))
				{
					if (isHashRight)
					{
						SendReturnMessage(TRUE, socketAck);
						std::cout << "Hash comparison was successful!\n" << "Data was loaded!\n";
						break;
					}
					else
					{
						SendReturnMessage(FALSE, socketAck);
						remove(fileName.data());
						std::cerr << "error: Hash comparison was not successful!\n"
							<< "Try again...\n";
						break;
					}
				}
				else if (wasStop) {
					std::cerr << "warrning: No End Detected!\n";
					if (IsRightHeaderName(buffer, "STOP"))
						SendReturnMessage(TRUE, socketAck);
					else
						SendReturnMessage(FALSE, socketAck);
					continue;
				}

				if (IsRightHeaderName(buffer, "STOP")) {

					std::cout << "STOP was detected.\n";
					wasStop = true;

					SaveReceivedDataToFile(fileName, recieved_data);
					std::string my_hash = GetFileHash(fileName.data());
					std::string other_hash(main_hash.begin(), main_hash.end());

					isHashRight = (my_hash.compare(other_hash) == 0);

					continue;
				}
				else {
					std::cerr << "warrning: No STOP Detected!\n";
				}
			}

			if (amount_of_success == 0 && IsRightHeaderName(buffer, "START"))
			{
				std::cerr << "warning: START has already been!\n";
				SendReturnMessage(TRUE, socketAck);
				continue;
			}

			std::cout << "\nrecieved offset: ";
			int index = GetIndexFromOffset(buffer);
			for (int i = 0; i < OFFSET_LEN; i++)
			{
				return_mes[i] = buffer[i];
			}
			SendReturnMessage(return_mes, socketAck);
			std::cout << "index: " << index << std::endl;

			if (index < 0 || index >= recieved_data.size() || recieved_data_status[index])
			{
				std::cerr << "warning: This data has already loaded!" << std::endl;
				continue;
			}
			else
			{
				int data_len = (index == recieved_data.size() - 1) ? last_data_size : DATA_LEN;
				for (int q = 0; q < data_len; q++)
				{
					recieved_data[index].push_back(buffer[q + OFFSET_LEN]);
				}
				recieved_data_status[index] = true;
				amount_of_success++;
				std::cout << "Packege was successful loaded!\n";
			}


		}
		else
		{
			std::cerr << "warning: wrong CRC!\n";
		}

	} while (true);

#pragma endregion //DATA RECEIVING

	closesocket(socketS);
	return 0;
}

#pragma region Functions

void InitWinsock()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

bool SetSocket(SOCKET *socketS, struct sockaddr_in *local, struct sockaddr_in *from, int PORT)
{
	local->sin_family = AF_INET;
	local->sin_port = htons(PORT);
	InetPton(AF_INET, _T(LOCAL_IP), &local->sin_addr.s_addr);

	*socketS = socket(AF_INET, SOCK_DGRAM, 0);
	if (bind(*socketS, (sockaddr*)local, sizeof(*local)) != 0) {
		printf("Binding error!\n");
		getchar(); //wait for press Enter
		return false;
	}

	return true;


}

void clearBuffer(char* buf, int size)
{
	for (int i = 0; i < size; i++) {
		buf[i] = '\0';
	}
}

void SendReturnMessage(char return_mes[OFFSET_LEN], SOCKET socket) {

	sockaddr_in addrDest;
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_ACK_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);
	// for cout
	std::cout << "return offset: ";
	GetIndexFromOffset(return_mes);
	//
	std::cout << std::endl;
	sendto(socket, return_mes, OFFSET_LEN, 0, (sockaddr*)&addrDest, sizeof(addrDest));
}

void SendReturnMessage(char status, SOCKET socket) {

	sockaddr_in addrDest;
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_ACK_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);

	char return_mes[1];

	return_mes[0] = status;
	sendto(socket, return_mes, 1, 0, (sockaddr*)&addrDest, sizeof(addrDest));
}

bool IsHereLittleEndian() {
	short int word = 0x0001;
	char* b = (char*)&word;
	return b[0];
}

bool CheckCRC(char* buf) {

	char crc_bytes[CRC_LEN];
	char data[DATA_LEN + OFFSET_LEN];
	std::uint32_t recieved_crc;

	clearBuffer(data, DATA_LEN + OFFSET_LEN);

	for (int i = 0; i < DATA_LEN + OFFSET_LEN; i++) {
		data[i] = buf[i];
	}

	for (int i = BUFFERS_LEN - CRC_LEN; i < BUFFERS_LEN; i++) {
		crc_bytes[i - BUFFERS_LEN + CRC_LEN] = buf[i];
	}
	
	std::memcpy(&recieved_crc, crc_bytes, sizeof(int));
	std::uint32_t my_crc = CRC::Calculate(data, DATA_LEN + OFFSET_LEN, CRC::CRC_32());

	return recieved_crc == my_crc;
}

void SaveReceivedDataToFile(std::string fileName, std::vector<std::vector<char>> recieved_data)
{
	std::ofstream fileToSave(fileName, std::ofstream::binary);
	for (int i = 0; i < recieved_data.size(); i++) {
		fileToSave.write(recieved_data[i].data(), recieved_data[i].size());
	}
	fileToSave.close();
}

int GetIndexFromOffset(char buf[])
{
	int offset_value;
	char offset_bytes[OFFSET_LEN];
	for (int i = 0; i < OFFSET_LEN; i++) {
		offset_bytes[i] = buf[i];
	}
	
	std::memcpy(&offset_value, offset_bytes, sizeof(int));
	std::cout << offset_value << std::endl;
	return offset_value / DATA_LEN;
}


std::string GetHeader(char* buf)
{
	std::string path = "";
	int i = 0;
	while (buf[i] != '=' && buf[i++] != '\0');
	while (buf[i++] != NULL) {
		path += buf[i];
	}
	return path;
}

bool IsRightHeaderName(char* buf, std::string name) {
	for (int i = 0; ; i++) {
		if (name[i] == NULL) {
			break;
		}
		else if (buf[i] == NULL || buf[i] != name[i]) {
			return false;
		}
	}
	return true;
}

std::string GetFileHash(const char* fileName) {
	std::ifstream tmpFile(fileName, std::ios::binary);
	std::string tmpHashStr = digestpp::blake2b(512).absorb(tmpFile).hexdigest();
	tmpHashStr += '\0';
	tmpFile.close();
	return tmpHashStr;
}

#pragma endregion //Functions