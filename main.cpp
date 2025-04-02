#include <iostream>
#include <sw/redis++/redis++.h>

#include "RaidRoomManager.h"

int main() {
    std::shared_ptr<sw::redis::RedisCluster> redis;
    sw::redis::ConnectionOptions connection_options;

    try {
        connection_options.host = "127.0.0.1";  // Redis Cluster IP
        connection_options.port = 7001;  // Redis Cluster Master Node Port
        connection_options.socket_timeout = std::chrono::seconds(10);
        connection_options.keep_alive = true;

        // Redis Ŭ������ ����
        redis = std::make_shared<sw::redis::RedisCluster>(connection_options);
        std::cout << "Redis Cluster Connect Success !" << std::endl;

    }
    catch (const  sw::redis::Error& err) {
        std::cout << "Redis ���� �߻�: " << err.what() << std::endl;
    }

	RaidRoomManager raidRoomManager;

	return 0;
}