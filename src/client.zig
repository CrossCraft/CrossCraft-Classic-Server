const std = @import("std");
const network = @import("network");
const Self = @This();
const protocol = @import("protocol.zig");
const world = @import("world.zig");
const server = @import("server.zig");

const zlib = @cImport({
    @cInclude("zlib.h");
});

/// Connection State
conn: network.Socket,
is_connected: bool,

/// Current packet processing buffer
packet_buffer: [131]u8,

/// Username of client
username: [16]u8,

// TODO: Track IP Data

is_loaded: bool,
is_op: u8,
allocator: *std.mem.Allocator,
id: u8,
x: u16,
y: u16,
z: u16,
yaw: u8,
pitch: u8,
send_lock: std.Thread.Mutex = undefined,

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
    if (packetID > 0xF)
        return 0;

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
    if (!self.is_connected)
        return;

    const amt = try self.conn.receive(buf);

    if (amt == 0)
        self.is_connected = false;
}

/// Receive a packet
fn receive(self: *Self) !void {
    if (self.is_connected) {
        _ = try self.conn.peek(self.packet_buffer[0..1]);

        var packet_size = get_size(self.packet_buffer[0]);
        if (packet_size == 0) {
            self.is_connected = false;
            return;
        }

        try self.readsock(self.packet_buffer[0..packet_size]);
    }
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
pub fn send(self: *Self, buf: []u8) void {
    if (!self.is_connected)
        return;

    self.send_lock.lock();
    defer self.send_lock.unlock();

    self.conn.writer().writeAll(buf) catch |err| switch (err){
        error.WouldBlock => send(self, buf),
        else => {self.is_connected = false;}
    };
}

fn compress_level(compBuf: []u8) !usize {
    var strm: zlib.z_stream = undefined;
    strm.zalloc = null;
    strm.zfree = null;
    strm.@"opaque" = null;

    var worldSize: u32 = @byteSwap(@intCast(u32, world.size));
    strm.avail_in = 4;
    strm.next_in = @ptrCast([*c]u8, &worldSize);

    strm.avail_out = 0;
    strm.next_out = zlib.Z_NULL;

    var ret = zlib.deflateInit2(&strm, zlib.Z_BEST_COMPRESSION, zlib.Z_DEFLATED, (zlib.MAX_WBITS + 16), 8, zlib.Z_DEFAULT_STRATEGY);
    if (ret != zlib.Z_OK)
        return error.ZLIB_INIT_FAIL;

    strm.avail_out = @intCast(c_uint, compBuf.len);
    strm.next_out = compBuf.ptr;

    ret = zlib.deflate(&strm, zlib.Z_NO_FLUSH);

    switch (ret) {
        zlib.Z_NEED_DICT => return error.ZLIB_NEED_DICT,
        zlib.Z_DATA_ERROR => return error.ZLIB_DATA_ERROR,
        zlib.Z_MEM_ERROR => return error.ZLIB_MEMORY_ERROR,
        else => {},
    }

    strm.avail_in = world.size;
    strm.next_in = world.worldData.ptr;
    ret = zlib.deflate(&strm, zlib.Z_FINISH);

    switch (ret) {
        zlib.Z_NEED_DICT => return error.ZLIB_NEED_DICT,
        zlib.Z_DATA_ERROR => return error.ZLIB_DATA_ERROR,
        zlib.Z_MEM_ERROR => return error.ZLIB_MEMORY_ERROR,
        else => {},
    }

    _ = zlib.deflateEnd(&strm);
    return strm.total_out;
}

fn send_level(self: *Self) !void {
    // Create Compression Buffer
    var compBuf: []u8 = try self.allocator.alloc(u8, world.size + 4);
    // Dealloc at end
    defer self.allocator.free(compBuf);

    // Compress and Get Length
    var len = try compress_level(compBuf);

    // Send
    var bytes: usize = 0;
    while (bytes < len) {
        // Calculate remainingBytes and count
        var remainingBytes = len - bytes;
        var count: usize = if (remainingBytes > 1024) 1024 else remainingBytes;

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
        var startPos: usize = bytes;
        var endPos: usize = bytes + count;
        try bstream.writer().writeAll(compBuf[startPos..endPos]);

        bytes += count;

        //PERCENT
        buf[1027] = @intCast(u8, (bytes * 100) / len);

        //SEND
        self.send(buf);

        //Delay Send
        std.time.sleep(1000 * 1000);
    }
}

fn join_message(self: *Self) !void {
    // Broadcast join message
    var buf2 = try protocol.create_packet(self.allocator, protocol.Packet.Message);
    var msg: [64]u8 = undefined;
    for(msg) |*v| {
        v.* = 0x20;
    }

    var pos: usize = 2;
    msg[0] = '&';
    msg[1] = 'e';

    while((pos-2) < 16 and self.username[pos-2] != 0) : (pos += 1){
        msg[pos] = self.username[pos-2];
    }

    var msg2 = " joined the game!";
    var pos_start = pos;

    while(pos < 64 and pos - pos_start < msg2.len): (pos += 1) {
        msg[pos] = msg2[pos - pos_start];
    }

    try protocol.make_message(buf2, 0, msg[0..]);

    var b_info = server.BroadcastInfo {
        .buf = buf2,
        .exclude_id = 0,
    };

    try server.request_broadcast(b_info);
}

/// Send initial packet spam
fn send_init(self: *Self) !void {
    self.x = 128 * 32;
    self.y = 32 * 32;
    self.z = 128 * 32;

    // Send Server Identification
    var buf = try protocol.create_packet(self.allocator, protocol.Packet.ServerIdentification);
    try protocol.make_server_identification(buf, "CrossCraft", "Welcome!", self.is_op);
    self.send(buf);
    self.allocator.free(buf);

    // Send Level Initialization
    buf = try protocol.create_packet(self.allocator, protocol.Packet.LevelInitialize);
    protocol.make_level_initialize(buf);
    self.send(buf);
    self.allocator.free(buf);

    // Send Level Itself
    try self.send_level();

    // Send Level Finalization
    buf = try protocol.create_packet(self.allocator, protocol.Packet.LevelFinalize);
    try protocol.make_level_finalize(buf, 256, 64, 256);
    self.send(buf);
    self.allocator.free(buf);

    // Send Welcome Message
    buf = try protocol.create_packet(self.allocator, protocol.Packet.Message);
    try protocol.make_message(buf, 0, "&eWelcome to the server!");
    self.send(buf);
    self.allocator.free(buf);

    // Send Join Message
    try self.join_message();

    //Spawn Player
    var buf2 = try protocol.create_packet(self.allocator, protocol.Packet.SpawnPlayer);
    try protocol.make_spawn_player(buf2, self.id, self.username[0..], self.x, self.y, self.z, self.yaw, self.pitch);
    var b_info = server.BroadcastInfo {
        .buf = buf2,
        .exclude_id = 0,
    };
    try server.request_broadcast(b_info);

    // Spawn all other players
    try server.new_player_spawn(self);

    //Teleport Player
    buf = try protocol.create_packet(self.allocator, protocol.Packet.PlayerTeleport);
    try protocol.make_teleport_player(buf, 255, self.x, self.y, self.z, self.yaw, self.pitch);
    self.send(buf);
    self.allocator.free(buf);

    // Send Update User Types
    buf = try protocol.create_packet(self.allocator, protocol.Packet.UpdateUserType);
    protocol.make_user_update(buf, self.is_op);
    self.send(buf);
    self.allocator.free(buf);
}

fn disconnect(self: *Self, reason: []const u8) !void {
    var buf = try protocol.create_packet(self.allocator, protocol.Packet.Disconnect);
    try protocol.make_disconnect(buf, reason);
    self.send(buf);
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

            try self.send_init();
        },
        PacketType.SetBlock => {
            var fbstream = std.io.fixedBufferStream(self.packet_buffer[1..]);
            var reader = fbstream.reader();

            var x: u16 = try reader.readIntBig(u16);
            var y: u16 = try reader.readIntBig(u16);
            var z: u16 = try reader.readIntBig(u16);

            var mode: u8 = try reader.readIntBig(u8);
            var btype: u8 = try reader.readIntBig(u8);

            var idx: usize = (@intCast(usize, y) * 256 * 256) + (@intCast(usize, z) * 256) + @intCast(usize, x);

            if (mode == 0x00) {
                //Destroy
                world.worldData[idx] = 0;
                _ = btype;
            } else {
                //Create
                world.worldData[idx] = btype;
            }

            var buf = try protocol.create_packet(self.allocator, protocol.Packet.SetBlock);
            try protocol.make_set_block(buf, x, y, z, world.worldData[idx]);
            var b_info = server.BroadcastInfo {
                .buf = buf,
                .exclude_id = 0,
            };
            try server.request_broadcast(b_info);
        },
        PacketType.PositionAndOrientation => {
            var fbstream = std.io.fixedBufferStream(self.packet_buffer[2..]);
            var reader = fbstream.reader();

            self.x = try reader.readIntBig(u16);
            self.y = try reader.readIntBig(u16);
            self.z = try reader.readIntBig(u16);
            self.yaw = try reader.readIntBig(u8);
            self.pitch = try reader.readIntBig(u8);

            var buf = try protocol.create_packet(self.allocator, protocol.Packet.PlayerTeleport);
            try protocol.make_teleport_player(buf, self.id, self.x, self.y, self.z, self.yaw, self.pitch);
            var b_info = server.BroadcastInfo {
                .buf = buf,
                .exclude_id = self.id,
            };
            try server.request_broadcast(b_info);
        },
        PacketType.Message => {
            var fbstream = std.io.fixedBufferStream(self.packet_buffer[2..]);
            var reader = fbstream.reader();

            var msg: [64]u8 = undefined;
            _ = try reader.readAll(msg[0..]);

            var msg2: [64]u8 = undefined;
            
            //Copy username 
            var pos : usize = 0;
            while(pos < 16 and self.username[pos] != 0): (pos += 1) {
                msg2[pos] = self.username[pos];
            }
            msg2[pos] = ':';
            pos += 1;

            msg2[pos] = ' ';
            pos += 1;

            var init_pos = pos;
            while(pos < 64) : (pos += 1){
                msg2[pos] = msg[pos - init_pos];
            }

            var buf = try protocol.create_packet(self.allocator, protocol.Packet.Message);
            try protocol.make_message(buf, @bitCast(i8, self.id), msg2[0..]);

            var b_info = server.BroadcastInfo {
                .buf = buf,
                .exclude_id = 0,
            };
            try server.request_broadcast(b_info);
        },
    }
}

/// Handler thread
/// Runs through receive()
/// Then processes the taken packet
/// Then submits all things in packet queue
pub fn handle(self: *Self) !void {
    std.debug.print("Client connected!\n", .{});
    while (self.is_connected) {
        std.time.sleep(50 * 1000 * 1000);
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

    // No more packets will be sent
    self.conn.close();
}
