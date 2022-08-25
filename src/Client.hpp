#pragma once
#include "ProtocolTypes.hpp"
#include "Utility.hpp"
#include <iostream>
#include <string>

namespace ClassicServer {
using namespace Stardust_Celeste;

class Server;

class Client {
  public:
    Client(int socket, std::string clientIP, Server *server, int pid);
    ~Client();

    static void update(Client *client);

    std::vector<RefPtr<Network::ByteBuffer>> packetsIn;

    std::mutex packetsOutMutex;
    std::vector<RefPtr<Network::ByteBuffer>> packetsOut;

    int PlayerID;
    std::string username;
    std::string ip;
    bool connected;
    bool isOP;

    Short X, Y, Z;
    Byte Yaw, Pitch;

    void send();

  private:
    int socket;
    Server *server;

    void send_init();
    void receive();

    void process_packet(RefPtr<Network::ByteBuffer> packet);

    ScopePtr<std::thread> update_thread;
};
} // namespace ClassicServer