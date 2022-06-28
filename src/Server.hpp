#pragma once
#include "Utility.hpp"
#include "World/World.hpp"
#include <map>

namespace ClassicServer {
using namespace Stardust_Celeste;

class Client;
class World;

class Server {
  public:
    Server();
    ~Server();

    void update(float dt);
    void broadcast_packet(RefPtr<Network::ByteBuffer>);
    static auto connection_listener(Server *server) -> void;

    std::mutex broadcast_mutex;
    std::vector<RefPtr<Network::ByteBuffer>> broadcast_list;

    std::mutex client_mutex;
    std::map<int, Client *> clients;

    RefPtr<World> world;

  private:
    int id_count = 1;
    int listener_socket;

    int pingCounter;

    ScopePtr<std::thread> listener_thread;
};
} // namespace ClassicServer