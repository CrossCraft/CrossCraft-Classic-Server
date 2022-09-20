const std = @import("std");
const network = @import("network");
const Self = @This();
const protocol = @import("protocol.zig");
const world = @import("world.zig");

/// Connection State
conn: network.Socket,
is_connected: bool,

/// Current packet processing buffer
packet_buffer: [131]u8,

/// Username of client
username: [16]u8,

// TODO: Track IP Data

kick_max: bool,
is_loaded: bool,
is_op: u8,
allocator: *std.mem.Allocator,
id: u8,
x: u16,
y: u16,
z: u16,
yaw: u8,
pitch: u8,

/// Async Frame Handler
handle_frame: @Frame(Self.handle),

/// Packet Incoming Types [C->S]
const PacketType = enum(u8) {
    PlayerIdentification = 0x00,
    SetBlock = 0x05,
    PositionAndOrientation = 0x08,
    Message = 0x0D,
};

/// Get Packet Size from ID
fn get_size(packetID: u8) usize {
    return switch (@intToEnum(PacketType, packetID)) {
        PacketType.PlayerIdentification => 131,
        PacketType.SetBlock => 9,
        PacketType.PositionAndOrientation => 10,
        PacketType.Message => 66,
    };
}

/// Get Packet Name from ID
/// TODO: Remove this in non-debug builds
fn get_name(packetID: u8) []const u8 {
    return switch (@intToEnum(PacketType, packetID)) {
        PacketType.PlayerIdentification => "Player Identification",
        PacketType.SetBlock => "Set Block",
        PacketType.PositionAndOrientation => "Position and Orientation",
        PacketType.Message => "Message",
    };
}

/// Read buffer from socket
fn readsock(self: *Self, buf: []u8) !void {
    const amt = try self.conn.receive(buf);

    if (amt == 0) {
        self.is_connected = false;
    }
}

/// Receive a packet
fn receive(self: *Self) !void {
    try self.readsock(self.packet_buffer[0..1]);
    var packet_size = get_size(self.packet_buffer[0]);
    try self.readsock(self.packet_buffer[1..packet_size]);
}

/// Copy the Username of the Player
fn copyUsername(self: *Self) void {
    var i: usize = 0;

    // ZERO out the Username
    while (i < 16) : (i += 1) {
        self.username[i] = 0;
    }
    i = 0;

    // Set username = packet username
    while (i < 16) : (i += 1) {
        var c = self.packet_buffer[2 + i];

        // Stop at space
        if (c != ' ') {
            self.username[i] = c;
        } else {
            break;
        }
    }
}

/// Send Packet from buffer
fn send(self: *Self, buf: []u8) !void {
    try self.conn.writer().writeAll(buf);
}

fn compress_level(compBuf: []u8) !usize {

}

fn send_level(self: *Self) !void {
    // Create Compression Buffer
    var compBuf : []u8 = try self.allocator.alloc(u8, world.size + 4);
    // Dealloc at end
    defer self.allocator.free(compBuf);

    // Compress and Get Length
    var len = compress_level(); 

    std.debug.print("World Size: {}\n", .{world.size});
    std.debug.print("Compressed Length: {}\n", .{len});

    // Send
    var bytes : usize = 0;
    while (bytes < len) {
        // Calculate remainingBytes and count
        var remainingBytes = len - bytes;
        var count : usize = if(remainingBytes > 1024) 1024 else remainingBytes;

        // Create Packet
        var buf = try protocol.create_packet(self.allocator, protocol.Packet.LevelDataChunk);
        // Destroy Packet
        defer self.allocator.free(buf);
        // Blank to 0
        @memset(buf.ptr, 0, 1028);

        // Create Buffer Stream
        var bstream = std.io.fixedBufferStream(buf);

        //PID
        try bstream.writer().writeIntBig(u8, @enumToInt(protocol.Packet.LevelDataChunk));
        //LENGTH (0-1024)
        try bstream.writer().writeIntBig(u16, @intCast(u16, count));
        //BUFFER COPY
        var startPos : usize = bytes;
        var endPos : usize = bytes + count;
        try bstream.writer().writeAll(compBuf[startPos..endPos]);

        bytes += count;

        //PERCENT
        buf[1027] = @intCast(u8, bytes / len * 100);

        //SEND
        try self.send(buf);
    }
}

