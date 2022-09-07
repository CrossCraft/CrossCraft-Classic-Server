const std = @import("std");

/// All possible packets in Classic v0.30 (Protocol 7)
pub const Packet = enum(u8) { ServerIdentification = 0x00, Ping = 0x01, LevelInitialize = 0x02, LevelDataChunk = 0x03, LevelFinalize = 0x04, SetBlock = 0x06, SpawnPlayer = 0x07, PlayerTeleport = 0x08, PositionAndOrientation = 0x09, PositionUpdate = 0x0A, OrientationUpdate = 0x0B, DespawnPlayer = 0x0C, Message = 0x0D, Disconnect = 0x0E, UpdateUserType = 0x0F };

/// Get size of all packets
fn get_packet_size(ptype: Packet) usize {
    return switch (ptype) {
        Packet.ServerIdentification => 131,
        Packet.Ping => 1,
        Packet.LevelInitialize => 1,
        Packet.LevelDataChunk => 1028,
        Packet.LevelFinalize => 7,
        Packet.SetBlock => 8,
        Packet.SpawnPlayer => 74,
        Packet.PlayerTeleport => 10,
        Packet.PositionAndOrientation => 7,
        Packet.PositionUpdate => 5,
        Packet.OrientationUpdate => 4,
        Packet.DespawnPlayer => 2,
        Packet.Message => 66,
        Packet.Disconnect => 65,
        Packet.UpdateUserType => 2,
    };
}

/// Allocates and creates a buffer for a packet to be sent with
pub fn create_packet(allocator: *std.mem.Allocator, ptype: Packet) ![]u8 {
    var size = get_packet_size(ptype);
    var buf = try allocator.alloc(u8, size);

    var i: usize = 0;
    while (i < size) : (i += 1) {
        buf[i] = ' ';
    }

    return buf;
}

/// Protocol Version (7)
pub const PROTOCOL_VERSION: u8 = 0x07;

/// Creates a server identification packet
/// buf - Buffer to output to
/// server_name - Name of your server
/// server_motd - Message of the Day for your server
/// is_op - Is the user an OP?
pub fn make_server_identification(buf: []u8, server_name: []const u8, server_motd: []const u8, is_op: u8) !void {
    buf[0] = @enumToInt(Packet.ServerIdentification);
    buf[1] = PROTOCOL_VERSION;

    var fbstream = std.io.fixedBufferStream(buf[2..66]);
    try fbstream.writer().writeAll(if (server_name.len < 64) server_name else server_name[0..64]);

    fbstream = std.io.fixedBufferStream(buf[66..130]);
    try fbstream.writer().writeAll(if (server_motd.len < 64) server_motd else server_motd[0..64]);

    buf[130] = is_op;
}

/// Make Ping Packet
/// buf - Buffer to output
pub fn make_ping(buf: []u8) void {
    buf[0] = @enumToInt(Packet.Ping);
}

/// Make Level Initialization
/// buf - Buffer to output
pub fn make_level_initialize(buf: []u8) void {
    buf[0] = @enumToInt(Packet.LevelInitialize);
}