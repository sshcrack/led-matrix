#pragma once
#include <thread>

class UdpServer {
    private:
        void server_loop();


        int udp_socket;
        struct sockaddr_in server_addr;
        std::thread udp_server_thread;
    public:
        UdpServer(int port);
        ~UdpServer();
};