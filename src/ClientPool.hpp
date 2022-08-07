#pragma once
#include "Server.hpp"
#include "Client.hpp"

namespace ClassicServer
{
    using namespace Stardust_Celeste;

    class ClientPool
    {
    public:
        ClientPool(Server *server);
        ~ClientPool();

        void add_client(int);
        void remove_client(int);

        static void update(ClientPool *);
        bool done;
    private:
        ScopePtr<std::thread> client_update_thread;
        Server *server;
        std::mutex client_mutex;
        std::vector<int> clients;
    };

}