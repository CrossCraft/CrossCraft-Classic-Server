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
    Client(int socket, Server *server, int pid);
    ~Client();

    static void update(Client *client);

    std::vector<RefPtr<Network::ByteBuffer>> packetsIn;

    std::mutex packetsOutMutex;
    std::vector<RefPtr<Network::ByteBuffer>> packetsOut;

    int PlayerID;
    std::string username;
    bool connected;
    bool isOP;

    Short X, Y, Z;
    Byte Yaw, Pitch;

    void send();

  private:
    int socket;

    ScopePtr<std::thread> client_update_thread;
    Server *server;

    void send_init();
    void receive();

    void process_packet(RefPtr<Network::ByteBuffer> packet);
};
} // namespace ClassicServer