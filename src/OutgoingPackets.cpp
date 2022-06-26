#include "OutgoingPackets.hpp"

const size_t MAX_SIZE = 1028;

namespace ClassicServer::Outgoing
{
    auto createOutgoingPacket(RefPtr<BasePacket> base_packet) -> RefPtr<Network::ByteBuffer>
    {
        Byte id = base_packet->PacketID;

        switch (static_cast<OutPacketTypes>(id))
        {

        case OutPacketTypes::eServerIdentification: {
            auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
            ptr->WriteU8(id);

            auto packet = std::dynamic_pointer_cast<ServerIdentification>(base_packet);

            ptr->WriteU8(packet->ProtocolVersion);
            ptr->WriteBuf(packet->ServerName.contents, STRING_LENGTH);
            ptr->WriteBuf(packet->MOTD.contents, STRING_LENGTH);
            ptr->WriteU8(packet->UserType);

            return ptr;
        }
        case OutPacketTypes::eLevelDataChunk: {
            auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
            ptr->WriteU8(id);

            auto packet = std::dynamic_pointer_cast<LevelDataChunk>(base_packet);

            ptr->WriteI16(packet->ChunkLength);
            ptr->WriteBuf(packet->ChunkData.contents, ARRAY_LENGTH);
            ptr->WriteU8(packet->PercentComplete);

            return ptr;
        }
        case OutPacketTypes::eLevelFinalize: {
            auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
            ptr->WriteU8(id);

            auto packet = std::dynamic_pointer_cast<LevelFinalize>(base_packet);

            ptr->WriteI16(packet->XSize);
            ptr->WriteI16(packet->YSize);
            ptr->WriteI16(packet->ZSize);

            return ptr;
        }
        case OutPacketTypes::eSetBlock: {
            auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
            ptr->WriteU8(id);

            auto packet = std::dynamic_pointer_cast<SetBlock>(base_packet);

            ptr->WriteI16(packet->X);
            ptr->WriteI16(packet->Y);
            ptr->WriteI16(packet->Z);
            ptr->WriteU8(packet->BlockType);

            return ptr;
        }
        case OutPacketTypes::eSpawnPlayer: {
            auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
            ptr->WriteU8(id);

            auto packet = std::dynamic_pointer_cast<SpawnPlayer>(base_packet);

            ptr->WriteI8(packet->PlayerID);
            ptr->WriteBuf(packet->PlayerName.contents, STRING_LENGTH);
            ptr->WriteI16(packet->X);
            ptr->WriteI16(packet->Y);
            ptr->WriteI16(packet->Z);
            ptr->WriteU8(packet->Yaw);
            ptr->WriteU8(packet->Pitch);

            return ptr;
        }
        case OutPacketTypes::ePlayerTeleport: {
            auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
            ptr->WriteU8(id);

            auto packet = std::dynamic_pointer_cast<PlayerTeleport>(base_packet);

            ptr->WriteI8(packet->PlayerID);
            ptr->WriteI16(packet->X);
            ptr->WriteI16(packet->Y);
            ptr->WriteI16(packet->Z);
            ptr->WriteU8(packet->Yaw);
            ptr->WriteU8(packet->Pitch);

            return ptr;
        }
        case OutPacketTypes::ePlayerUpdate: {
            auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
            ptr->WriteU8(id);

            auto packet = std::dynamic_pointer_cast<PlayerUpdate>(base_packet);

            ptr->WriteI8(packet->PlayerID);
            ptr->WriteI16(packet->X);
            ptr->WriteI16(packet->Y);
            ptr->WriteI16(packet->Z);
            ptr->WriteU8(packet->Yaw);
            ptr->WriteU8(packet->Pitch);

            return ptr;
        }
        case OutPacketTypes::ePositionUpdate: {
            auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
            ptr->WriteU8(id);

            auto packet = std::dynamic_pointer_cast<PositionUpdate>(base_packet);

            ptr->WriteI8(packet->PlayerID);
            ptr->WriteI8(packet->DX);
            ptr->WriteI8(packet->DY);
            ptr->WriteI8(packet->DZ);

            return ptr;
        }
        case OutPacketTypes::eOrientationUpdate: {
            auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
            ptr->WriteU8(id);

            auto packet = std::dynamic_pointer_cast<OrientationUpdate>(base_packet);

            ptr->WriteI8(packet->PlayerID);
            ptr->WriteU8(packet->Yaw);
            ptr->WriteU8(packet->Pitch);

            return ptr;
        }
        case OutPacketTypes::eDespawnPlayer: {
            auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
            ptr->WriteU8(id);

            auto packet = std::dynamic_pointer_cast<DespawnPlayer>(base_packet);

            ptr->WriteI8(packet->PlayerID);

            return ptr;
        }
        case OutPacketTypes::eMessage: {
            auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
            ptr->WriteU8(id);

            auto packet = std::dynamic_pointer_cast<Message>(base_packet);

            ptr->WriteI8(packet->PlayerID);
            ptr->WriteBuf(packet->Message.contents, STRING_LENGTH);

            return ptr;
        }
        case OutPacketTypes::eDisconnect: {
            auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
            ptr->WriteU8(id);

            auto packet = std::dynamic_pointer_cast<Disconnect>(base_packet);
            ptr->WriteBuf(packet->Reason.contents, STRING_LENGTH);

            return ptr;
        }

        case OutPacketTypes::eUpdateUserType: {
            auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
            ptr->WriteU8(id);

            auto packet = std::dynamic_pointer_cast<UpdateUserType>(base_packet);
            ptr->WriteU8(packet->UserType);

            return ptr;
        }

        default:
        {
            auto ptr = create_refptr<Network::ByteBuffer>(1);
            ptr->WriteU8(id);
            return ptr;
        }
        }
    }
}