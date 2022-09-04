const std = @import("std");
const network = @import("network");
const Client = @import("client.zig");

/// Fixed Buffer Allocation
var ram_buffer: [16 * 1000 * 1000]u8 = undefined;
var allocator: std.mem.Allocator = undefined;
var fba : std.heap.FixedBufferAllocator = undefined;

/// Server Socket
var socket: network.Socket = undefined;

/// Initialize Server
pub fn init() !void {
    // Initialize Network
    try network.init();

    // Setup Fixed Buffer Allocator
    fba = std.heap.FixedBufferAllocator.init(&ram_buffer);
    allocator = fba.allocator();

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
        std.os.nanosleep(0, 50 * 1000 * 1000);

        // TODO: Update World
        // TODO: Update All Clients (broadcast packets, etc.)

        // Check Client
        var conn = socket.accept() catch |err| switch (err) {
            error.WouldBlock => continue,
            else => return,
        };

        std.debug.print("New Client @ {}\n", .{try conn.getRemoteEndPoint()});

        // New Client Thread
        // TODO: Assign ID number OR kick for full server
        // TODO: Add to Client* list
        const client = try allocator.create(Client);
        client.* = Client{
            .conn = conn,
            .is_connected = true,
            .handle_frame = async client.handle(),
            .packet_buffer = undefined,
            .username = undefined,
        };
    }
}

/// Cleanup Server
pub fn deinit() void {
    socket.close();
    network.deinit();
}
