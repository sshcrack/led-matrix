#pragma once
#include <thread>
#include <arpa/inet.h>

class UdpServer {
    private:
        void server_loop();


        int udp_socket;
        struct sockaddr_in server_addr;
        bool server_running;

        std::thread udp_server_thread;
    public:
        UdpServer(int port);
        ~UdpServer();
};