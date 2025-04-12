#pragma once
#pragma comment(lib, "ws2_32.lib") // ���� ���α׷��ֿ�
#pragma comment(lib, "mswsock.lib") // AcceptEx ���

#include <winsock2.h>
#include <windows.h>
#include <cstdint>
#include <atomic>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <deque>
#include <queue>
#include <boost/lockfree/queue.hpp>
#include <tbb/concurrent_hash_map.h>

#include "Packet.h"
#include "Define.h"
#include "ConnUser.h"

#include "OverLappedManager.h"
#include "ConnUsersManager.h"
#include "RoomManager.h"
#include "PacketManager.h"

constexpr uint16_t MAX_USERS_OBJECT = 30; // 1���� ��� ���� ������ 30���� �����ϰ� �̸� �����Ҵ��� ���� ��ü (���߿� 1���� ��� ���� �þ�� ī��Ʈ ����)
constexpr int MAX_CHANNEL1_USERS_COUNT = 30; // �� ä�� ���� ���� �ο� * ä�� ��

class GameServer1 {
public:
    bool init(const uint16_t MaxThreadCnt_, int port_);
    bool CenterConnect();
    bool StartWork();
    void ServerEnd();

private:
    bool CreateWorkThread();
    bool CreateAccepterThread();

    void WorkThread(); // IOCP Complete Event Thread
    void AccepterThread(); // Accept req Thread

    // 136 bytes 
    boost::lockfree::queue<ConnUser*> AcceptQueue{ MAX_USERS_OBJECT }; // For Aceept User Queue

    // 32 bytes
    std::vector<std::thread> workThreads;
    std::vector<std::thread> acceptThreads;

    // 8 bytes
    SOCKET serverSkt = INVALID_SOCKET;
    HANDLE sIOCPHandle = INVALID_HANDLE_VALUE;

    OverLappedManager* overLappedManager;
    ConnUsersManager* connUsersManager;
    RoomManager* roomManager;
    PacketManager* packetManager;

    // 2 bytes
    uint16_t MaxThreadCnt = 0;

    // 1 bytes
    bool WorkRun = false;
    bool AccepterRun = false;
};