#include "RaidRoomManager.h"

bool RaidRoomManager::init() {
	WSADATA wsaData;
    int check = 0;

	check = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (check) {
        std::cout << "WSAStartup ����" << std::endl;
        return false;
    }

    rgsSkt = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
    if (rgsSkt == INVALID_SOCKET) {
        std::cout << "Server Socket ���� ����" << std::endl;
        return false;
    }

    SOCKADDR_IN addr;
    addr.sin_port = htons(PORT);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    check = bind(rgsSkt, (SOCKADDR*)&addr, sizeof(addr));
    if (check) {
        std::cout << "bind �Լ� ����:" << WSAGetLastError() << std::endl;
        return false;
    }

    check = listen(rgsSkt, SOMAXCONN);
    if (check) {
        std::cout << "listen �Լ� ����" << std::endl;
        return false;
    }

    rgsIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
    if (rgsIOCPHandle == NULL) {
        std::cout << "iocp �ڵ� ���� ����" << std::endl;
        return false;
    }

    auto bIOCPHandle = CreateIoCompletionPort((HANDLE)rgsSkt, rgsIOCPHandle, (uint32_t)0, 0);
    if (bIOCPHandle == nullptr) {
        std::cout << "iocp �ڵ� ���ε� ����" << std::endl;
        return false;
    }

    udpSkt = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (udpSkt == INVALID_SOCKET) {
        std::cout << "Server Socket ���� ����" << std::endl;
        return false;
    }

	if (!CreateTimeCheckThread()) {
		return false;
	}

	if (!CreateTickRateThread()) {
		return false;
	}

	return true;
}

bool RaidRoomManager::CreateTimeCheckThread() {
    timeChekcRun = true;
    timeCheckThread = std::thread([this]() {TimeCheckThread(); });
    std::cout << "TimeCheckThread Start" << std::endl;
    return true;
}

bool RaidRoomManager::CreateTickRateThread() {

}

void RaidRoomManager::TickRateThread() {

}

void RaidRoomManager::DeleteMob(Room* room_) { // �� ���� �׿�����

    if (room_->TimeOverCheck()) { // Ÿ�Ӿƿ� üũ�Ǽ� �̹� endRoomCheckSet���� ���� ������ (�� ������ ó��)
        roomManager->DeleteRoom(room_->GetRoomNum());
        roomNumQueue.push(room_->GetRoomNum());
        return;
    }

    {    // Ÿ�Ӿƿ����� ���� ��Ƽ� endRoomCheckSet ���� ����
        std::lock_guard<std::mutex> guard(mDeleteRoom);
        for (auto iter = endRoomCheckSet.begin(); iter != endRoomCheckSet.end(); iter++) {
            if (*iter == room_) {
                delete* iter;
                endRoomCheckSet.erase(iter);
                break;
            }
        }
    }

    roomManager->DeleteRoom(room_->GetRoomNum());
    roomNumQueue.push(room_->GetRoomNum());
}

void RaidRoomManager::TimeCheckThread() {
    std::chrono::steady_clock::time_point now;
    Room* room_;
    while (timeChekcRun) {
        if (!endRoomCheckSet.empty()) { // Room Exist
            room_ = (*endRoomCheckSet.begin());
            if (room_->GetEndTime() <= std::chrono::steady_clock::now()) {
                std::cout << "Ÿ�� �ƿ�. ���̵� ����" << std::endl;
                room_->TimeOver();
                endRoomCheckSet.erase(endRoomCheckSet.begin());
            }
            else {
                std::cout << "������" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
        else { // Room Not Exist
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
}
