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

    if (bind(listener_socket, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) <
        0) {
        throw std::runtime_error(
            "Fatal: Could not bind socket address! Port: " +
            std::to_string(DEFAULT_PORT) + ". Errno: " + std::to_string(errno));
    }

    SC_APP_INFO("Server: Socket Created!");

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

void Server::update(float dt) {
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