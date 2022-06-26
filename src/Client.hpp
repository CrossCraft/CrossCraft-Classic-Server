#pragma once
#include "Utility.hpp"
#include <string>

namespace ClassicServer
{
    using namespace Stardust_Celeste;

    class Server;

    class Client
    {
    public:
        Client(int socket);
        ~Client();

        static void update(Client* client);

        std::vector<RefPtr<Network::ByteBuffer>> packetsIn;

        std::mutex packetsOutMutex;
        std::vector<RefPtr<Network::ByteBuffer>> packetsOut;

        int PlayerID;

    private:
        int socket;
        bool connected;

        std::string username;

        ScopePtr<std::thread> client_update_thread;

        void send();
        void send_init();
        void receive();

        void process_packet(RefPtr<Network::ByteBuffer> packet);
    };
}