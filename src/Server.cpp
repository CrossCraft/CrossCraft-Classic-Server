#include "Server.hpp"
#include "Client.hpp"
#include "OutgoingPackets.hpp"
#include "World/Generation/ClassicGenerator.hpp"
#include "World/Generation/CrossCraftGenerator.hpp"

const int DEFAULT_PORT = 25565;

namespace ClassicServer {
Server::Server() {
    SC_APP_INFO("Server Starting...");
    SC_APP_INFO("Server: Creating Socket...");

    listener_socket = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));

    if (listener_socket == -1)
        throw std::runtime_error("Fatal: Could not open socket! Errno: " +
                                 std::to_string(errno));

    sockaddr_in sockaddr{};
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    sockaddr.sin_port = htons(DEFAULT_PORT);

    int flag = 1;
#if BUILD_PLAT != BUILD_WINDOWS
    setsockopt(listener_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&flag,
               sizeof(int));
    setsockopt(listener_socket, SOL_SOCKET, SO_REUSEPORT, (void *)&flag,
               sizeof(int));
#endif

    if (bind(listener_socket, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) <
        0) {
        throw std::runtime_error(
            "Fatal: Could not bind socket address! Port: " +
            std::to_string(DEFAULT_PORT) + ". Errno: " + std::to_string(errno));
    }

    SC_APP_INFO("Server: Socket Created!");

    setsockopt(listener_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag,
               sizeof(int));

    world = create_refptr<World>(this);
    world->cfg = Config::loadConfig();

    FILE *fptr = fopen("save.ccc", "r");
    if (fptr) {
        if (!world->load_world())
            if (world->cfg.compat)
                ClassicGenerator::generate(world.get());
            else
                CrossCraftGenerator::generate(world.get());
        fclose(fptr);
    } else {
        if (world->cfg.compat)
            ClassicGenerator::generate(world.get());
        else
            CrossCraftGenerator::generate(world.get());
    }

    world->start_autosave();

#if BUILD_PLAT == BUILD_POSIX
    signal(SIGPIPE, SIG_IGN);
#endif

    SC_APP_INFO("Server: Creating Listener...");
    listener_thread =
        create_scopeptr<std::thread>(Server::connection_listener, this);
    command_thread =
        create_scopeptr<std::thread>(Server::command_listener, this);
    pingCounter = 0;
}

Server::~Server() {
#if BUILD_PLAT == BUILD_WINDOWS
    closesocket(listener_socket);
#else
    ::close(listener_socket);
#endif
}

void Server::broadcast_packet(RefPtr<Network::ByteBuffer> p) {
    std::lock_guard lg(client_mutex);

    for (auto &[id, c] : clients) {

        Byte pID = 0;
        p->ReadU8(pID);

        if (pID == 0x7 || pID == 0x8) {
            SByte playerID = 0;
            p->ReadI8(playerID);

            if (playerID == id) {
                p->ResetRead();
                continue;
            }
        }

        p->ResetRead();
        c->packetsOutMutex.lock();
        c->packetsOut.push_back(p);
        c->packetsOutMutex.unlock();
    }
}

