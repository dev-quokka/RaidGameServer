#pragma once

#include <set>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <boost/lockfree/queue.hpp>
#include <tbb/concurrent_hash_map.h>

#include "Room.h"

constexpr uint16_t TICK_RATE = 5; // 1�ʿ� ����� ��������
constexpr int PORT = 9501;
constexpr int UDP_PORT = 50000;
constexpr uint16_t MAX_ROOM = 10;

struct EndTimeComp {
	bool operator()(Room* r1, Room* r2) const {
		return r1->GetEndTime() > r2->GetEndTime();
	}
};

class RaidRoomManager {
public:
	~RaidRoomManager() {
		timeChekcRun = false;
		if (timeCheckThread.joinable()) {
			timeCheckThread.join();
		}

		tickRateRun = false;
		if (tickRateThread.joinable()) {
			tickRateThread.join();
		}
	}

	bool init();
	bool CreateTimeCheckThread();
	bool CreateTickRateThread();
	void TimeCheckThread();
	void TickRateThread();
	void DeleteMob(Room* room_);

	// Tick Rate Test 1 (vector) ���� �������� 2������ ���� ���������� ����
	//bool CreateTickRateThread1();
	//void TickRateThread1();

	// Tick Rate Test 2 (lockfree_queue) ���������� ���� ���� ���� (�������� pop, push)
	//bool CreateTickRateThread2();
	//void TickRateThread2();

private:
	// 80 bytes
	std::mutex mDeleteRoom;
	std::mutex mDeleteMatch;

	// 24 bytes
	std::set<Room*, EndTimeComp> endRoomCheckSet;

	// 16 bytes
	std::thread timeCheckThread;
	std::thread tickRateThread;

	// 8 bytes
	SOCKET rgsSkt;
	SOCKET udpSkt;
	HANDLE rgsIOCPHandle;

	// 1 bytes
	std::atomic<bool> timeChekcRun;
	std::atomic<bool> tickRateRun;
};