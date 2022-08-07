#include "Client.hpp"
#include "IncomingPackets.hpp"
#include "OutgoingPackets.hpp"
#include "Server.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <zlib.h>

namespace ClassicServer
{

    Client::Client(int socket, Server *server, int pid)
    {
        this->PlayerID = pid;
        this->socket = socket;
        this->server = server;
        this->isOP = false;

        SC_APP_INFO("Client Created!");
        connected = true;

        int flag = 1;
        setsockopt(this->socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag,
                   sizeof(int));

        packetsIn.clear();
        packetsOut.clear();

        X = 64 * 32;
        Y = 32 * 32 + 51;
        Z = 64 * 32;
        Yaw = 0;
        Pitch = 0;
    }
    Client::~Client()
    {
        send();
#if BUILD_PLAT == BUILD_WINDOWS
        closesocket(socket);
#else
        close(socket);
#endif
    }

    void Client::process_packet(RefPtr<Network::ByteBuffer> packet)
    {
        auto packet_data = Incoming::readIncomingPacket(packet);
        auto id = packet_data->PacketID;
        switch (static_cast<Incoming::InPacketTypes>(id))
        {

        case Incoming::InPacketTypes::ePlayerIdentification:
        {
            auto data = (Incoming::PlayerIdentification *)packet_data.get();
            data->Username.contents[STRING_LENGTH - 1] = 0;
            username = std::string((char *)data->Username.contents);
            username = username.substr(0, username.find_first_of(0x20));

            std::string key = std::string((char *)data->VerificationKey.contents);
            key = key.substr(0, key.find_first_of(0x20));

            if (server->key != "")
            {
                if (server->key != key)
                {
                    auto ptr = create_refptr<Outgoing::Disconnect>();
                    ptr->PacketID = Outgoing::OutPacketTypes::eDisconnect;
                    memset(ptr->Reason.contents, 0x20, STRING_LENGTH);
                    auto reason = std::string("&dYou are not verified!");
                    memcpy(ptr->Reason.contents, reason.c_str(),
                           reason.length() < STRING_LENGTH ? reason.length()
                                                           : STRING_LENGTH);
                    packetsOut.push_back(Outgoing::createOutgoingPacket(ptr.get()));
                    connected = false;
                    send();
                    return;
                }
            }

            if (server->bans.is_banned(username))
            {
                auto ptr = create_refptr<Outgoing::Disconnect>();
                ptr->PacketID = Outgoing::OutPacketTypes::eDisconnect;
                memset(ptr->Reason.contents, 0x20, STRING_LENGTH);
                auto reason = std::string("&4You are banned!");
                memcpy(ptr->Reason.contents, reason.c_str(),
                       reason.length() < STRING_LENGTH ? reason.length()
                                                       : STRING_LENGTH);
                packetsOut.push_back(Outgoing::createOutgoingPacket(ptr.get()));
                connected = false;
                send();
                return;
            }

            bool dupe = false;
            for (auto &c : server->clients)
            {
                if (c.second->username == username && c.second->PlayerID != PlayerID)
                {
                    dupe = true;
                    break;
                }
            }

            if (dupe)
            {
                auto ptr = create_refptr<Outgoing::Disconnect>();
                ptr->PacketID = Outgoing::OutPacketTypes::eDisconnect;
                memset(ptr->Reason.contents, 0x20, STRING_LENGTH);
                auto reason = std::string("&4You are joined in another session!");
                memcpy(ptr->Reason.contents, reason.c_str(),
                       reason.length() < STRING_LENGTH ? reason.length()
                                                       : STRING_LENGTH);
                packetsOut.push_back(Outgoing::createOutgoingPacket(ptr.get()));
                connected = false;
                send();
                return;
            }

            send_init();

            break;
        }
        case Incoming::InPacketTypes::eMessage:
        {
            auto ptr = create_refptr<Outgoing::Message>();
            ptr->PacketID = Outgoing::OutPacketTypes::eMessage;
            ptr->PlayerID = PlayerID;
            memset(ptr->Message.contents, 0x20, STRING_LENGTH);

            auto data = (Incoming::Message *)packet_data.get();
            data->Message.contents[STRING_LENGTH - 1] = 0;

            auto msg = std::string((char *)data->Message.contents);

            if (msg[0] == '/')
            {
                server->process_command(msg, isOP, username);
            }
            else
            {
                std::string message = username + ": " + msg;

                std::cout << message << std::endl;
                memcpy(ptr->Message.contents, message.c_str(), STRING_LENGTH);

                server->broadcast_packet(Outgoing::createOutgoingPacket(ptr.get()));
            }
            break;
        }

        case Incoming::InPacketTypes::ePositionAndOrientation:
        {
            auto data = (Incoming::PositionAndOrientation *)packet_data.get();

            X = data->X;
            Y = data->Y;
            Z = data->Z;
            Yaw = data->Yaw;
            Pitch = data->Pitch;

            break;
        }

        case Incoming::InPacketTypes::eSetBlock:
        {
            auto ptr = create_refptr<Outgoing::SetBlock>();
            ptr->PacketID = Outgoing::OutPacketTypes::eSetBlock;

            auto data = (Incoming::SetBlock *)packet_data.get();
            ptr->X = data->X;
            ptr->Y = data->Y;
            ptr->Z = data->Z;

            auto idx = (data->Y * 256 * 256) + (data->Z * 256) + data->X + 4;

            if (data->Mode == 0)
                server->world->worldData[idx] = 0;
            else
            {
                auto blk = data->BlockType;
                auto blk2 = server->world->worldData[server->world->getIdx(
                    data->X, data->Y - 1, data->Z)];

                if (!(((blk == Block::Flower1 || blk == Block::Flower2 ||
                        blk == Block::Sapling) &&
                       blk2 != Block::Grass && blk2 != Block::Dirt) ||
                      ((blk == Block::Mushroom1 || blk == Block::Mushroom2) &&
                       (blk2 != Block::Stone && blk2 != Block::Cobblestone &&
                        blk2 != Block::Gravel))))
                    server->world->worldData[idx] = data->BlockType;
            }

            ptr->BlockType = server->world->worldData[idx];

            server->world->update_nearby_blocks({data->X, data->Y, data->Z});

            server->broadcast_mutex.lock();
            server->broadcast_list.push_back(
                Outgoing::createOutgoingPacket(ptr.get()));
            server->broadcast_mutex.unlock();
            break;
        }
        }
    }

