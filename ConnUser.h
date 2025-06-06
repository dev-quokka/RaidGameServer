#pragma once
#include <atomic>

#include "Define.h"
#include "CircularBuffer.h"
#include "Packet.h"
#include "overLappedManager.h"

class ConnUser {
public:
	ConnUser(uint32_t bufferSize_, uint16_t connObjNum_, HANDLE sIOCPHandle_, OverLappedManager* overLappedManager_)
		: connObjNum(connObjNum_), sIOCPHandle(sIOCPHandle_), overLappedManager(overLappedManager_) {
		
		userSkt = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (userSkt == INVALID_SOCKET) {
			std::cout << "Client socket Error : " << GetLastError() << std::endl;
		}

		auto tIOCPHandle = CreateIoCompletionPort((HANDLE)userSkt, sIOCPHandle_, (ULONG_PTR)0, 0);
		if (tIOCPHandle == INVALID_HANDLE_VALUE)
		{
			std::cout << "createIoCompletionPort Fail : " << GetLastError() << std::endl;
		}

		circularBuffer = std::make_unique<CircularBuffer>(bufferSize_);
	}
	~ConnUser() {
		shutdown(userSkt, SD_BOTH);
		closesocket(userSkt);
	}

public:
	// ======================= INITIALIZATION =======================

	void Reset() { // Reset connuser object socket
		isConn = false;
		shutdown(userSkt, SD_BOTH);
		closesocket(userSkt);

		userSkt = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (userSkt == INVALID_SOCKET) {
			std::cout << "Client socket Error : " << GetLastError() << std::endl;
		}

		auto tIOCPHandle = CreateIoCompletionPort((HANDLE)userSkt, sIOCPHandle, (ULONG_PTR)0, 0);
		if (tIOCPHandle == INVALID_HANDLE_VALUE)
		{
			std::cout << "createIoCompletionPort Fail : " << GetLastError() << std::endl;
		}

	}


	// ======================= IDENTIFICATION =======================

	bool IsConn() { // Check connection status
		return isConn;
	}

	void SetUserRoomInfo(uint16_t roomNum_, uint16_t userRaidServerObjNum_) {
		roomNum = roomNum_;
		userRaidServerObjNum = userRaidServerObjNum_;
	}

	SOCKET GetSocket() {
		return userSkt;
	}

	uint16_t GetObjNum() {
		return connObjNum;
	}

	uint16_t GetRoomNum() {
		return roomNum;
	}

	uint32_t GetUserRaidServerObjNum() {
		return userRaidServerObjNum;
	}


	// ======================= CIRCULAR BUFFER =======================

	bool WriteRecvData(const char* data_, uint32_t size_) { // Set recvdata in circular buffer 
		return circularBuffer->Write(data_, size_);
	}

	PacketInfo ReadRecvData(char* readData_, uint32_t size_) { // Get recvdata in circular buffer 
		CopyMemory(readData, readData_, size_);

		if (circularBuffer->Read(readData, size_)) {
			auto pHeader = (PACKET_HEADER*)readData;

			PacketInfo packetInfo;
			packetInfo.packetId = pHeader->PacketId;
			packetInfo.dataSize = pHeader->PacketLength;
			packetInfo.connObjNum = connObjNum;
			packetInfo.pData = readData;

			return packetInfo;
		}
	}


	// ========================== ACCEPT ==========================