void Server::update(float dt, Core::Application *app) {
    std::vector<int> idForDeletion;
    for (auto &[id, c] : clients) {
        if (!c->connected) {
            idForDeletion.push_back(id);
        }
    }

    for (auto id : idForDeletion) {
        auto ptr = create_refptr<Outgoing::DespawnPlayer>();
        ptr->PacketID = Outgoing::OutPacketTypes::eDespawnPlayer;
        ptr->PlayerID = id;
        broadcast_packet(Outgoing::createOutgoingPacket(ptr.get()));

        auto ptr2 = create_refptr<Outgoing::Message>();
        ptr2->PacketID = Outgoing::OutPacketTypes::eMessage;
        ptr2->PlayerID = 0;
        auto msg = "&e" + clients[id]->username + " left the game";

        std::cout << msg << std::endl;
        memset(ptr2->Message.contents, 0x20, STRING_LENGTH);
        memcpy(ptr2->Message.contents, msg.c_str(),
               msg.length() < STRING_LENGTH ? msg.length() : STRING_LENGTH);

        broadcast_packet(Outgoing::createOutgoingPacket(ptr2.get()));
    }

    client_mutex.lock();
    for (auto id : idForDeletion) {
        delete clients[id];
        clients.erase(id);
    }
    client_mutex.unlock();

    pingCounter++;
    if (pingCounter > 100) {
        pingCounter = 0;

        auto ptr = create_refptr<Outgoing::Ping>();
        ptr->PacketID = Outgoing::OutPacketTypes::ePing;

        broadcast_packet(Outgoing::createOutgoingPacket(ptr.get()));
    }

    broadcast_mutex.lock();
    for (auto &b : broadcast_list) {
        broadcast_packet(b);
    }
    broadcast_list.clear();
    broadcast_mutex.unlock();

    world->update(dt);

    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(50));

    if (stopping) {

        stop_tcount -= 1;
        if (stop_tcount < 0) {
            std::vector<int> ids;
            for (auto &[key, val] : clients) {
                ids.push_back(key);
            }

            for (auto id : ids) {
                clients[id]->send();
                clients[id]->connected = false;
                delete clients[id];
                clients.erase(id);
            }

            if (stop_tcount < -50) {
                app->exit();
            }
        }
    }
}

void Server::process_command(std::string cmd, bool op) {
    auto firstArg = cmd.substr(0, cmd.find_first_of(" "));
    auto remaining = cmd.substr(cmd.find_first_of(" ") + 1);

    if (firstArg == "/kick") {
        int kicked = -1;

        for (auto c : clients) {
            if (remaining == c.second->username) {
                kicked = c.first;
                break;
            }
        }

        if (kicked < 0)
            return;

        auto ptr = create_refptr<Outgoing::DespawnPlayer>();
        ptr->PacketID = Outgoing::OutPacketTypes::eDespawnPlayer;
        ptr->PlayerID = kicked;
        broadcast_packet(Outgoing::createOutgoingPacket(ptr.get()));

        auto ptr2 = create_refptr<Outgoing::Message>();
        ptr2->PacketID = Outgoing::OutPacketTypes::eMessage;
        ptr2->PlayerID = 0;
        auto msg = "&e" + clients[kicked]->username + " was kicked!";

        std::cout << msg << std::endl;
        memset(ptr2->Message.contents, 0x20, STRING_LENGTH);
        memcpy(ptr2->Message.contents, msg.c_str(),
               msg.length() < STRING_LENGTH ? msg.length() : STRING_LENGTH);

        broadcast_packet(Outgoing::createOutgoingPacket(ptr2.get()));

        auto ptr3 = create_refptr<Outgoing::Disconnect>();
        auto reason = "&4You were kicked!";
        ptr3->PacketID = Outgoing::OutPacketTypes::eDisconnect;
        memset(ptr3->Reason.contents, 0x20, STRING_LENGTH);
        memcpy(ptr3->Reason.contents, reason,
               strlen(reason) < STRING_LENGTH ? strlen(reason) : STRING_LENGTH);

        clients[kicked]->packetsOut.push_back(
            Outgoing::createOutgoingPacket(ptr3.get()));
        clients[kicked]->send();
        clients[kicked]->connected = false;
        delete clients[kicked];
        clients.erase(kicked);
    }

    if (firstArg == "/unban") {
        if (bans.is_banned(remaining)) {
            bans.unban(remaining);
        }
    }

    if (firstArg == "/ban") {
        int kicked = -1;

        for (auto c : clients) {
            if (remaining == c.second->username) {
                kicked = c.first;
                break;
            }
        }

        if (kicked < 0)
            return;

        auto ptr = create_refptr<Outgoing::DespawnPlayer>();
        ptr->PacketID = Outgoing::OutPacketTypes::eDespawnPlayer;
        ptr->PlayerID = kicked;
        broadcast_packet(Outgoing::createOutgoingPacket(ptr.get()));

        auto ptr2 = create_refptr<Outgoing::Message>();
        ptr2->PacketID = Outgoing::OutPacketTypes::eMessage;
        ptr2->PlayerID = 0;
        auto msg = "&e" + clients[kicked]->username + " was banned!";

        std::cout << msg << std::endl;
        memset(ptr2->Message.contents, 0x20, STRING_LENGTH);
        memcpy(ptr2->Message.contents, msg.c_str(),
               msg.length() < STRING_LENGTH ? msg.length() : STRING_LENGTH);

        broadcast_packet(Outgoing::createOutgoingPacket(ptr2.get()));

        auto ptr3 = create_refptr<Outgoing::Disconnect>();
        auto reason = "&4You were banned!";
        ptr3->PacketID = Outgoing::OutPacketTypes::eDisconnect;
        memset(ptr3->Reason.contents, 0x20, STRING_LENGTH);
        memcpy(ptr3->Reason.contents, reason,
               strlen(reason) < STRING_LENGTH ? strlen(reason) : STRING_LENGTH);

        clients[kicked]->packetsOut.push_back(
            Outgoing::createOutgoingPacket(ptr3.get()));
        clients[kicked]->send();
        clients[kicked]->connected = false;
        delete clients[kicked];
        clients.erase(kicked);

        bans.add_ban(remaining);
    }

    if (firstArg == "/stop") {
        auto reason = "&6Server is stopping!";
        auto ptr = create_refptr<Outgoing::Disconnect>();
        ptr->PacketID = Outgoing::OutPacketTypes::eDisconnect;
        memset(ptr->Reason.contents, 0x20, STRING_LENGTH);
        memcpy(ptr->Reason.contents, reason,
               strlen(reason) < STRING_LENGTH ? strlen(reason) : STRING_LENGTH);
        broadcast_packet(Outgoing::createOutgoingPacket(ptr.get()));

        stopping = true;
    }
}

