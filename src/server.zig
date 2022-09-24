const std = @import("std");
const network = @import("network");
const Client = @import("client.zig");
const world = @import("world.zig");
const protocol = @import("protocol.zig");
const users = @import("users.zig");

/// Fixed Buffer Allocation
var ram_buffer: [16 * 1000 * 1000]u8 = undefined;
var allocator: std.mem.Allocator = undefined;
var fba: std.heap.FixedBufferAllocator = undefined;

/// Packet Queue
var packet_queue: std.ArrayList(BroadcastInfo) = undefined;
var packet_queue_lock: std.Thread.Mutex = undefined;

/// Server Socket
var socket: network.Socket = undefined;
var ticks_alive: usize = 0;

// ID 0 is Reserved for Console
var client_list: [128]?*Client = undefined;

/// Initialize Server
pub fn init() !void {

    // Initialize Network
    try network.init();

    // Setup Fixed Buffer Allocator
    fba = std.heap.FixedBufferAllocator.init(&ram_buffer);
    allocator = fba.allocator();

    // Init world
    try world.init(&allocator);

    // Init client list
    var i: usize = 0;
    while (i < 128) : (i += 1) {
        client_list[i] = null;
    }

    // Init users list
    try users.init(allocator);

    // Init packet list
    packet_queue = std.ArrayList(BroadcastInfo).init(allocator);

    // Create brand new socket handle
    socket = try network.Socket.create(.ipv4, .tcp);
    try socket.enablePortReuse(true);

    // Bind socket to port
    try socket.bind(.{
        .address = .{ .ipv4 = network.Address.IPv4.any },
        .port = 25565,
    });

    // Listen for connections
    try socket.listen();
    std.debug.print("Listening for connections...\n", .{});
}

/// Broadcast information
/// buf - The packet buffer to send
/// exclude_id - Excludes client with ID -- 0 otherwise
pub const BroadcastInfo = struct { buf: []u8, exclude_id: u8 };

/// Request Broadcast
/// Adds packet to the queue to broadcast to
pub fn request_broadcast(info: BroadcastInfo) !void {
    packet_queue_lock.lock();
    defer packet_queue_lock.unlock();
    try packet_queue.append(info);
}

/// Broadcasts all packets
fn broadcast_all() void {
    packet_queue_lock.lock();
    defer packet_queue_lock.unlock();

    for (packet_queue.items) |b_info| {
        var i: usize = 1;
        while (i < 128) : (i += 1) {
            if (client_list[i] != null and i != b_info.exclude_id) {
                var client = client_list[i].?;
                client.send(b_info.buf);
            }
        }
        allocator.free(b_info.buf);
    }

    packet_queue.clearAndFree();
}

/// Gets an ID if available -- returns -1 otherwise
fn get_available_ID() i8 {
    var i: usize = 1;

    while (i < 128) : (i += 1) {
        if (client_list[i] == null)
            return @intCast(i8, i);
    }

    return -1;
}

fn ping_all() !void {
    var buf = try protocol.create_packet(&allocator, protocol.Packet.Ping);
    protocol.make_ping(buf);
    defer allocator.free(buf);

    var i: usize = 1;
    while (i < 128) : (i += 1) {
        if (client_list[i] != null) {
            var client = client_list[i].?;
            client.send(buf);
        }
    }
}

pub fn new_player_spawn(client: *Client) !void {
    var i: usize = 1;
    while (i < 128) : (i += 1) {
        if (client_list[i] != null and client_list[i].?.id != client.id) {
            var client2 = client_list[i].?;

            var buf = try protocol.create_packet(&allocator, protocol.Packet.SpawnPlayer);
            try protocol.make_spawn_player(buf, client2.id, client2.username[0..], client2.x, client2.y, client2.z, client2.yaw, client2.pitch);
            client.send(buf);
            allocator.free(buf);
        }
    }
}

fn broadcast_client_kill(client: *Client) !void {
    var buf = try protocol.create_packet(&allocator, protocol.Packet.DespawnPlayer);
    protocol.make_despawn_player(buf, client.id);
    var b_info = BroadcastInfo{
        .buf = buf,
        .exclude_id = client.id,
    };
    try request_broadcast(b_info);

    var buf2 = try protocol.create_packet(&allocator, protocol.Packet.Message);
    var msg: [64]u8 = undefined;
    for (msg) |*v| {
        v.* = 0x20;
    }

    var pos: usize = 2;
    msg[0] = '&';
    msg[1] = 'e';

    while ((pos - 2) < 16 and client.username[pos - 2] != 0) : (pos += 1) {
        msg[pos] = client.username[pos - 2];
    }

    var msg2 = " left the game!";
    var pos_start = pos;

    while (pos < 64 and pos - pos_start < msg2.len) : (pos += 1) {
        msg[pos] = msg2[pos - pos_start];
    }

    try protocol.make_message(buf2, 0, msg[0..]);

    var b_info2 = BroadcastInfo{
        .buf = buf2,
        .exclude_id = client.id,
    };
    try request_broadcast(b_info2);
}

fn gc_dead_clients() !void {
    var i: usize = 1;
    var kills: usize = 0;

    while (i < 128) : (i += 1) {
        if (client_list[i] != null) {
            if (!client_list[i].?.is_connected) {
                var client = client_list[i].?;
                try broadcast_client_kill(client);

                client.deinit();
                allocator.destroy(client);
                client_list[i] = null;

                kills += 1;
            }
        }
    }

    if (kills > 0) {
        std.debug.print("Killed {} dead connections.\n", .{kills});
    }
}

/// Run Server
pub fn run() !void {
    while (true) {
        // Sleep 50ms (20 TPS)
        std.time.sleep(50 * 1000 * 1000);

        world.update();
        ticks_alive += 1;

        // PING ALL CLIENTS
        if (ticks_alive % 600 == 0) {
            try ping_all();

            // Garbage Collect Dead Clients
            try gc_dead_clients();
        }

        // Broadcast all events
        broadcast_all();

        // Check Client
        var conn = socket.accept() catch |err| switch (err) {
            error.WouldBlock => continue,
            else => return err,
        };

        std.debug.print("New Client @ {}\n", .{try conn.getRemoteEndPoint()});

        // New Client Thread
        var id: i8 = get_available_ID();

        if (id < 0) {
            conn.close();
            continue;
        }

        const client = try allocator.create(Client);
        client.* = Client{
            .conn = conn,
            .allocator = &allocator,
            .is_connected = true,
            .handle_frame = async client.handle(),
            .packet_buffer = undefined,
            .username = undefined,
            .is_loaded = false,
            .is_op = 0,
            .yaw = 0,
            .pitch = 0,
            .x = 0,
            .y = 0,
            .z = 0,
            .id = @bitCast(u8, id),
        };
        client_list[@intCast(usize, @bitCast(u8, id))] = client;
    }
}

/// Cleanup Server
pub fn deinit() void {
    var i: usize = 1;
    while (i < 128) : (i += 1) {
        if (client_list[i] != null) {
            var client = client_list[i].?;
            client.deinit();
            allocator.destroy(client);
            client_list[i] = null;
        }
    }

    users.deinit();
    socket.close();
    network.deinit();
    world.deinit();
}