    void Client::send_init()
    {

        auto ptr = create_scopeptr<Outgoing::ServerIdentification>();
        ptr->PacketID = Outgoing::OutPacketTypes::eServerIdentification;
        ptr->ProtocolVersion = 0x07;
        memset(ptr->ServerName.contents, 0x20, STRING_LENGTH);
        memcpy(ptr->ServerName.contents, "CrossCraft-Classic-Server", 26);
        memset(ptr->MOTD.contents, 0x20, STRING_LENGTH);
        memcpy(ptr->MOTD.contents, "A basic server", 15);
        ptr->UserType = 0x00;

        auto ptr2 = create_refptr<Outgoing::LevelInitialize>();
        ptr2->PacketID = Outgoing::OutPacketTypes::eLevelInitialize;

        packetsOutMutex.lock();
        packetsOut.push_back(Outgoing::createOutgoingPacket(ptr.get()));
        packetsOut.push_back(Outgoing::createOutgoingPacket(ptr2.get()));
        packetsOutMutex.unlock();

        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = 256 * 64 * 256 + 4;
        strm.next_in = (Bytef *)server->world->worldData;
        strm.avail_out = 0;
        strm.next_out = Z_NULL;

        int ret = deflateInit2(&strm, Z_BEST_COMPRESSION, Z_DEFLATED,
                               (MAX_WBITS + 16), 8, Z_DEFAULT_STRATEGY);
        if (ret != Z_OK)
        {
            throw std::runtime_error("INVALID START!");
        }

        char *outBuffer = new char[256 * 64 * 256 + 4];

        strm.avail_out = 256 * 64 * 256 + 4;
        strm.next_out = (Bytef *)outBuffer;

        ret = deflate(&strm, Z_FINISH);

        switch (ret)
        {
        case Z_NEED_DICT:
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            throw std::runtime_error("Zlib error: inflate()");
        }

        deflateEnd(&strm);
        int len = strm.total_out;

        size_t bytes = 0;
        while (bytes < len)
        {
            size_t remainingBytes = len - bytes;
            size_t count = (remainingBytes >= 1024) ? 1024 : (remainingBytes);

            auto ptr3 = create_scopeptr<Outgoing::LevelDataChunk>();
            ptr3->PacketID = Outgoing::OutPacketTypes::eLevelDataChunk;
            ptr3->ChunkLength = count;
            memset(ptr3->ChunkData.contents, 0, ARRAY_LENGTH);
            memcpy(ptr3->ChunkData.contents, (void *)&outBuffer[bytes], count);

            bytes += count;

            char percent = (char)(((float)bytes / (float)len) * 100.0f);
            ptr3->PercentComplete = percent;

            packetsOutMutex.lock();
            packetsOut.push_back(Outgoing::createOutgoingPacket(ptr3.get()));
            packetsOutMutex.unlock();
        }

        delete[] outBuffer;

        auto ptr4 = create_refptr<Outgoing::LevelFinalize>();
        ptr4->PacketID = Outgoing::OutPacketTypes::eLevelFinalize;
        ptr4->XSize = 256;
        ptr4->YSize = 64;
        ptr4->ZSize = 256;

        packetsOutMutex.lock();
        packetsOut.push_back(Outgoing::createOutgoingPacket(ptr4.get()));
        packetsOutMutex.unlock();

        auto ptr5 = create_refptr<Outgoing::Message>();
        ptr5->PacketID = Outgoing::OutPacketTypes::eMessage;
        ptr5->PlayerID = 0; // 0 = console
        memset(ptr5->Message.contents, 0x20, STRING_LENGTH);

        auto greeter = "&eWelcome to CrossCraft-Classic Server, " + username + "!";
        memcpy(ptr5->Message.contents, greeter.c_str(),
               greeter.length() < STRING_LENGTH ? greeter.length() : STRING_LENGTH);

        packetsOutMutex.lock();
        packetsOut.push_back(Outgoing::createOutgoingPacket(ptr5.get()));
        packetsOutMutex.unlock();

        bool spawned = false;
        int count = 30;

        while (!spawned && count >= 0)
        {
            count--;
            int x = rand() % 64 - 32 + 128;
            int z = rand() % 64 - 32 + 128;

            for (int y = 50; y > 32; y--)
            {
                auto blk = server->world->worldData[server->world->getIdx(x, y, z)];

                if (blk != 0 && blk != 8)
                {
                    X = x * 32 + 16;
                    Y = (y + 1) * 32 + 51;
                    Z = z * 32 + 16;
                    Yaw = 0;
                    Pitch = 0;

                    SC_APP_INFO("SPAWNED AT {} {} {} {}", x, y, z, blk);
                    goto spawn;
                }
            }
        }
        X = 128 * 32;
        Y = 60 * 32 + 51;
        Z = 128 * 32;
        Yaw = 0;
        Pitch = 0;

    spawn:
        auto ptr6 = create_refptr<Outgoing::SpawnPlayer>();
        ptr6->PacketID = Outgoing::OutPacketTypes::eSpawnPlayer;
        ptr6->PlayerID = 255;
        memset(ptr6->PlayerName.contents, 0x20, STRING_LENGTH);
        memcpy(ptr6->PlayerName.contents, username.c_str(), username.length());
        ptr6->X = X;
        ptr6->Y = Y;
        ptr6->Z = Z;
        ptr6->Yaw = Yaw;
        ptr6->Pitch = Pitch;

        packetsOutMutex.lock();
        packetsOut.push_back(Outgoing::createOutgoingPacket(ptr6.get()));
        packetsOutMutex.unlock();

        auto ptr7 = create_refptr<Outgoing::SpawnPlayer>();
        ptr7->PacketID = Outgoing::OutPacketTypes::eSpawnPlayer;
        ptr7->PlayerID = PlayerID;
        memset(ptr7->PlayerName.contents, 0x20, STRING_LENGTH);
        memcpy(ptr7->PlayerName.contents, username.c_str(), username.length());
        ptr7->X = X;
        ptr7->Y = Y;
        ptr7->Z = Z;
        ptr7->Yaw = Yaw;
        ptr7->Pitch = Pitch;

        server->broadcast_mutex.lock();
        server->broadcast_list.push_back(
            Outgoing::createOutgoingPacket(ptr7.get()));
        server->broadcast_mutex.unlock();

        auto ptr8 = create_refptr<Outgoing::Message>();
        ptr8->PacketID = Outgoing::OutPacketTypes::eMessage;
        ptr8->PlayerID = 0; // 0 = console
        memset(ptr8->Message.contents, 0x20, STRING_LENGTH);

        auto greeter2 = "&e" + username + " joined the game";
        std::cout << greeter2 << std::endl;
        memcpy(ptr8->Message.contents, greeter2.c_str(),
               greeter2.length() < STRING_LENGTH ? greeter2.length()
                                                 : STRING_LENGTH);

        server->broadcast_mutex.lock();
        server->broadcast_list.push_back(
            Outgoing::createOutgoingPacket(ptr8.get()));
        server->broadcast_mutex.unlock();

        server->client_mutex.lock();
        for (auto &[id, c] : server->clients)
        {
            if (id == PlayerID)
                continue;

            auto ptr9 = create_refptr<Outgoing::SpawnPlayer>();
            ptr9->PacketID = Outgoing::OutPacketTypes::eSpawnPlayer;
            ptr9->PlayerID = id;
            memset(ptr9->PlayerName.contents, 0x20, STRING_LENGTH);
            memcpy(ptr9->PlayerName.contents, c->username.c_str(),
                   c->username.length());
            ptr9->X = c->X;
            ptr9->Y = c->Y;
            ptr9->Z = c->Z;
            ptr9->Yaw = c->Yaw;
            ptr9->Pitch = c->Pitch;

            packetsOutMutex.lock();
            packetsOut.push_back(Outgoing::createOutgoingPacket(ptr9.get()));
            packetsOutMutex.unlock();
        }
        server->client_mutex.unlock();
    }

