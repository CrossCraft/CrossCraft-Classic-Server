#include "OutgoingPackets.hpp"

const size_t MAX_SIZE = 1028;

namespace ClassicServer::Outgoing {
auto createOutgoingPacket(BasePacket *base_packet)
    -> RefPtr<Network::ByteBuffer> {
    Byte id = base_packet->PacketID;

    switch (static_cast<OutPacketTypes>(id)) {

    case OutPacketTypes::eServerIdentification: {
        auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
        ptr->WriteU8(id);

        auto packet = reinterpret_cast<ServerIdentification *>(base_packet);

        VERIFY(ptr->WriteU8(packet->ProtocolVersion))
        VERIFY(ptr->WriteBuf(packet->ServerName.contents, STRING_LENGTH))
        VERIFY(ptr->WriteBuf(packet->MOTD.contents, STRING_LENGTH))
        VERIFY(ptr->WriteU8(packet->UserType))

        return ptr;
    }
    case OutPacketTypes::eLevelDataChunk: {
        auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
        ptr->WriteU8(id);

        auto packet = reinterpret_cast<LevelDataChunk *>(base_packet);

        VERIFY(ptr->WriteI16(packet->ChunkLength))
        VERIFY(ptr->WriteBuf(packet->ChunkData.contents, ARRAY_LENGTH))
        VERIFY(ptr->WriteU8(packet->PercentComplete))

        return ptr;
    }
    case OutPacketTypes::eLevelFinalize: {
        auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
        ptr->WriteU8(id);

        auto packet = reinterpret_cast<LevelFinalize *>(base_packet);

        VERIFY(ptr->WriteI16(packet->XSize))
        VERIFY(ptr->WriteI16(packet->YSize))
        VERIFY(ptr->WriteI16(packet->ZSize))

        return ptr;
    }
    case OutPacketTypes::eSetBlock: {
        auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
        ptr->WriteU8(id);

        auto packet = reinterpret_cast<SetBlock *>(base_packet);

        VERIFY(ptr->WriteI16(packet->X))
        VERIFY(ptr->WriteI16(packet->Y))
        VERIFY(ptr->WriteI16(packet->Z))
        VERIFY(ptr->WriteU8(packet->BlockType))

        return ptr;
    }
    case OutPacketTypes::eSpawnPlayer: {
        auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
        ptr->WriteU8(id);

        auto packet = reinterpret_cast<SpawnPlayer *>(base_packet);

        VERIFY(ptr->WriteI8(packet->PlayerID))
        VERIFY(ptr->WriteBuf(packet->PlayerName.contents, STRING_LENGTH))
        VERIFY(ptr->WriteI16(packet->X))
        VERIFY(ptr->WriteI16(packet->Y))
        VERIFY(ptr->WriteI16(packet->Z))
        VERIFY(ptr->WriteU8(packet->Yaw))
        VERIFY(ptr->WriteU8(packet->Pitch))

        return ptr;
    }
    case OutPacketTypes::ePlayerTeleport: {
        auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
        ptr->WriteU8(id);

        auto packet = reinterpret_cast<PlayerTeleport *>(base_packet);

        VERIFY(ptr->WriteI8(packet->PlayerID))
        VERIFY(ptr->WriteI16(packet->X))
        VERIFY(ptr->WriteI16(packet->Y))
        VERIFY(ptr->WriteI16(packet->Z))
        VERIFY(ptr->WriteU8(packet->Yaw))
        VERIFY(ptr->WriteU8(packet->Pitch))

        return ptr;
    }
    case OutPacketTypes::ePlayerUpdate: {
        auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
        ptr->WriteU8(id);

        auto packet = reinterpret_cast<PlayerUpdate *>(base_packet);

        VERIFY(ptr->WriteI8(packet->PlayerID))
        VERIFY(ptr->WriteI16(packet->X))
        VERIFY(ptr->WriteI16(packet->Y))
        VERIFY(ptr->WriteI16(packet->Z))
        VERIFY(ptr->WriteU8(packet->Yaw))
        VERIFY(ptr->WriteU8(packet->Pitch))

        return ptr;
    }
    case OutPacketTypes::ePositionUpdate: {
        auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
        ptr->WriteU8(id);

        auto packet = reinterpret_cast<PositionUpdate *>(base_packet);

        VERIFY(ptr->WriteI8(packet->PlayerID))
        VERIFY(ptr->WriteI8(packet->DX))
        VERIFY(ptr->WriteI8(packet->DY))
        VERIFY(ptr->WriteI8(packet->DZ))

        return ptr;
    }
    case OutPacketTypes::eOrientationUpdate: {
        auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
        ptr->WriteU8(id);

        auto packet = reinterpret_cast<OrientationUpdate *>(base_packet);

        VERIFY(ptr->WriteI8(packet->PlayerID))
        VERIFY(ptr->WriteU8(packet->Yaw))
        VERIFY(ptr->WriteU8(packet->Pitch))

        return ptr;
    }
    case OutPacketTypes::eDespawnPlayer: {
        auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
        ptr->WriteU8(id);

        auto packet = reinterpret_cast<DespawnPlayer *>(base_packet);

        VERIFY(ptr->WriteI8(packet->PlayerID))

        return ptr;
    }
    case OutPacketTypes::eMessage: {
        auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
        ptr->WriteU8(id);

        auto packet = reinterpret_cast<Message *>(base_packet);

        VERIFY(ptr->WriteI8(packet->PlayerID))
        VERIFY(ptr->WriteBuf(packet->Message.contents, STRING_LENGTH))

        return ptr;
    }
    case OutPacketTypes::eDisconnect: {
        auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
        ptr->WriteU8(id);

        auto packet = reinterpret_cast<Disconnect *>(base_packet);

        VERIFY(ptr->WriteBuf(packet->Reason.contents, STRING_LENGTH))

        return ptr;
    }

    case OutPacketTypes::eUpdateUserType: {
        auto ptr = create_refptr<Network::ByteBuffer>(MAX_SIZE);
        ptr->WriteU8(id);

        auto packet = reinterpret_cast<UpdateUserType *>(base_packet);

        VERIFY(ptr->WriteU8(packet->UserType))

        return ptr;
    }

    default: {
        auto ptr = create_refptr<Network::ByteBuffer>(1);

        VERIFY(ptr->WriteU8(id))

        return ptr;
    }
    }
}
} // namespace ClassicServer::Outgoing