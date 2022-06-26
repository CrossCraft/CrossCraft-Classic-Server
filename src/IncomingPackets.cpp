#include "IncomingPackets.hpp"

namespace ClassicServer::Incoming
{

    auto readIncomingPacket(RefPtr<Network::ByteBuffer> byte_buffer) -> RefPtr<BasePacket>
    {
        byte_buffer->ResetRead();

        Byte id = -1;
        byte_buffer->ReadU8(id);

        switch (static_cast<InPacketTypes>(id))
        {
        case InPacketTypes::ePlayerIdentification:
        {
            auto ptr = create_refptr<PlayerIdentification>();
            ptr->PacketID = id;

            byte_buffer->ReadU8(ptr->ProtocolVersion);
            byte_buffer->ReadBuf(ptr->Username.contents, STRING_LENGTH);
            byte_buffer->ReadBuf(ptr->VerificationKey.contents, STRING_LENGTH);
            byte_buffer->ReadU8(ptr->Unused);
            
            return ptr;
        }

        case InPacketTypes::eSetBlock:
        {
            auto ptr = create_refptr<SetBlock>();
            ptr->PacketID = id;

            byte_buffer->ReadI16(ptr->X);
            byte_buffer->ReadI16(ptr->Y);
            byte_buffer->ReadI16(ptr->Z);

            byte_buffer->ReadU8(ptr->Mode);
            byte_buffer->ReadU8(ptr->BlockType);

            return ptr;
        }

        case InPacketTypes::ePositionAndOrientation:
        {
            auto ptr = create_refptr<PositionAndOrientation>();
            ptr->PacketID = id;

            byte_buffer->ReadU8(ptr->PlayerID);

            byte_buffer->ReadI16(ptr->X);
            byte_buffer->ReadI16(ptr->Y);
            byte_buffer->ReadI16(ptr->Z);

            byte_buffer->ReadU8(ptr->Yaw);
            byte_buffer->ReadU8(ptr->Pitch);

            return ptr;
        }

        case InPacketTypes::eMessage:
        {
            auto ptr = create_refptr<Message>();
            ptr->PacketID = id;

            byte_buffer->ReadU8(ptr->Unused);
            byte_buffer->ReadBuf(ptr->Message.contents, STRING_LENGTH);

            return ptr;
        }

        default:
        {
            SC_APP_ERROR("INVALID PACKET! ID {}", id);
            return nullptr;
        }

        }
    }
}