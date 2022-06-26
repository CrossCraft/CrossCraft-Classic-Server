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

            VERIFY(byte_buffer->ReadU8(ptr->ProtocolVersion))
            VERIFY(byte_buffer->ReadBuf(ptr->Username.contents, STRING_LENGTH))
            VERIFY(byte_buffer->ReadBuf(ptr->VerificationKey.contents, STRING_LENGTH))
            VERIFY(byte_buffer->ReadU8(ptr->Unused))

            SC_APP_TRACE("Client: Player Identification");
            SC_APP_TRACE("Client: Protocol Version {x}", ptr->ProtocolVersion);
            SC_APP_TRACE("Client: Username {}", ptr->Username.contents);
            SC_APP_TRACE("Client: Verification Key {}", ptr->VerificationKey.contents);
            SC_APP_TRACE("Client: Unused {x}", ptr->Unused);

            return ptr;
        }

        case InPacketTypes::eSetBlock:
        {
            auto ptr = create_refptr<SetBlock>();
            ptr->PacketID = id;

            VERIFY(byte_buffer->ReadI16(ptr->X))
            VERIFY(byte_buffer->ReadI16(ptr->Y))
            VERIFY(byte_buffer->ReadI16(ptr->Z))
            VERIFY(byte_buffer->ReadU8(ptr->Mode))
            VERIFY(byte_buffer->ReadU8(ptr->BlockType))

            SC_APP_TRACE("Client: Set Block");
            SC_APP_TRACE("Client: Position {} {} {}", ptr->X, ptr->Y, ptr->Z);
            SC_APP_TRACE("Client: Mode {x}", ptr->Mode);
            SC_APP_TRACE("Client: Block {d}", ptr->BlockType);

            return ptr;
        }

        case InPacketTypes::ePositionAndOrientation:
        {
            auto ptr = create_refptr<PositionAndOrientation>();
            ptr->PacketID = id;

            VERIFY(byte_buffer->ReadU8(ptr->PlayerID))
            VERIFY(byte_buffer->ReadI16(ptr->X))
            VERIFY(byte_buffer->ReadI16(ptr->Y))
            VERIFY(byte_buffer->ReadI16(ptr->Z))
            VERIFY(byte_buffer->ReadU8(ptr->Yaw))
            VERIFY(byte_buffer->ReadU8(ptr->Pitch))

            SC_APP_TRACE("Client: Position And Orientation");
            SC_APP_TRACE("Client: Player ID {d}", ptr->PacketID);
            SC_APP_TRACE("Client: Position {f} {f} {f}", ptr->X, ptr->Y, ptr->Z);
            SC_APP_TRACE("Client: Orientation {d} {d}", ptr->Yaw, ptr->Pitch);

            return ptr;
        }

        case InPacketTypes::eMessage:
        {
            auto ptr = create_refptr<Message>();
            ptr->PacketID = id;

            VERIFY(byte_buffer->ReadU8(ptr->Unused))
            VERIFY(byte_buffer->ReadBuf(ptr->Message.contents, STRING_LENGTH))

            SC_APP_TRACE("Client: Message");
            SC_APP_TRACE("Client: Unused {x}", ptr->Unused);
            SC_APP_TRACE("Client: Message: {}", ptr->Message.contents);

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