#pragma once
#include "Utility.hpp"
#include <map>

namespace ClassicServer
{
    using namespace Stardust_Celeste;

    class Client;

    class Server
    {
    public:
        Server();
        ~Server();

        void update(float dt);

        static auto connection_listener(Server *server) -> void;

    private:
        ThreadSafe<std::map<int, Client *>> clients;
        int id_count = 1;
        int listener_socket;

        ScopePtr<std::thread> listener_thread;
    };
}