    void Client::update()
    {
        if (connected)
        {
            receive();

            for (auto p : packetsIn)
                process_packet(p);

            packetsIn.clear();

            send();
        }
    }

    auto get_len(Byte type) -> int
    {
        switch (static_cast<Incoming::InPacketTypes>(type))
        {
        case Incoming::InPacketTypes::ePlayerIdentification:
            return 131;
        case Incoming::InPacketTypes::eSetBlock:
            return 9;
        case Incoming::InPacketTypes::ePositionAndOrientation:
            return 10;
        case Incoming::InPacketTypes::eMessage:
            return 66;
        default:
            return -1;
        }
    }

    void Client::receive()
    {

        Byte newByte;
        int res = ::recv(socket, reinterpret_cast<char *>(&newByte), 1, MSG_PEEK);

        if (res <= 0)
            return;

        auto len = get_len(newByte);
        if (len < 0)
            return;

        auto byte_buffer = create_refptr<Network::ByteBuffer>(len);

        for (int i = 0; i < len; i++)
        {
            Byte b;
            ::recv(socket, reinterpret_cast<char *>(&b), 1, 0);

            byte_buffer->WriteU8(b);
        }

        packetsIn.push_back(byte_buffer);
    }

    void Client::send()
    {
        std::lock_guard lg(packetsOutMutex);

        for (auto &p : packetsOut)
        {

            int res =
                ::send(socket, p->m_Buffer, static_cast<int>(p->GetUsedSpace()), 0);

            if (res < 0)
            {
                SC_APP_ERROR("Client: Failed to send packets. Disconnecting.");
                connected = false;
                break;
            }
        }

        packetsOut.clear();
    }
} // namespace ClassicServer