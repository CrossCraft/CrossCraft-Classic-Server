const std = @import("std");
const network = @import("network");
const Client = @import("client.zig");
const world = @import("world.zig");
const protocol = @import("protocol.zig");
const users = @import("users.zig");
const broadcaster = @import("broadcaster.zig");

/// Fixed Buffer Allocation
var ram_buffer: [16 * 1000 * 1000]u8 = undefined;
var allocator: std.mem.Allocator = undefined;
var fba: std.heap.FixedBufferAllocator = undefined;

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

/// Spawns all players previously connected for a new client
/// client - Client to send the spawn packets to
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

/// Sends a despawn & message to all clients
/// Triggered by gc_dead_clients()
/// client - Client that is being killed
fn broadcast_client_kill(client: *Client) !void {
    var buf = try protocol.create_packet(&allocator, protocol.Packet.DespawnPlayer);
    protocol.make_despawn_player(buf, client.id);
    broadcaster.request_broadcast(buf, client.id);

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

    var buf2 = try protocol.create_packet(&allocator, protocol.Packet.Message);
    try protocol.make_message(buf2, 0, msg[0..]);
    broadcaster.request_broadcast(buf2, client.id);
}

/// Garbage Collect Dead Clients
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

fn send_message_to_client(msg: []const u8, sender_id: u8) !void {
    if(sender_id == 0){
        std.debug.print("{s}\n", .{msg});
    } else {
        var buf = try protocol.create_packet(&allocator, protocol.Packet.Message);
        defer allocator.free(buf);
        try protocol.make_message(buf, @bitCast(i8, sender_id), msg);
        
        if(client_list[sender_id] != null){
            client_list[sender_id].?.send(buf);
        }
    }
}

fn get_client(name: []const u8) ?*Client {
    var i: usize = 1;
    while(i < 128) : (i += 1){
        if(client_list[i] != null){
            var name2 = std.mem.sliceTo(client_list[i].?.username[0..], 0);
            if(std.mem.eql(u8, name, name2)){
                return client_list[i];
            }
        }
    }

    return null;
}

