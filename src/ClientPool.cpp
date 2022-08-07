#include "ClientPool.hpp"
#include "Server.hpp"

namespace ClassicServer
{
    ClientPool::ClientPool(Server *server)
    {
        this->server = server;
        done = false;
        client_update_thread = create_scopeptr<std::thread>(ClientPool::update, this);
    }


    ClientPool::~ClientPool() {
        client_update_thread->join();
    }

    void ClientPool::add_client(int id)
    {
        std::lock_guard lg(client_mutex);
        clients.push_back(id);
    }
    void ClientPool::remove_client(int id)
    {
        std::lock_guard lg(client_mutex);
        int i = -1;
        for (int c = 0; c < clients.size(); c++) {
            auto cid = clients[c];

            if (cid == id) {
                i = c;
            }
        }

        if (i >= 0)
            clients.erase(clients.begin() + i);
        
    }

    void ClientPool::update(ClientPool* cp)
    {
        while (!cp->done) {
            
            std::vector<int> toRemove;
            for (auto& id : cp->clients) {
                //Find 
                if (cp->server->clients.find(id) != cp->server->clients.end()) {
                    auto client = cp->server->clients[id];
                    client->update();
                }
                else {
                    toRemove.push_back(id);
                }
            }

            for (auto& id : toRemove) {
                cp->remove_client(id);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}