/// Send initial packet spam
fn send_init(self: *Self) !void {

    // Send Server Identification
    var buf = try protocol.create_packet(self.allocator, protocol.Packet.ServerIdentification);
    try protocol.make_server_identification(buf, "CrossCraft", "Welcome!", self.is_op);
    try self.send(buf);
    self.allocator.free(buf);

    // Send Level Initialization
    buf = try protocol.create_packet(self.allocator, protocol.Packet.LevelInitialize);
    protocol.make_level_initialize(buf);
    try self.send(buf);
    self.allocator.free(buf);

    try self.send_level();

    // Send Level Finalization
    buf = try protocol.create_packet(self.allocator, protocol.Packet.LevelFinalize);
    try protocol.make_level_finalize(buf, 256, 64, 256);
    try self.send(buf);
    self.allocator.free(buf);

    // Send Welcome Message
    buf = try protocol.create_packet(self.allocator, protocol.Packet.Message);
    try protocol.make_message(buf, 0, "&eWelcome to the server!");
    try self.send(buf);
    self.allocator.free(buf);

    //Spawn Player
    buf = try protocol.create_packet(self.allocator, protocol.Packet.SpawnPlayer);
    try protocol.make_spawn_player(buf, self.id, self.username[0..], self.x, self.y, self.z, self.yaw, self.pitch);
    try self.send(buf);
    self.allocator.free(buf);

    // Send Update User Types
    buf = try protocol.create_packet(self.allocator, protocol.Packet.UpdateUserType);
    protocol.make_user_update(buf, self.is_op);
    try self.send(buf);
    self.allocator.free(buf);
}

fn disconnect(self: *Self, reason: []const u8) !void {
    var buf = try protocol.create_packet(self.allocator, protocol.Packet.Disconnect);
    try protocol.make_diconnect(buf, reason);
    try self.send(buf);
    self.allocator.free(buf);
    self.is_connected = false;
}

/// Process
fn process(self: *Self) !void {
    if (!self.is_connected)
        return;

    switch (@intToEnum(PacketType, self.packet_buffer[0])) {
        PacketType.PlayerIdentification => {
            if (self.packet_buffer[1] != 7) {
                std.debug.print("Error: Wrong protocol version! Is your client up to date? Terminating Connection.\n", .{});
                try self.disconnect("Invalid protocol version!");
                return;
            }

            self.copyUsername();

            // TODO: Check Ban List
            // TODO: Check IP Ban List
            // TODO: Check Duplicates

            if (self.kick_max) {
                std.debug.print("Client tried joining when server is full! Terminating Connection.\n", .{});
                try self.disconnect("Server is full!");
                return;
            }

            try self.send_init();
        },
        PacketType.SetBlock => {},
        PacketType.PositionAndOrientation => {},
        PacketType.Message => {},
    }
}

/// Handler thread
/// Runs through receive()
/// Then processes the taken packet
/// Then submits all things in packet queue
pub fn handle(self: *Self) !void {
    std.debug.print("Client connected!\n", .{});
    while (self.is_connected) {
        try self.receive();
        try self.process();
    }

    std.debug.print("Client connection closed!\n", .{});
}

/// Deinits the client
/// Client should NOT be used past this point
/// This method should be called by the dead client collector
pub fn deinit(self: *Self) void {
    // This is already probably set
    self.is_connected = false;

    //TODO: Make sure all packets in queue get sent

    // No more packets will be sent
    self.conn.close();
}
