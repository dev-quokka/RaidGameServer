#include "RoomManager.h"

bool RoomManager::init() {
    udpSkt = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (udpSkt == INVALID_SOCKET) {
        std::cout << "Server Socket ���� ����" << std::endl;
        return false;
    }

    sockaddr_in serverUdpAddr{};
    serverUdpAddr.sin_family = AF_INET;
    serverUdpAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverUdpAddr.sin_port = htons(UDP_PORT);

    bind(udpSkt, (sockaddr*)&serverUdpAddr, sizeof(serverUdpAddr));

	if (!CreateTimeCheckThread()) {
		return false;
	}

	if (!CreateTickRateThread()) {
		return false;
	}

	return true;
}

bool RoomManager::CreateTimeCheckThread() {
    timeChekcRun = true;
    timeCheckThread = std::thread([this]() {TimeCheckThread(); });
    std::cout << "TimeCheckThread Start" << std::endl;
    return true;
}

bool RoomManager::CreateTickRateThread() {
    tickRateRun = true;
    tickRateThread = std::thread([this]() {TickRateThread(); });
    std::cout << "TickRateThread Start" << std::endl;
    return true;
}

void RoomManager::TickRateThread() {
    while (tickRateRun) {
        auto tickRate = std::chrono::milliseconds(1000 / TICK_RATE);
        auto timeCheck = std::chrono::steady_clock::now() + tickRate;

        for (auto iter = roomMap.begin(); iter != roomMap.end(); iter++) { // ���� ���� �濡 ƽ ����Ʈ ����ȭ �޽��� ����
            iter->second->SendSyncMsg();
        }

        while (timeCheck > std::chrono::steady_clock::now()) { // ƽ����Ʈ ���� ���
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

bool RoomManager::MakeRoom(uint16_t roomNum_, uint16_t mapNum_, uint16_t timer_, int mobHp_, RaidUserInfo* raidUserInfo1, RaidUserInfo* raidUserInfo2) {
    Room* room = new Room(&udpSkt);

    if (!room->Set(roomNum_, mapNum_, timer_, mobHp_, raidUserInfo1, raidUserInfo2)) {
        std::cout << "Making Room Fail in RoomManager" << std::endl;
        return false;
    }

    roomMap[roomNum_] = room;
    return true;
}

Room* RoomManager::GetRoom(uint16_t roomNum_) {
    return roomMap[roomNum_];
}

void RoomManager::DeleteRoom(uint16_t roomNum_) {
    Room* room = roomMap[roomNum_];
    delete room;
    roomMap.erase(roomNum_);
}

void RoomManager::DeleteMob(Room* room_) { // �� ���� �׿�����
    if (room_->TimeOverCheck()) { // Ÿ�Ӿƿ� üũ�Ǽ� �̹� endRoomCheckSet���� ���� ������ (�� ������ ó��)
        DeleteRoom(room_->GetRoomNum());
        return;
    }

    { // Ÿ�Ӿƿ����� ���� ��Ƽ� endRoomCheckSet ���� ����
        std::lock_guard<std::mutex> guard(mDeleteRoom);
        for (auto iter = endRoomCheckSet.begin(); iter != endRoomCheckSet.end(); iter++) {
            if (*iter == room_) {
                delete* iter;
                endRoomCheckSet.erase(iter);
                break;
            }
        }
    }

    DeleteRoom(room_->GetRoomNum());
}

void RoomManager::TimeCheckThread() {
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
