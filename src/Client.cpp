#include "Client.hpp"
#include "IncomingPackets.hpp"
#include "OutgoingPackets.hpp"
#include <chrono>
#include <zlib.h>
#include <algorithm>

namespace ClassicServer
{
    uint8_t sample_map[128 * 64 * 128 + 4];


    inline uint32_t HostToNetwork4(const void* a_Value) {
        uint32_t buf;
        memcpy(&buf, a_Value, sizeof(buf));
        buf = ntohl(buf);
        return buf;
    }

    Client::Client(int socket) {
        this->socket = socket;

        SC_APP_INFO("Client Created!");
        connected = true;

        packetsIn.clear();
        packetsOut.clear();

        client_update_thread = create_scopeptr<std::thread>(Client::update, this);

        memset(sample_map, 0, 128 * 64 * 128 + 4);

        for(int x = 0; x < 128; x++)
            for(int y = 0; y < 2; y++)
                for (int z = 0; z < 128; z++) {
                    auto idx = (y * 128 * 128) + (z * 128) + x + 4;

                    if (y == 0)
                        sample_map[idx] = 7;
                    else
                        sample_map[idx] = 1;
                }

        uint32_t size = 128 * 64 * 128;
        size = HostToNetwork4(&size);

        memcpy(sample_map, &size, 4);
    }
    Client::~Client() {}

    void Client::process_packet(RefPtr<Network::ByteBuffer> packet) {
        auto packet_data = Incoming::readIncomingPacket(packet);

        if (packet_data->PacketID == Incoming::InPacketTypes::ePlayerIdentification) {

            auto data = (Incoming::PlayerIdentification*)packet_data.get();
            data->Username.contents[STRING_LENGTH - 1] = 0;
            username = std::string((char*)data->Username.contents);
            username = username.substr(0, username.find_first_of(0x20));
            send_init();
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
        strm.avail_in = 128 * 64 * 128 + 4;
        strm.next_in = (Bytef*)sample_map;
        strm.avail_out = 0;
        strm.next_out = Z_NULL;

        int ret = deflateInit2(&strm, Z_BEST_COMPRESSION, Z_DEFLATED, (MAX_WBITS + 16), 8, Z_DEFAULT_STRATEGY);
        if (ret != Z_OK) {
            throw std::runtime_error("INVALID START!");
        }

        char* outBuffer = new char[128 * 64 * 128 + 4];

        strm.avail_out = 128 * 64 * 128 + 4;
        strm.next_out = (Bytef*)outBuffer;

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
            memcpy(ptr3->ChunkData.contents, (void*)&outBuffer[bytes], count);

            bytes += count;

            char percent = (char)(((float)bytes / (float)len) * 100.0f);
            ptr3->PercentComplete = percent;
            
            packetsOut.push_back(Outgoing::createOutgoingPacket(ptr3.get()));
        }

        auto ptr4 = create_refptr<Outgoing::LevelFinalize>();
        ptr4->PacketID = Outgoing::OutPacketTypes::eLevelFinalize;
        ptr4->XSize = 128;
        ptr4->YSize = 64;
        ptr4->ZSize = 128;

        packetsOut.push_back(Outgoing::createOutgoingPacket(ptr4.get()));


        auto ptr5 = create_refptr<Outgoing::Message>();
        ptr5->PacketID = Outgoing::OutPacketTypes::eMessage;
        ptr5->PlayerID = 0; //0 = console
        memset(ptr5->Message.contents, 0x20, STRING_LENGTH);

        auto greeter = "&eWelcome to CrossCraft-Classic Server, " + username + "!";
        memcpy(ptr5->Message.contents, greeter.c_str(), greeter.length());

        packetsOut.push_back(Outgoing::createOutgoingPacket(ptr5.get()));



        auto ptr6 = create_refptr<Outgoing::SpawnPlayer>();
        ptr6->PacketID = Outgoing::OutPacketTypes::eSpawnPlayer;
        ptr6->PlayerID = 255;
        memset(ptr6->PlayerName.contents, 0x20, STRING_LENGTH);
        memcpy(ptr6->PlayerName.contents, username.c_str(), username.length() < STRING_LENGTH ? username.length() : STRING_LENGTH);
        ptr6->X = 64 * 32;
        ptr6->Y = 32 * 32 + 51;
        ptr6->Z = 64 * 32;
        ptr6->Yaw = 0;
        ptr6->Pitch = 0;
        
        packetsOut.push_back(Outgoing::createOutgoingPacket(ptr6.get()));
    }

    void Client::update(Client* client) {
    
        while (client->connected) {
            client->receive();

            for (auto p : client->packetsIn)
                client->process_packet(p);
            
            client->packetsIn.clear();

            client->send();

            std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(50));
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
        int res =
            ::recv(socket, reinterpret_cast<char*>(&newByte), 1, MSG_PEEK);

        if (res <= 0)
            return;

        auto len = get_len(newByte);
        if (len < 0)
            return;

        auto byte_buffer = create_refptr<Network::ByteBuffer>(len);

        for (int i = 0; i < len; i++) {
            Byte b;
            ::recv(socket, reinterpret_cast<char*>(&b), 1, 0);

            byte_buffer->WriteU8(b);
        }

        packetsIn.push_back(byte_buffer);
    }

    void Client::send() {
        std::lock_guard lg(packetsOutMutex);
        
        for (auto& p : packetsOut) {
            
            int res = ::send(socket, p->m_Buffer,
                static_cast<int>(p->GetUsedSpace()), 0);

            if (res < 0) {
                SC_APP_ERROR("Client: Failed to send packets. Disconnecting.");
                connected = false;
                break;
            }
        }

        packetsOut.clear();
    }
}