#include "Client.hpp"
#include "IncomingPackets.hpp"
#include "OutgoingPackets.hpp"
#include "Server.hpp"
#include <algorithm>
#include <chrono>
#include <zlib.h>

namespace ClassicServer {

Client::Client(int socket, Server *server) {
    this->socket = socket;
    this->server = server;

    SC_APP_INFO("Client Created!");
    connected = true;

    packetsIn.clear();
    packetsOut.clear();

    client_update_thread = create_scopeptr<std::thread>(Client::update, this);

    X = 64 * 32;
    Y = 32 * 32 + 51;
    Z = 64 * 32;
    Yaw = 0;
    Pitch = 0;
}
Client::~Client() { client_update_thread->join(); }

void Client::process_packet(RefPtr<Network::ByteBuffer> packet) {
    auto packet_data = Incoming::readIncomingPacket(packet);
    auto id = packet_data->PacketID;
    switch (static_cast<Incoming::InPacketTypes>(id)) {

    case Incoming::InPacketTypes::ePlayerIdentification: {
        auto data = (Incoming::PlayerIdentification *)packet_data.get();
        data->Username.contents[STRING_LENGTH - 1] = 0;
        username = std::string((char *)data->Username.contents);
        username = username.substr(0, username.find_first_of(0x20));
        send_init();

        break;
    }
    case Incoming::InPacketTypes::eMessage: {
        auto ptr = create_refptr<Outgoing::Message>();
        ptr->PacketID = Outgoing::OutPacketTypes::eMessage;
        ptr->PlayerID = PlayerID;
        memset(ptr->Message.contents, 0x20, STRING_LENGTH);

        auto data = (Incoming::Message *)packet_data.get();
        data->Message.contents[STRING_LENGTH - 1] = 0;

        auto msg = std::string((char *)data->Message.contents);

        std::string message = username + ": " + msg;
        memcpy(ptr->Message.contents, message.c_str(), STRING_LENGTH);

        server->broadcast_packet(Outgoing::createOutgoingPacket(ptr.get()));
        break;
    }

    case Incoming::InPacketTypes::ePositionAndOrientation: {
        auto ptr = create_refptr<Outgoing::PlayerTeleport>();
        ptr->PacketID = Outgoing::OutPacketTypes::ePlayerTeleport;
        ptr->PlayerID = PlayerID;

        auto data = (Incoming::PositionAndOrientation *)packet_data.get();

        X = ptr->X = data->X;
        Y = ptr->Y = data->Y;
        Z = ptr->Z = data->Z;
        Yaw = ptr->Yaw = data->Yaw;
        Pitch = ptr->Pitch = data->Pitch;

        server->broadcast_mutex.lock();
        server->broadcast_list.push_back(
            Outgoing::createOutgoingPacket(ptr.get()));
        server->broadcast_mutex.unlock();
        break;
    }

    case Incoming::InPacketTypes::eSetBlock: {
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
            server->world->worldData[idx] = data->BlockType;

        ptr->BlockType = server->world->worldData[idx];

        server->broadcast_mutex.lock();
        server->broadcast_list.push_back(
            Outgoing::createOutgoingPacket(ptr.get()));
        server->broadcast_mutex.unlock();
        break;
    }
    }
}

void Client::send_init() {

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

    std::lock_guard lg(packetsOutMutex);
    packetsOut.push_back(Outgoing::createOutgoingPacket(ptr.get()));
    packetsOut.push_back(Outgoing::createOutgoingPacket(ptr2.get()));

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
    if (ret != Z_OK) {
        throw std::runtime_error("INVALID START!");
    }

    char *outBuffer = new char[256 * 64 * 256 + 4];

    strm.avail_out = 256 * 64 * 256 + 4;
    strm.next_out = (Bytef *)outBuffer;

    ret = deflate(&strm, Z_FINISH);

    switch (ret) {
    case Z_NEED_DICT:
    case Z_DATA_ERROR:
    case Z_MEM_ERROR:
        throw std::runtime_error("Zlib error: inflate()");
    }

    deflateEnd(&strm);
    int len = strm.total_out;

    size_t bytes = 0;
    while (bytes < len) {
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

        packetsOut.push_back(Outgoing::createOutgoingPacket(ptr3.get()));
    }

    delete[] outBuffer;

    auto ptr4 = create_refptr<Outgoing::LevelFinalize>();
    ptr4->PacketID = Outgoing::OutPacketTypes::eLevelFinalize;
    ptr4->XSize = 256;
    ptr4->YSize = 64;
    ptr4->ZSize = 256;

    packetsOut.push_back(Outgoing::createOutgoingPacket(ptr4.get()));

    auto ptr5 = create_refptr<Outgoing::Message>();
    ptr5->PacketID = Outgoing::OutPacketTypes::eMessage;
    ptr5->PlayerID = 0; // 0 = console
    memset(ptr5->Message.contents, 0x20, STRING_LENGTH);

    auto greeter = "&eWelcome to CrossCraft-Classic Server, " + username + "!";
    memcpy(ptr5->Message.contents, greeter.c_str(),
           greeter.length() < STRING_LENGTH ? greeter.length() : STRING_LENGTH);

    packetsOut.push_back(Outgoing::createOutgoingPacket(ptr5.get()));

    auto ptr6 = create_refptr<Outgoing::SpawnPlayer>();
    ptr6->PacketID = Outgoing::OutPacketTypes::eSpawnPlayer;
    ptr6->PlayerID = 255;
    memset(ptr6->PlayerName.contents, 0x20, STRING_LENGTH);
    memcpy(ptr6->PlayerName.contents, username.c_str(), username.length());
    ptr6->X = 64 * 32;
    ptr6->Y = 32 * 32 + 51;
    ptr6->Z = 64 * 32;
    ptr6->Yaw = 0;
    ptr6->Pitch = 0;

    packetsOut.push_back(Outgoing::createOutgoingPacket(ptr6.get()));

    auto ptr7 = create_refptr<Outgoing::SpawnPlayer>();
    ptr7->PacketID = Outgoing::OutPacketTypes::eSpawnPlayer;
    ptr7->PlayerID = PlayerID;
    memset(ptr7->PlayerName.contents, 0x20, STRING_LENGTH);
    memcpy(ptr7->PlayerName.contents, username.c_str(), username.length());
    ptr7->X = 64 * 32;
    ptr7->Y = 32 * 32 + 51;
    ptr7->Z = 64 * 32;
    ptr7->Yaw = 0;
    ptr7->Pitch = 0;

    server->broadcast_mutex.lock();
    server->broadcast_list.push_back(
        Outgoing::createOutgoingPacket(ptr7.get()));
    server->broadcast_mutex.unlock();

    auto ptr8 = create_refptr<Outgoing::Message>();
    ptr8->PacketID = Outgoing::OutPacketTypes::eMessage;
    ptr8->PlayerID = 0; // 0 = console
    memset(ptr8->Message.contents, 0x20, STRING_LENGTH);

    auto greeter2 = "&e" + username + " joined the game";
    memcpy(ptr8->Message.contents, greeter2.c_str(),
           greeter2.length() < STRING_LENGTH ? greeter2.length()
                                             : STRING_LENGTH);

    server->broadcast_mutex.lock();
    server->broadcast_list.push_back(
        Outgoing::createOutgoingPacket(ptr8.get()));
    server->broadcast_mutex.unlock();

    server->client_mutex.lock();
    for (auto &[id, c] : server->clients) {
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

        packetsOut.push_back(Outgoing::createOutgoingPacket(ptr9.get()));
    }
    server->client_mutex.unlock();
}

void Client::update(Client *client) {

    while (client->connected) {
        client->receive();

        for (auto p : client->packetsIn)
            client->process_packet(p);

        client->packetsIn.clear();

        client->send();

        std::this_thread::sleep_for(
            std::chrono::duration<double, std::milli>(20));
    }
}

auto get_len(Byte type) -> int {
    switch (static_cast<Incoming::InPacketTypes>(type)) {
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

void Client::receive() {

    Byte newByte;
    int res = ::recv(socket, reinterpret_cast<char *>(&newByte), 1, MSG_PEEK);

    if (res <= 0)
        return;

    auto len = get_len(newByte);
    if (len < 0)
        return;

    auto byte_buffer = create_refptr<Network::ByteBuffer>(len);

    for (int i = 0; i < len; i++) {
        Byte b;
        ::recv(socket, reinterpret_cast<char *>(&b), 1, 0);

        byte_buffer->WriteU8(b);
    }

    packetsIn.push_back(byte_buffer);
}

void Client::send() {
    std::lock_guard lg(packetsOutMutex);

    for (auto &p : packetsOut) {

        int res =
            ::send(socket, p->m_Buffer, static_cast<int>(p->GetUsedSpace()), 0);

        if (res < 0) {
            SC_APP_ERROR("Client: Failed to send packets. Disconnecting.");
            connected = false;
            break;
        }
    }

    packetsOut.clear();
}
} // namespace ClassicServer