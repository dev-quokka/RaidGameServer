#include "GameServer1.h"

// ========================== INITIALIZATION ===========================

bool GameServer1::init(const uint16_t MaxThreadCnt_, int port_) {
    WSADATA wsadata;
    MaxThreadCnt = MaxThreadCnt_; // Set the number of worker threads

    if (WSAStartup(MAKEWORD(2, 2), &wsadata)) {
        std::cout << "Failed to WSAStartup" << std::endl;
        return false;
    }

    serverSkt = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
    if (serverSkt == INVALID_SOCKET) {
        std::cout << "Failed to Create Server Socket" << std::endl;
        return false;
    }

    SOCKADDR_IN addr;
    addr.sin_port = htons(port_);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(serverSkt, (SOCKADDR*)&addr, sizeof(addr))) {
        std::cout << "Failed to Bind :" << WSAGetLastError() << std::endl;
        return false;
    }

    if (listen(serverSkt, SOMAXCONN)) {
        std::cout << "Failed to listen" << std::endl;
        return false;
    }

    sIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MaxThreadCnt);
    if (sIOCPHandle == NULL) {
        std::cout << "Failed to Create IOCP Handle" << std::endl;
        return false;
    }

    auto bIOCPHandle = CreateIoCompletionPort((HANDLE)serverSkt, sIOCPHandle, (uint32_t)0, 0);
    if (bIOCPHandle == nullptr) {
        std::cout << "Failed to Bind IOCP Handle" << std::endl;
        return false;
    }

    overLappedManager = new OverLappedManager;
    overLappedManager->init();

    return true;
}

bool GameServer1::StartWork() {
    if (!CreateWorkThread()) {
        return false;
    }

    if (!CreateAccepterThread()) {
        return false;
    }

    connUsersManager = new ConnUsersManager(MAX_USERS_OBJECT);
    roomManager = new RoomManager;
    redisManager = new RedisManager;

    // 0 : Center Server
    ConnUser* centerConnUser = new ConnUser(MAX_CIRCLE_SIZE, 0, sIOCPHandle, overLappedManager);
    connUsersManager->InsertUser(0, centerConnUser);

    // 1 : Matching Server
    ConnUser* matchingConnUser = new ConnUser(MAX_CIRCLE_SIZE, 1, sIOCPHandle, overLappedManager);
    connUsersManager->InsertUser(1, matchingConnUser);

    for (int i = 2; i < MAX_USERS_OBJECT; i++) { // Make ConnUsers Queue
        ConnUser* connUser = new ConnUser(MAX_CIRCLE_SIZE, i, sIOCPHandle, overLappedManager);

        AcceptQueue.push(connUser); // Push ConnUser
        connUsersManager->InsertUser(i, connUser); // Init ConnUsers
    }

    roomManager->init();
    redisManager->init(MaxThreadCnt);
    redisManager->SetManager(connUsersManager, roomManager);

    MatchingServerConnect();
    CenterServerConnect();

    return true;
}

void GameServer1::ServerEnd() {
    WorkRun = false;
    AccepterRun = false;

    for (int i = 0; i < workThreads.size(); i++) {
        PostQueuedCompletionStatus(sIOCPHandle, 0, 0, nullptr);
    }

    for (int i = 0; i < workThreads.size(); i++) { // Shutdown worker threads
        if (workThreads[i].joinable()) {
            workThreads[i].join();
        }
    }
    for (int i = 0; i < acceptThreads.size(); i++) { // Shutdown accept threads
        if (acceptThreads[i].joinable()) {
            acceptThreads[i].join();
        }
    }

    delete redisManager;
    delete connUsersManager;
    delete roomManager;

    CloseHandle(sIOCPHandle);
    closesocket(serverSkt);
    WSACleanup();

    std::cout << "Wait 5 Seconds Before Shutdown" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait 5 seconds before server shutdown
    std::cout << "Game Server1 Shutdown" << std::endl;
}


// ======================== SERVER CONNECTION ==========================

bool GameServer1::CenterServerConnect() {
    auto centerObj = connUsersManager->FindUser(0);

    SOCKADDR_IN addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ServerAddressMap[ServerType::CenterServer].port);
    inet_pton(AF_INET, ServerAddressMap[ServerType::CenterServer].ip.c_str(), &addr.sin_addr.s_addr);

    std::cout << "Connecting to Center Server" << std::endl;

    if (connect(centerObj->GetSocket(), (SOCKADDR*)&addr, sizeof(addr))) {
        std::cout << "Failed to Connect to Center Server" << std::endl;
        return false;
    }

    std::cout << "Center Server Connected" << std::endl;

    centerObj->ConnUserRecv();

    RAID_SERVER_CONNECT_REQUEST imReq;
    imReq.PacketId = (UINT16)PACKET_ID::RAID_SERVER_CONNECT_REQUEST;
    imReq.PacketLength = sizeof(RAID_SERVER_CONNECT_REQUEST);
    imReq.gameServerNum = GAME_SERVER_NUM;

    centerObj->PushSendMsg(sizeof(RAID_SERVER_CONNECT_REQUEST), (char*)&imReq);

    return true;
}

