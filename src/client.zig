const std = @import("std");
const network = @import("network");
const Self = @This();

/// Connection State
conn: network.Socket,
is_connected: bool,

/// Current packet processing buffer
packet_buffer: [131]u8,

/// Username of client
username: [16]u8,
// TODO: Track IP Data
// TODO: Track Player Position Data
// TODO: Track Player ID number

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

/// Process 
fn process(self: *Self) !void {
    switch (@intToEnum(PacketType, self.packet_buffer[0])) {
        PacketType.PlayerIdentification => {
            if (self.packet_buffer[1] != 7) {
                std.debug.print("Error: Wrong protocol version! Is your client up to date? Terminating Connection.\n", .{});

                // TODO: Send disconnect

                self.is_connected = false;
                return;
            }

            self.copyUsername();

            // TODO: Check Ban List
            // TODO: Check IP Ban List
            // TODO: Check not currently connected
            // TODO: Check server not full
            // TODO: Send initial response and world data

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