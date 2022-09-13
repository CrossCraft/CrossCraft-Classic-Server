const std = @import("std");
const network = @import("network");
const Client = @import("client.zig");
const world = @import("world.zig");

/// Fixed Buffer Allocation
var ram_buffer: [16 * 1000 * 1000]u8 = undefined;
var allocator: std.mem.Allocator = undefined;
var fba: std.heap.FixedBufferAllocator = undefined;

/// Server Socket
var socket: network.Socket = undefined;

/// Initialize Server
pub fn init() !void {

    // Initialize Network
    try network.init();

    // Setup Fixed Buffer Allocator
    fba = std.heap.FixedBufferAllocator.init(&ram_buffer);
    allocator = fba.allocator();

    try world.init(&allocator);

    // Create brand new socket handle
    socket = try network.Socket.create(.ipv4, .tcp);

    // Bind socket to port
    try socket.bind(.{
        .address = .{ .ipv4 = network.Address.IPv4.any },
        .port = 25565,
    });

    // Listen for connections
    try socket.listen();
    std.debug.print("Listening for connections...\n", .{});
}

/// Run Server
pub fn run() !void {
    while (true) {
        // Sleep 50ms (20 TPS)
        std.time.sleep(50 * 1000 * 1000);

        // TODO: Update World
        // TODO: Update All Clients (broadcast packets, etc.)

        // Check Client
        var conn = socket.accept() catch |err| switch (err) {
            error.WouldBlock => continue,
            else => return err,
        };

        std.debug.print("New Client @ {}\n", .{try conn.getRemoteEndPoint()});

        // New Client Thread
        // TODO: Assign ID number OR kick for full server
        // TODO: Add to Client* list
        const client = try allocator.create(Client);
        client.* = Client{
            .conn = conn,
            .allocator = &allocator,
            .is_connected = true,
            .handle_frame = async client.handle(),
            .packet_buffer = undefined,
            .username = undefined,
            .is_loaded = false,
            .kick_max = false,
            .is_op = 0,
            .yaw = 0,
            .pitch = 0,
            .x = 0,
            .y = 0,
            .z = 0,
            .id = 1,
        };
    }
}

/// Cleanup Server
pub fn deinit() void {
    socket.close();
    network.deinit();
    world.deinit();
}