bool GameServer1::MatchingServerConnect() {
    auto matchingObj = connUsersManager->FindUser(1);

    SOCKADDR_IN addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ServerAddressMap[ServerType::MatchingServer].port);
    inet_pton(AF_INET, ServerAddressMap[ServerType::MatchingServer].ip.c_str(), &addr.sin_addr.s_addr);

    std::cout << "Connecting to Matching Server" << std::endl;

    if (connect(matchingObj->GetSocket(), (SOCKADDR*)&addr, sizeof(addr))) {
        std::cout << "Failed to Connect to Matching Server" << std::endl;
        return false;
    }

    std::cout << "Matching Server Connected" << std::endl;

    matchingObj->ConnUserRecv();

    MATCHING_SERVER_CONNECT_REQUEST_FROM_RAID_SERVER imReq;
    imReq.PacketId = (UINT16)PACKET_ID::MATCHING_SERVER_CONNECT_REQUEST_FROM_RAID_SERVER;
    imReq.PacketLength = sizeof(MATCHING_SERVER_CONNECT_REQUEST_FROM_RAID_SERVER);
    imReq.gameServerNum = GAME_SERVER_NUM;

    matchingObj->PushSendMsg(sizeof(MATCHING_SERVER_CONNECT_REQUEST_FROM_RAID_SERVER), (char*)&imReq);

    return true;
}


// ========================= THREAD MANAGEMENT =========================

bool GameServer1::CreateWorkThread() {
    WorkRun = true;

    try {
        auto threadCnt = MaxThreadCnt;
        for (int i = 0; i < threadCnt; i++) {
            workThreads.emplace_back([this]() { WorkThread(); });
        }
    }
    catch (const std::system_error& e) {
        std::cerr << "Failed to Create Work Threads : " << e.what() << std::endl;
        return false;
    }

    return true;
}

bool GameServer1::CreateAccepterThread() {
    AccepterRun = true;

    try {
        auto threadCnt = MaxThreadCnt / 4 + 1;
        for (int i = 0; i < threadCnt; i++) {
            workThreads.emplace_back([this]() { AccepterThread(); });
        }
    }
    catch (const std::system_error& e) {
        std::cerr << "Failed to Create Accepter Threads : " << e.what() << std::endl;
        return false;
    }

    return true;
}

void GameServer1::WorkThread() {
    LPOVERLAPPED lpOverlapped = NULL;
    ConnUser* connUser = nullptr;
    DWORD dwIoSize = 0;
    bool gqSucces = TRUE;

    while (WorkRun) {
        gqSucces = GetQueuedCompletionStatus(
            sIOCPHandle,
            &dwIoSize,
            (PULONG_PTR)&connUser,
            &lpOverlapped,
            INFINITE
        );

        if (gqSucces && dwIoSize == 0 && lpOverlapped == NULL) {
            WorkRun = false;
            continue;
        }

        auto overlappedEx = (OverlappedEx*)lpOverlapped;
        uint16_t connObjNum = overlappedEx->connObjNum;
        connUser = connUsersManager->FindUser(connObjNum);

        if (!gqSucces || (dwIoSize == 0 && overlappedEx->taskType != TaskType::ACCEPT)) { // User Disconnect
            std::cout << "socket " << connUser->GetSocket() << " Disconnected" << std::endl;

            if (connObjNum == 0) { // Auto shutdown if the center server is disconnected
                std::cout << "Center Server Disconnected" << std::endl;
            }

            redisManager->Disconnect(connObjNum);
            connUser->Reset(); // Reset 
            AcceptQueue.push(connUser);
            continue;
        }

        if (overlappedEx->taskType == TaskType::ACCEPT) { // User Connect
            if (connUser->ConnUserRecv()) {
                std::cout << "socket " << connUser->GetSocket() << " Connection Request" << std::endl;
            }
            else { // Bind Fail
                connUser->Reset(); // Reset ConnUser
                AcceptQueue.push(connUser);
                std::cout << "socket " << connUser->GetSocket() << " Connection Failed" << std::endl;
            }
        }
        else if (overlappedEx->taskType == TaskType::RECV) {
            redisManager->PushRedisPacket(connObjNum, dwIoSize, overlappedEx->wsaBuf.buf); // Proccess In Redismanager
            connUser->ConnUserRecv(); // Wsarecv Again
            overLappedManager->returnOvLap(overlappedEx);
        }
        else if (overlappedEx->taskType == TaskType::SEND) {
            overLappedManager->returnOvLap(overlappedEx);
            connUser->SendComplete();
        }
        else if (overlappedEx->taskType == TaskType::NEWRECV) {
            redisManager->PushRedisPacket(connObjNum, dwIoSize, overlappedEx->wsaBuf.buf); // Proccess In Redismanager
            connUser->ConnUserRecv(); // Wsarecv Again
            delete[] overlappedEx->wsaBuf.buf;
            delete overlappedEx;
        }
        else if (overlappedEx->taskType == TaskType::NEWSEND) {
            delete[] overlappedEx->wsaBuf.buf;
            delete overlappedEx;
            connUser->SendComplete();
        }
    }
}

void GameServer1::AccepterThread() {
    ConnUser* connUser;

    while (AccepterRun) {
        if (AcceptQueue.pop(connUser)) { // AcceptQueue not empty
            if (!connUser->PostAccept(serverSkt)) {
                AcceptQueue.push(connUser);
            }
        }
        else { // AcceptQueue empty
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            //while (AccepterRun) {
            //    if (WaittingQueue.pop(connUser)) { // WaittingQueue not empty
            //        WaittingQueue.push(connUser);
            //    }
            //    else { // WaittingQueue empty
            //        std::this_thread::sleep_for(std::chrono::milliseconds(10));
            //        break;
            //    }
            //}
        }
    }
}

