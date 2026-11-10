#pragma once
#include <thread>
#include <atomic>
#include <arpa/inet.h>

class UdpServer {
    private:
        void server_loop();


        int udp_socket;
        struct sockaddr_in server_addr;
        std::atomic<bool> server_running{false};

        std::thread udp_server_thread;
    public:
        UdpServer(int port);
        ~UdpServer();
};