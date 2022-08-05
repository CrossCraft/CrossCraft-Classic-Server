#pragma once
#include "Banlist.hpp"
#include "Utility.hpp"
#include "World/World.hpp"
#include <Core/Application.hpp>
#include <map>

namespace ClassicServer {
using namespace Stardust_Celeste;

class Client;
class World;

class Server {
  public:
    Server();
    ~Server();

    void process_command(std::string cmd, bool op);

    void update(float dt, Core::Application *app);
    void broadcast_packet(RefPtr<Network::ByteBuffer>);
    static auto connection_listener(Server *server) -> void;
    static auto command_listener(Server *server) -> void;

    std::mutex broadcast_mutex;
    std::vector<RefPtr<Network::ByteBuffer>> broadcast_list;

    std::mutex client_mutex;
    std::map<int, Client *> clients;

    RefPtr<World> world;

    BanList bans;

  private:
    bool stopping = false;
    int stop_tcount = 20;
    int id_count = 1;
    int listener_socket;

    int pingCounter;

    ScopePtr<std::thread> listener_thread;
    ScopePtr<std::thread> command_thread;
};
} // namespace ClassicServer