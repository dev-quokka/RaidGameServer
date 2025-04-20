#include "RoomManager.h"

// ====================== INITIALIZATION =======================

bool RoomManager::init() {
    udpSkt = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSkt == INVALID_SOCKET) {
        std::cout << "Failed to Create UDP Socket" << std::endl;
        return false;
    }

    sockaddr_in serverUdpAddr{};
    serverUdpAddr.sin_family = AF_INET;
    serverUdpAddr.sin_addr.s_addr = INADDR_ANY;
    serverUdpAddr.sin_port = htons(UDP_PORT);

    if (bind(udpSkt, (sockaddr*)&serverUdpAddr, sizeof(serverUdpAddr))) {
        std::cout << "Failed to Bind UDP Socket:" << WSAGetLastError() << std::endl;
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

// ===================== THREAD MANAGEMENT =====================

bool RoomManager::CreateTimeCheckThread() {
    timeChekcRun = true;

    try {
        timeCheckThread = std::thread([this]() { TimeCheckThread(); });
    }
    catch (const std::system_error& e) {
        std::cerr << "Failed to Create TimeCheck Thread : " << e.what() << std::endl;
        return false;
    }

    return true;
}

bool RoomManager::CreateTickRateThread() {
    tickRateRun = true;

    try {
        tickRateThread = std::thread([this]() { TickRateThread(); });
    }
    catch (const std::system_error& e) {
        std::cerr << "Failed to Create TickRate Thread : " << e.what() << std::endl;
        return false;
    }

    return true;
}

void RoomManager::TickRateThread() {
    auto tickRate = std::chrono::milliseconds(1000 / TICK_RATE); // Set tick interval

    while (tickRateRun) { 
        auto timeCheck = std::chrono::steady_clock::now() + tickRate; 

        if (roomMap.empty()) { // No active room
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        for (auto iter = roomMap.begin(); iter != roomMap.end(); iter++) { // Send synchronization data
            if (iter->second->IsGameRunning()) {
                iter->second->SendSyncMsg();
            }
        }

        auto currentTime = std::chrono::steady_clock::now();

        while (timeCheck > currentTime) {  // Sleep for the remaining time until the next tick
            std::this_thread::sleep_for(timeCheck - currentTime);
        }

    }
}

void RoomManager::TimeCheckThread() {
    Room* room_;

    while (timeChekcRun) {
        if (!endRoomCheckSet.empty()) { // Active room exists
            room_ = (*endRoomCheckSet.begin());
            if (room_->GetEndTime() <= std::chrono::steady_clock::now()) { // Timeout occurred
                std::cout << "Time out. Raid ended Room : " << room_->GetRoomNum() << std::endl;
                room_->TimeOver();
                endRoomCheckSet.erase(endRoomCheckSet.begin());
            }
            else { // Game in progress
                std::cout << "���� ��" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
        else { // No active room
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
}


// ===================== ROOM MANAGEMENT =======================

void RoomManager::MakeRoom(uint16_t roomNum_) {
    tbb::concurrent_hash_map<uint16_t, Room*>::accessor accessor;

	if (roomMap.insert(accessor, roomNum_)) { // Create a new room if it doesn't exist
        accessor->second = new Room(roomNum_, &udpSkt);
    }

    return;
}

void RoomManager::InsertEndCheckSet(Room* tempRoom_) {
    endRoomCheckSet.insert(tempRoom_); // Insert the room into the end check set
}

Room* RoomManager::GetRoom(uint16_t roomNum_) {
    tbb::concurrent_hash_map<uint16_t, Room*>::accessor accessor;

    if (!roomMap.find(accessor, roomNum_)) { // Return if the room does not exist
        std::cout << "Room not found" << std::endl;
        return nullptr;
    }

    return accessor->second;
}

void RoomManager::DeleteRoom(uint16_t roomNum_) {
    tbb::concurrent_hash_map<uint16_t, Room*>::accessor accessor;

    if (!roomMap.find(accessor, roomNum_)) { // Return if the room does not exist
        std::cout << "Room not found" << std::endl;
        return;
    }

    delete accessor->second;
    roomMap.erase(accessor); // Remove the room from the map
}


// ====================== RAID CLEANUP =========================

void RoomManager::DeleteMob(Room* room_) { // Raid mob defeated
    if (room_->TimeOverCheck()) { // The room was already removed due to a timeout check
        DeleteRoom(room_->GetRoomNum());
        return;
    }

    { // Raid Mob defeated before timeout
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
