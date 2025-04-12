#pragma once
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

const int MAX_USER_ID_LEN = 32;
const int MAX_SERVER_USERS = 128; // ���� ���� �� ���� ��Ŷ
const int MAX_JWT_TOKEN_LEN = 256;
const int MAX_SCORE_SIZE = 512;

constexpr int GAME_NUM = 1;

struct DataPacket {
	uint32_t dataSize;
	uint16_t connObjNum;
	DataPacket(uint32_t dataSize_, uint16_t connObjNum_) : dataSize(dataSize_), connObjNum(connObjNum_) {}
	DataPacket() = default;
};

struct PacketInfo
{
	uint16_t packetId = 0;
	uint16_t dataSize = 0;
	uint16_t connObjNum = 0;
	char* pData = nullptr;
};

struct PACKET_HEADER
{
	uint16_t PacketLength;
	uint16_t PacketId;
};


//  ---------------------------- RAID  ----------------------------

struct IM_GAME_REQUEST : PACKET_HEADER {
	uint16_t gameServerNum;
};

struct IM_GAME_RESPONSE : PACKET_HEADER {
	bool isSuccess;
};

struct RAID_TEAMINFO_REQUEST : PACKET_HEADER {
	sockaddr_in userAddr; // ������ ���� udp ������ sockaddr_in ����
	uint16_t roomNum;
	uint16_t myNum;
};

struct RAID_TEAMINFO_RESPONSE : PACKET_HEADER {
	uint16_t teamLevel;
	char teamId[MAX_USER_ID_LEN + 1];
};

struct RAID_START : PACKET_HEADER {
	std::chrono::time_point<std::chrono::steady_clock> endTime; // ������ ���� �ð� + 5�� (��� ���� ������ �ð� 5�� ����)
	unsigned int mobHp;
};

struct RAID_HIT_REQUEST : PACKET_HEADER {
	unsigned int damage;
	uint16_t roomNum;
	uint16_t myNum;
};

struct RAID_HIT_RESPONSE : PACKET_HEADER {
	unsigned int yourScore;
	unsigned int currentMobHp;
};

struct RAID_END_REQUEST : PACKET_HEADER { // Server TO User
	unsigned int userScore;
	unsigned int teamScore;
};

struct RAID_END_REQUEST_TO_CENTER_SERVER : PACKET_HEADER {
	uint16_t gameServerNum;
	uint16_t roomNum;
};


//  ---------------------------- MATCHING  ----------------------------

struct MATCHING_REQUEST_TO_GAME_SERVER : PACKET_HEADER {
	uint16_t roomNum;
	uint16_t userObjNum1;
	uint16_t userObjNum2;
};

struct MATCHING_RESPONSE_FROM_GAME_SERVER : PACKET_HEADER {
	uint16_t roomNum;
	bool isSuccess;
};


enum class PACKET_ID : uint16_t {
	//  ---------------------------- GAME(8001~)  ----------------------------
	IM_GAME_REQUEST = 8001,
	IM_GAME_RESPONSE = 8002,

	MATCHING_REQUEST_TO_GAME_SERVER = 8011,
	MATCHING_RESPONSE_FROM_GAME_SERVER = 8012,

	ROOM_INSERT_REQUEST = 8051,
	ROOM_INSERT_RESPONSE = 8052,

	RAID_TEAMINFO_REQUEST = 8055,
	RAID_TEAMINFO_RESPONSE = 8056,
	RAID_START = 8057,
	RAID_HIT_REQUEST = 8058,
	RAID_HIT_RESPONSE = 8059,

	RAID_END_REQUEST = 8101,
	RAID_END_REQUEST_TO_CENTER_SERVER = 8102
};