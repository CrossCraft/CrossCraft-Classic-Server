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

    key = "";
    std::ifstream f("key.txt");

    if (f.is_open())
        f >> key;

    f.close();
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

void Server::process_command(std::string cmd, bool op, std::string user) {
    auto firstArg = cmd.substr(0, cmd.find_first_of(" "));
    auto remaining = cmd.substr(cmd.find_first_of(" ") + 1);

    if (firstArg == "/kick" && op) {

        auto secondArg = remaining.substr(0, remaining.find_first_of(" "));
        int kicked = -1;

        for (auto c : clients) {
            if (secondArg == c.second->username) {
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
    } else if (firstArg == "/unban" && op) {
        auto secondArg = remaining.substr(0, remaining.find_first_of(" "));

        if (bans.is_banned(secondArg)) {
            bans.unban(secondArg);
        }
    } else if (firstArg == "/op" && op) {
        auto secondArg = remaining.substr(0, remaining.find_first_of(" "));

        int oper = -1;
        for (auto c : clients) {
            if (secondArg == c.second->username) {
                oper = c.first;
                break;
            }
        }

        if (oper < 0)
            return;

        clients[oper]->isOP = true;

        auto ptr3 = create_refptr<Outgoing::Message>();
        auto reason = "&bYou were promoted to operator!";
        ptr3->PacketID = Outgoing::OutPacketTypes::eMessage;
        ptr3->PlayerID = 255;
        memset(ptr3->Message.contents, 0x20, STRING_LENGTH);
        memcpy(ptr3->Message.contents, reason,
               strlen(reason) < STRING_LENGTH ? strlen(reason) : STRING_LENGTH);

        clients[oper]->packetsOut.push_back(
            Outgoing::createOutgoingPacket(ptr3.get()));
    } else if (firstArg == "/deop" && op) {
        auto secondArg = remaining.substr(0, remaining.find_first_of(" "));
        int oper = -1;
        for (auto c : clients) {
            if (secondArg == c.second->username) {
                if (c.second->isOP)
                    oper = c.first;
                break;
            }
        }

        if (oper < 0)
            return;

        clients[oper]->isOP = false;

        auto ptr3 = create_refptr<Outgoing::Message>();
        auto reason = "&cYou were demoted!";
        ptr3->PacketID = Outgoing::OutPacketTypes::eMessage;
        ptr3->PlayerID = 255;
        memset(ptr3->Message.contents, 0x20, STRING_LENGTH);
        memcpy(ptr3->Message.contents, reason,
               strlen(reason) < STRING_LENGTH ? strlen(reason) : STRING_LENGTH);

        clients[oper]->packetsOut.push_back(
            Outgoing::createOutgoingPacket(ptr3.get()));
    } else if (firstArg == "/ban" && op) {
        auto secondArg = remaining.substr(0, remaining.find_first_of(" "));
        int kicked = -1;

        for (auto c : clients) {
            if (secondArg == c.second->username) {
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
        bans.add_ban(secondArg);
    } else if (firstArg == "/stop" && op) {
        auto reason = "&6Server is stopping!";
        auto ptr = create_refptr<Outgoing::Disconnect>();
        ptr->PacketID = Outgoing::OutPacketTypes::eDisconnect;
        memset(ptr->Reason.contents, 0x20, STRING_LENGTH);
        memcpy(ptr->Reason.contents, reason,
               strlen(reason) < STRING_LENGTH ? strlen(reason) : STRING_LENGTH);
        broadcast_packet(Outgoing::createOutgoingPacket(ptr.get()));

        stopping = true;
    }
    else if (firstArg == "/list") {
        int player = -1;
        for (auto c : clients) {
            if (user == c.second->username) {
                player = c.first;
                break;
            }
        }

        if (player < 0)
            return;

        auto ptr1 = create_refptr<Outgoing::Message>();
        ptr1->PacketID = Outgoing::OutPacketTypes::eMessage;
        ptr1->PlayerID = 0;
        auto msg = std::string("&aCurrent players online:");

        memset(ptr1->Message.contents, 0x20, STRING_LENGTH);
        memcpy(ptr1->Message.contents, msg.c_str(),
            msg.length() < STRING_LENGTH ? msg.length() : STRING_LENGTH);


        clients[player]->packetsOut.push_back(
            Outgoing::createOutgoingPacket(ptr1.get()));

        for (auto c : clients) {
            auto ptr2 = create_refptr<Outgoing::Message>();
            ptr2->PacketID = Outgoing::OutPacketTypes::eMessage;
            ptr2->PlayerID = 0;

            auto msgs = std::string("&f") + c.second->username;

            memset(ptr2->Message.contents, 0x20, STRING_LENGTH);
            memcpy(ptr2->Message.contents, msgs.c_str(),
                msgs.length() < STRING_LENGTH ? msgs.length() : STRING_LENGTH);

            clients[player]->packetsOut.push_back(
                Outgoing::createOutgoingPacket(ptr2.get()));
        }
    }
    else if (firstArg == "/msg") {
        auto secondArg = remaining.substr(0, remaining.find_first_of(" "));
        remaining = remaining.substr(remaining.find_first_of(" ") + 1);
        auto message = remaining;

        int player1 = -1;
        for (auto c : clients) {
            if (user == c.second->username) {
                player1 = c.first;
                break;
            }
        }

        int player2 = -1;
        for (auto c : clients) {
            if (secondArg == c.second->username) {
                player2 = c.first;
                break;
            }
        }

        if (player1 < 0 || player2 < 0)
            return;

        auto ptr1 = create_refptr<Outgoing::Message>();
        ptr1->PacketID = Outgoing::OutPacketTypes::eMessage;
        ptr1->PlayerID = 0;
        auto msg = std::string("&7[") + user + " -> " + clients[player2]->username + "]: " + remaining;
        memset(ptr1->Message.contents, 0x20, STRING_LENGTH);
        memcpy(ptr1->Message.contents, msg.c_str(),
            msg.length() < STRING_LENGTH ? msg.length() : STRING_LENGTH);

        clients[player1]->packetsOut.push_back(
            Outgoing::createOutgoingPacket(ptr1.get()));
        clients[player2]->packetsOut.push_back(
            Outgoing::createOutgoingPacket(ptr1.get()));
    }
    else if (firstArg == "/tp") {

        int player = -1;
        for (auto c : clients) {
            if (user == c.second->username) {
                player = c.first;
                break;
            }
        }

        if (player < 0)
            return;

        auto secondArg = remaining.substr(0, remaining.find_first_of(" "));
        remaining = remaining.substr(remaining.find_first_of(" ") + 1);

        auto thirdArg = remaining.substr(0, remaining.find_first_of(" "));
        remaining = remaining.substr(remaining.find_first_of(" ") + 1);

        auto fourthArg = remaining.substr(0, remaining.find_first_of(" "));
        remaining = remaining.substr(remaining.find_first_of(" ") + 1);

        int x = atoi(secondArg.c_str());
        int y = atoi(secondArg.c_str());
        int z = atoi(secondArg.c_str());

        if (x >= 0 && x < 256 && y >= 0 && z >= 0 && z < 256) {
            auto ptr1 = create_refptr<Outgoing::Message>();
            ptr1->PacketID = Outgoing::OutPacketTypes::eMessage;
            ptr1->PlayerID = 0;
            auto msg = std::string("&aTeleporting to coordinates!");

            memset(ptr1->Message.contents, 0x20, STRING_LENGTH);
            memcpy(ptr1->Message.contents, msg.c_str(),
                msg.length() < STRING_LENGTH ? msg.length() : STRING_LENGTH);

            clients[player]->packetsOut.push_back(
                Outgoing::createOutgoingPacket(ptr1.get()));


            auto ptr2 = create_refptr<Outgoing::PlayerTeleport>();
            ptr2->PacketID = Outgoing::OutPacketTypes::ePlayerTeleport;
            ptr2->PlayerID = 255;
            ptr2->X = x * 32;
            ptr2->Y = y * 32;
            ptr2->Z = z * 32;
            ptr2->Yaw = 0;
            ptr2->Pitch = 0;

            clients[player]->packetsOut.push_back(
                Outgoing::createOutgoingPacket(ptr2.get()));
        }
        else {
            auto ptr1 = create_refptr<Outgoing::Message>();
            ptr1->PacketID = Outgoing::OutPacketTypes::eMessage;
            ptr1->PlayerID = 0;
            auto msg = std::string("&cBad coordinates!");

            memset(ptr1->Message.contents, 0x20, STRING_LENGTH);
            memcpy(ptr1->Message.contents, msg.c_str(),
                msg.length() < STRING_LENGTH ? msg.length() : STRING_LENGTH);

            clients[player]->packetsOut.push_back(
                Outgoing::createOutgoingPacket(ptr1.get()));
        }

    }
    else {
        int player = -1;
        for (auto c : clients) {
            if (user == c.second->username) {
                player = c.first;
                break;
            }
        }

        if (player < 0)
            return;

        auto ptr1 = create_refptr<Outgoing::Message>();
        ptr1->PacketID = Outgoing::OutPacketTypes::eMessage;
        ptr1->PlayerID = 0;
        auto msg = std::string("&cUnknown Command!");

        memset(ptr1->Message.contents, 0x20, STRING_LENGTH);
        memcpy(ptr1->Message.contents, msg.c_str(),
            msg.length() < STRING_LENGTH ? msg.length() : STRING_LENGTH);

        clients[player]->packetsOut.push_back(
            Outgoing::createOutgoingPacket(ptr1.get()));
    }
}

auto Server::command_listener(Server *server) -> void {

    while (true) {
        char cmd[256];
        memset(cmd, 0, 256);

        std::cin.getline(cmd, 256);

        if (cmd[0] == '/') {
            // Process Command
            server->process_command(std::string(cmd), true, "CONSOLE");
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