	bool PostAccept(SOCKET ServerSkt_) {
		acceptOvlap = {};
		acceptOvlap.taskType = TaskType::ACCEPT;
		acceptOvlap.connObjNum = connObjNum;
		acceptOvlap.wsaBuf.buf = nullptr;
		acceptOvlap.wsaBuf.len = 0;

		DWORD bytes = 0;
		DWORD flags = 0;

		if (AcceptEx(ServerSkt_, userSkt, acceptBuf, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes, (LPWSAOVERLAPPED)&acceptOvlap) == 0) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				std::cout << "AcceptEx Error : " << GetLastError() << std::endl;
				return false;
			}
		}

		return true;
	}


	// =========================== RECV ===========================

	bool ConnUserRecv() { 
		OverlappedEx* tempOvLap = (overLappedManager->getOvLap());

		if (tempOvLap == nullptr) { // Allocate new overlap if pool is empty
			OverlappedEx* overlappedEx = new OverlappedEx;
			ZeroMemory(overlappedEx, sizeof(OverlappedEx));
			overlappedEx->wsaBuf.len = MAX_RECV_SIZE;
			overlappedEx->wsaBuf.buf = new char[MAX_RECV_SIZE];
			overlappedEx->connObjNum = connObjNum;
			overlappedEx->taskType = TaskType::NEWSEND;
		}
		else {
			tempOvLap->wsaBuf.len = MAX_RECV_SIZE;
			tempOvLap->wsaBuf.buf = new char[MAX_RECV_SIZE];
			tempOvLap->connObjNum = connObjNum;
			tempOvLap->taskType = TaskType::RECV;
		}

		DWORD dwFlag = 0;
		DWORD dwRecvBytes = 0;

		int tempR = WSARecv(userSkt, &(tempOvLap->wsaBuf), 1, &dwRecvBytes, &dwFlag, (LPWSAOVERLAPPED)tempOvLap, NULL);

		if (tempR == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			std::cout << userSkt << " WSARecv Fail : " << WSAGetLastError() << std::endl;
			return false;
		}

		return true;
	}


	// =========================== SEND ===========================

	void PushSendMsg(const uint32_t dataSize_, char* sendMsg) {
		auto k = reinterpret_cast<PACKET_HEADER*>(sendMsg);
		std::cout << std::endl;

		OverlappedEx* tempOvLap = overLappedManager->getOvLap();

		if (tempOvLap == nullptr) { // Allocate new overlap if pool is empty
			OverlappedEx* overlappedEx = new OverlappedEx;
			ZeroMemory(overlappedEx, sizeof(OverlappedEx));
			overlappedEx->wsaBuf.len = MAX_RECV_SIZE;
			overlappedEx->wsaBuf.buf = new char[MAX_RECV_SIZE];
			overlappedEx->connObjNum = connObjNum;
			CopyMemory(overlappedEx->wsaBuf.buf, sendMsg, dataSize_);
			overlappedEx->taskType = TaskType::NEWSEND;

			sendQueue.push(overlappedEx); // Push Send Msg To User
			sendQueueSize.fetch_add(1);
		}
		else {
			tempOvLap->wsaBuf.len = MAX_RECV_SIZE;
			tempOvLap->wsaBuf.buf = new char[MAX_RECV_SIZE];
			tempOvLap->connObjNum = connObjNum;
			CopyMemory(tempOvLap->wsaBuf.buf, sendMsg, dataSize_);
			tempOvLap->taskType = TaskType::SEND;

			sendQueue.push(tempOvLap); // Push Send Msg To User
			sendQueueSize.fetch_add(1);
		}

		if (sendQueueSize.load() == 1) {
			ProcSend();
		}
	}

	void SendComplete() {
		sendQueueSize.fetch_sub(1);

		if (sendQueueSize.load() == 1) {
			ProcSend();
		}
	}

private:
	void ProcSend() {
		OverlappedEx* overlappedEx;

		if (sendQueue.pop(overlappedEx)) {
			DWORD dwSendBytes = 0;
			int sCheck = WSASend(userSkt,
				&(overlappedEx->wsaBuf),
				1,
				&dwSendBytes,
				0,
				(LPWSAOVERLAPPED)overlappedEx,
				NULL);
		}
	}

	// 1024 bytes
	char readData[1024] = { 0 };

	// 136 bytes 
	boost::lockfree::queue<OverlappedEx*> sendQueue{ 10 };

	// 120 bytes
	std::unique_ptr<CircularBuffer> circularBuffer;

	// 64 bytes
	char acceptBuf[64] = { 0 };

	// 56 bytes
	OverlappedEx acceptOvlap;

	// 8 bytes
	SOCKET userSkt;
	HANDLE sIOCPHandle;
	OverLappedManager* overLappedManager;

	// 2 bytes
	uint16_t connObjNum;
	uint16_t roomNum;
	uint16_t userRaidServerObjNum;

	// 1 bytes
	bool isConn = false;
	std::atomic<uint16_t> sendQueueSize{ 0 };
};