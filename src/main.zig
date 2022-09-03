const std = @import("std");
const network = @import("network");

pub const io_mode = .evented;

var ramBuffer: [16 * 1000 * 1000]u8 = undefined;

pub fn main() !void {
    std.debug.print("CrossCraft Classic Server v1.3\n", .{});
    std.debug.print("Initializing Network...\n", .{});

    var fba = std.heap.FixedBufferAllocator.init(&ramBuffer);
    const allocator = fba.allocator();

    try network.init();
    defer network.deinit();

    var server = try network.Socket.create(.ipv4, .tcp);
    defer server.close();

    try server.bind(.{
        .address = .{ .ipv4 = network.Address.IPv4.any },
        .port = 25565,
    });

    try server.listen();

    std.debug.print("Listening at {}\n", .{try server.getLocalEndPoint()});
    while (true) {
        std.os.nanosleep(0, 50 * 1000 * 1000);
        var conn = server.accept() catch |err| switch(err) {
            error.WouldBlock => continue,
            else => return
        };


        const client = try allocator.create(Client);
        client.* = Client{
            .conn = conn,
            .is_connected = true,
            .handle_frame = async client.handle(),
        };
    }

}

const Client = struct {
    conn: network.Socket,
    is_connected: bool,
    handle_frame: @Frame(Client.handle),

    fn handle(self: *Client) !void {
        while (self.is_connected) {
            var pID: [1]u8 = undefined;
            const amt = try self.conn.receive(&pID);

            if (amt == 0){
                self.is_connected = false;
                break;
            }

            std.debug.print("PACKET RECEIVED! ID {}\n", .{@intCast(u32, pID[0])});


        }
    }
};
