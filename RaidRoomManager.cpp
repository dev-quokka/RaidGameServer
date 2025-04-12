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
    tickRateRun = true;
    tickRateThread = std::thread([this]() {TickRateThread(); });
    std::cout << "TickRateThread1 Start" << std::endl;
    return true;
}

void RaidRoomManager::TickRateThread() {
    while (tickRateRun) {
        auto tickRate = std::chrono::milliseconds(1000 / TICK_RATE);
        auto timeCheck = std::chrono::steady_clock::now() + tickRate;

        for (int i = 1; i <= MAX_ROOM; i++) {

        }

        while (timeCheck > std::chrono::steady_clock::now()) { // ƽ����Ʈ ���� ���
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

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




void RedisManager::RaidReqTeamInfo(uint16_t connObjNum_, uint16_t packetSize_, char* pPacket_) {
    auto raidTeamInfoReqPacket = reinterpret_cast<RAID_TEAMINFO_REQUEST*>(pPacket_);

    if (!raidTeamInfoReqPacket->imReady) { // ���̵� �غ� ���� => �� ����
        return;
    }

    Room* tempRoom = roomManager->GetRoom(raidTeamInfoReqPacket->roomNum);
    tempRoom->setSockAddr(raidTeamInfoReqPacket->myNum, raidTeamInfoReqPacket->userAddr); // Set User UDP Socket Info

    InGameUser* teamUser = tempRoom->GetTeamUser(raidTeamInfoReqPacket->myNum);

    RAID_TEAMINFO_RESPONSE raidTeamInfoResPacket;
    raidTeamInfoResPacket.PacketId = (uint16_t)PACKET_ID::RAID_TEAMINFO_RESPONSE;
    raidTeamInfoResPacket.PacketLength = sizeof(RAID_TEAMINFO_RESPONSE);
    raidTeamInfoResPacket.teamLevel = teamUser->GetLevel();
    strncpy_s(raidTeamInfoResPacket.teamId, teamUser->GetId().c_str(), MAX_USER_ID_LEN);

    connUsersManager->FindUser(connObjNum_)->PushSendMsg(sizeof(RAID_TEAMINFO_RESPONSE), (char*)&raidTeamInfoResPacket);

    if (tempRoom->StartCheck()) { // �� ���� �������� ���� ������ �����ϰ� �� �� ���� Ȯ���ϸ� ���� ���� ���� �����ֱ�
        RAID_START_REQUEST raidStartReqPacket1;
        raidStartReqPacket1.PacketId = (uint16_t)PACKET_ID::RAID_START_REQUEST;
        raidStartReqPacket1.PacketLength = sizeof(RAID_START_REQUEST);
        raidStartReqPacket1.endTime = tempRoom->SetEndTime();

        RAID_START_REQUEST raidStartReqPacket2;
        raidStartReqPacket2.PacketId = (uint16_t)PACKET_ID::RAID_START_REQUEST;
        raidStartReqPacket2.PacketLength = sizeof(RAID_START_REQUEST);
        raidStartReqPacket2.endTime = tempRoom->SetEndTime();

        connUsersManager->FindUser(connObjNum_)->PushSendMsg(sizeof(RAID_START_REQUEST), (char*)&raidStartReqPacket1);
        connUsersManager->FindUser(tempRoom->GetTeamObjNum(raidTeamInfoReqPacket->myNum))->PushSendMsg(sizeof(RAID_START_REQUEST), (char*)&raidStartReqPacket2);
    }
}

void RedisManager::RaidHit(uint16_t connObjNum_, uint16_t packetSize_, char* pPacket_) {
    auto raidHitReqPacket = reinterpret_cast<RAID_HIT_REQUEST*>(pPacket_);
    InGameUser* user = inGameUserManager->GetInGameUserByObjNum(connObjNum_);

    RAID_HIT_RESPONSE raidHitResPacket;
    raidHitResPacket.PacketId = (uint16_t)PACKET_ID::RAID_HIT_RESPONSE;
    raidHitResPacket.PacketLength = sizeof(RAID_HIT_RESPONSE);

    auto room = roomManager->GetRoom(raidHitReqPacket->roomNum);
    auto hit = room->Hit(raidHitReqPacket->myNum, raidHitReqPacket->damage);

    if (hit.first <= 0) { // Mob Dead
        if (room->EndCheck()) { // SendEndMsg
            raidHitResPacket.currentMobHp = 0;
            raidHitResPacket.yourScore = hit.second;
            connUsersManager->FindUser(connObjNum_)->PushSendMsg(sizeof(RAID_HIT_RESPONSE), (char*)&raidHitResPacket);

            InGameUser* inGameUser;
            try {
                auto pipe = redis->pipeline("ranking");
                for (int i = 0; i < room->GetRoomUserCnt(); i++) {  // ���̵� ���� �޽���
                    inGameUser = room->GetUser(i);

                    RAID_END_REQUEST raidEndReqPacket;
                    raidEndReqPacket.PacketId = (uint16_t)PACKET_ID::RAID_END_REQUEST;
                    raidEndReqPacket.PacketLength = sizeof(RAID_END_REQUEST);
                    raidEndReqPacket.userScore = room->GetScore(i);
                    raidEndReqPacket.teamScore = room->GetTeamScore(i);
                    connUsersManager->FindUser(room->GetUserObjNum(i))->PushSendMsg(sizeof(RAID_END_REQUEST), (char*)&raidEndReqPacket);

                    if (room->GetScore(i) > room->GetUser(i)->GetScore()) {
                        pipe.zadd("ranking", inGameUser->GetId(), (double)(room->GetScore(i))); // ���� ���𽺿� ����ȭ
                    }

                }
                pipe.exec(); // ������ ��ŷ ����ȭ
                matchingManager->DeleteMob(room); // �� ���� ó��
            }
            catch (const sw::redis::Error& e) {
                std::cerr << "Redis error: " << e.what() << std::endl;
                for (int i = 0; i < room->GetRoomUserCnt(); i++) {
                    std::cout << room->GetUser(i)->GetId() << " ���� ���� : " << room->GetScore(i) << std::endl;
                }
                std::cout << "����ȭ ����" << std::endl;
                matchingManager->DeleteMob(room); // �� ���� ó��
                return;
            }
        }
        else { // if get 0, waitting End message
            raidHitResPacket.currentMobHp = 0;
            raidHitResPacket.yourScore = hit.second;
            connUsersManager->FindUser(connObjNum_)->PushSendMsg(sizeof(RAID_HIT_RESPONSE), (char*)&raidHitResPacket);
        }
    }
    else {
        raidHitResPacket.currentMobHp = hit.first;
        raidHitResPacket.yourScore = hit.second;
        connUsersManager->FindUser(connObjNum_)->PushSendMsg(sizeof(RAID_HIT_RESPONSE), (char*)&raidHitResPacket);
    }

    if (hit.second != 0) { // Score�� 0�� �ƴϸ� �� ������ ����ȭ �޽��� ���� 
        connUsersManager->FindUser(GatewayServerObjNum)->PushSendMsg(sizeof(RAID_HIT_RESPONSE), (char*)&raidHitResPacket);
    }
}