/// Parses a given command or text
/// cmd - The command input
pub fn parse_command(cmd: []const u8, sender_id: u8) !void {

    if(cmd[0] == '/') {
        //It's a command
        var cmd_first = std.mem.sliceTo(cmd, ' ');
        if(cmd_first.len == cmd.len){
            cmd_first = std.mem.sliceTo(cmd, '\n');
        }

        if(std.mem.eql(u8, "/help", cmd_first)) {
            try send_message_to_client("&eHelp: ", sender_id);
            try send_message_to_client("&e/msg <user> <message_contents> -- Send message to a user", sender_id);
            try send_message_to_client("&e/list -- List all current players", sender_id);
            try send_message_to_client("&e/tp <x> <y> <z> -- Teleports you to the location if valid", sender_id);
            if(sender_id == 0 or client_list[sender_id].?.is_op != 0) {
                try send_message_to_client("&e/kick <user> -- Kicks user from the server", sender_id);
                try send_message_to_client("&e/ban <user> -- Bans user from the server", sender_id);
                try send_message_to_client("&e/unban <user> -- Unbans user from the server", sender_id);
                try send_message_to_client("&e/ip-ban <user> -- IP Bans user from the server", sender_id);
                try send_message_to_client("&e/ip-unban <user> -- IP Unbans user from the server", sender_id);
                try send_message_to_client("&e/op <user> -- Makes user an operator", sender_id);
                try send_message_to_client("&e/deop <user> -- Removes another user's operator status", sender_id);
                try send_message_to_client("&e/stop -- Stops the server", sender_id);
            }
        } else if (std.mem.eql(u8, "/list", cmd_first)) {
            try send_message_to_client("&eCurrent Players:", sender_id);
            var i: usize = 1;
            while(i < 128): (i += 1){
                if(client_list[i] != null){
                    var buf: [18]u8 = undefined;
                    buf[0] = '&';
                    buf[1] = 'a';
                    var pos: usize = 2;
                    while(pos < 16 and client_list[i].?.username[pos - 2] != 0) : (pos += 1) {
                        buf[pos] = client_list[i].?.username[pos - 2];
                    }
                    try send_message_to_client(buf[0..pos], sender_id);
                }
            }
        } else if(std.mem.eql(u8, "/msg", cmd_first)) {
            var cmd_second = std.mem.sliceTo(cmd[cmd_first.len+1..], ' ');

            std.debug.print("MSG {s}\n", .{cmd_second});

            var client = get_client(cmd_second);
            if(client == null){
                try send_message_to_client("Couldn't find player!", sender_id);
                return;
            }

            std.debug.print("MSG\n", .{});

            var cmd_remain = cmd[cmd_first.len+1+cmd_second.len+1..];
            var buf: [256]u8 = undefined;
            var user = if(sender_id == 0) "Console" else std.mem.sliceTo(client_list[sender_id].?.username[0..], 0);

            std.debug.print("User {s}\n", .{user});

            var msg = try std.fmt.bufPrint(buf[0..], "&7[{s}->{s}]: {s}", .{ user, cmd_second, cmd_remain});

            std.debug.print("MSG\n", .{});

            try send_message_to_client(msg[0..64], sender_id);
            try send_message_to_client(msg[0..64], client.?.id);
        } else if(std.mem.eql(u8, "/tp", cmd_first)) {
            var pos = cmd_first.len + 1;
            var cmd_second = std.mem.sliceTo(cmd[pos..], ' ');
            pos += cmd_second.len + 1;
            var cmd_third = std.mem.sliceTo(cmd[pos..], ' ');
            pos += cmd_second.len + 1;
            var cmd_fourth = std.mem.sliceTo(cmd[pos..], ' ');

            var x = std.fmt.parseInt(u16, cmd_second, 0) catch 0;
            var y = std.fmt.parseInt(u16, cmd_third, 0) catch 0;
            var z = std.fmt.parseInt(u16, cmd_fourth, 0) catch 0;

            var client = client_list[sender_id].?;
            client.x = x * 32 - 16;
            client.y = y * 32 - 16;
            client.z = z * 32 - 16;
            client.yaw = 0;
            client.pitch = 0;

            var buf = try protocol.create_packet(&allocator, protocol.Packet.PlayerTeleport);
            defer allocator.free(buf);
            try protocol.make_teleport_player(buf, 255, client.x, client.y, client.z, client.yaw, client.pitch);
            if(sender_id != 0){
                client.send(buf);
            }
        } else {
            std.debug.print("Error: Unknown command {s}\n", .{cmd_first});
            try send_message_to_client("&cUnknown command!", sender_id);
        }

    } else {
        //Just a message
        var pos: usize = 13;
        var msg: [64]u8 = [_]u8{0x20} ** 64;

        std.mem.copy(u8, msg[0..], "&4[Console]: ");
        
        while(pos < 64 and pos - 13 < cmd.len and (cmd[pos - 13] != '\n' and cmd[pos - 13] != '\r' and cmd[pos - 13] != 0)) : (pos += 1){
            msg[pos] = cmd[pos - 13];
        }

        var buf = try protocol.create_packet(&allocator, protocol.Packet.Message);
        try protocol.make_message(buf, 0, msg[0..]);
        broadcaster.request_broadcast(buf, 0);
    }
}

/// Command parser loop REPL
fn command_loop() !void {
    var console_in = std.io.getStdIn();
    var console_reader = console_in.reader();

    while(true) {
        std.time.sleep(50 * 1000 * 1000);
        var console_input : [256]u8 = [_]u8{0} ** 256;
        var cmd = try console_reader.read(console_input[0..]);

        if(cmd > 0)        
            try parse_command(console_input[0..cmd], 0);
    }
}

/// Run Server
pub fn run() !void {
    var frame = async command_loop();

    while (true) {
        // Sleep 50ms (20 TPS)
        std.time.sleep(50 * 1000 * 1000);

        world.update();
        ticks_alive += 1;

        // PING ALL CLIENTS
        if (ticks_alive % 100 == 0) {
            try ping_all();

            // Garbage Collect Dead Clients
            try gc_dead_clients();
        }

        // Broadcast all events
        broadcaster.broadcast_all(&allocator, client_list[0..]);

        // Check Client
        var conn = socket.accept() catch |err| switch (err) {
            error.WouldBlock => continue,
            else => return err,
        };
        // New Client Thread
        var id: i8 = get_available_ID();

        if (id < 0) {
            conn.close();
            continue;
        }

        var endpoint = try conn.getRemoteEndPoint();
        var ip: [15]u8 = [_]u8{0} ** 15;
        _ = try std.fmt.bufPrint(ip[0..], "{s}", .{endpoint.address});

        std.debug.print("New Client @ {s}\n", .{ip});

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
            .ip = ip
        };
        client_list[@intCast(usize, @bitCast(u8, id))] = client;
    }
    _ = frame;
}

/// Gets number of users with a given name
/// Used for checking for duplicates
/// name - Name to search for
pub fn get_user_count(name: []const u8) usize {
    var count : usize = 0;

    var i : usize = 1;
    while (i < 128) : (i += 1) {
        if (client_list[i] != null) {
            if(std.mem.eql(u8, client_list[i].?.username[0..], name)){
                count += 1;
            }
        }
    }

    return count;
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