auto Server::command_listener(Server *server) -> void {

    while (true) {
        char cmd[256];
        memset(cmd, 0, 256);

        std::cin.getline(cmd, 256);

        if (cmd[0] == '/') {
            // Process Command
            server->process_command(std::string(cmd), true);
        } else {
            auto ptr2 = create_refptr<Outgoing::Message>();
            ptr2->PacketID = Outgoing::OutPacketTypes::eMessage;
            ptr2->PlayerID = 0;
            auto msg = "&4[Console]: " + std::string(cmd);

            std::cout << msg << std::endl;
            memset(ptr2->Message.contents, 0x20, STRING_LENGTH);
            memcpy(ptr2->Message.contents, msg.c_str(),
                   msg.length() < STRING_LENGTH ? msg.length() : STRING_LENGTH);

            server->broadcast_packet(
                Outgoing::createOutgoingPacket(ptr2.get()));
        }
    }
}

auto Server::connection_listener(Server *server) -> void {
    while (true) {
        SC_APP_INFO("Server: Ready and Listening For Connections!");

        sockaddr_in socketAddress{};
        socketAddress.sin_family = AF_INET;
        socketAddress.sin_addr.s_addr = INADDR_ANY;
        socketAddress.sin_port = htons(DEFAULT_PORT);

        if (::listen(server->listener_socket, 1) < 0) {
            throw std::runtime_error(
                "Fatal: Could not listen on socket. Errno: " +
                std::to_string(errno));
        }

        auto addressLen = sizeof(socketAddress);
        auto conn = static_cast<int>(accept(server->listener_socket,
                                            (struct sockaddr *)&socketAddress,
                                            (socklen_t *)&addressLen));
        SC_APP_INFO("Connecting...");

        if (conn < 0) {
            SC_APP_ERROR("COULD NOT CONNECT!");
            continue;
        }

        SC_APP_INFO("New Connection from " +
                    std::string((inet_ntoa(socketAddress.sin_addr))) +
                    " on port " +
                    std::to_string(ntohs(socketAddress.sin_port)));

        auto client = new Client(conn, server);
        client->PlayerID = server->id_count;

        std::lock_guard lg(server->client_mutex);
        server->clients.emplace(server->id_count++, client);
    }
}
} // namespace ClassicServer
