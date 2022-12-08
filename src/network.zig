const std = @import("std");
const fmt = std.fmt;
const mem = std.mem;
const net = std.net;
const os = std.os;

const Config = @import("server.zig").Config;
const Address = net.Address;

const BeaconAddress = "www.classicube.net";

var listener_socket: os.socket_t = undefined;
var heartbeat_socket: os.socket_t = undefined;

pub fn start_server_sock() !void {
    listener_socket = try os.socket(os.AF.INET, os.SOCK.STREAM, 0);

    const flags = try os.fcntl(listener_socket, os.F.GETFL, 0);
    _ = try os.fcntl(listener_socket, os.F.SETFL, (flags | os.O.NONBLOCK));

    try os.setsockopt(listener_socket, os.SOL.SOCKET, os.SO.REUSEADDR, &mem.toBytes(@as(c_int, 1)));

    const address = try Address.parseIp("0.0.0.0", 25565);
    try os.bind(listener_socket, &address.any, @sizeOf(@TypeOf(address.any)));
    try os.listen(listener_socket, 1);
}

pub fn stop_server_sock() void {
    os.close(listener_socket);
}

pub fn start_heartbeat(allocator: mem.Allocator, config: Config) !void {
    var addressList = try std.net.getAddressList(allocator, BeaconAddress, 80);
    defer addressList.deinit();

    heartbeat_socket = try os.socket(os.AF.INET, os.SOCK.STREAM, 0);

    const address = addressList.addrs[0];
    try os.connect(heartbeat_socket, &address.any, @sizeOf(@TypeOf(address.any)));

    var content = std.ArrayList(u8).init(allocator);
    defer content.deinit();
    const content_writer = content.writer();
    try content_writer.print("name={s}", .{fmt.fmtSliceEscapeUpper(config.name)});
    try content_writer.print("&port={d}", .{config.port});
    try content_writer.print("&max={d}", .{config.max_players});
    try content_writer.print("&public={any}", .{config.public});
    try content_writer.print("&version={d}", .{config.version});
    try content_writer.print("&salt={s}", .{fmt.fmtSliceEscapeUpper(config.salt)});
    try content_writer.print("&software={s}", .{fmt.fmtSliceEscapeUpper(config.software)});
    try content_writer.print("&web={any}", .{config.web});
    try content_writer.print("&users={d}", .{0});

    var request = std.ArrayList(u8).init(allocator);
    defer request.deinit();
    const request_writer = request.writer();
    try request_writer.print("POST /heartbeat.jsp HTTP/1.1\r\n", .{});
    try request_writer.print("User-Agent: {s}\r\n", .{fmt.fmtSliceEscapeUpper(config.software)});
    try request_writer.print("Content-Type: application/x-www-form-urlencoded\r\n", .{});
    try request_writer.print("Host: {s}\r\n", .{BeaconAddress});
    try request_writer.print("Cache-Control: no-store,no-cache\r\n", .{});
    try request_writer.print("Pragma: no-cache\r\n", .{});
    try request_writer.print("Content-Length: {d}\r\n", .{content.items.len});
    try request_writer.print("Connection: Keep-Alive\r\n\r\n", .{});
    try request_writer.print("{s}", .{content.items});

    _ = try os.send(heartbeat_socket, request.items, 0);
}

pub fn stop_heartbeat() void {
    os.close(heartbeat_socket);
}

pub const Connection = struct {
    fd: os.socket_t,
    ip: Address,
};

pub fn accept_new_conn() !Connection {
    var address = try Address.parseIp("0.0.0.0", 25565);
    var size: u32 = @sizeOf(@TypeOf(address.any));
    const conn = try os.accept(listener_socket, &address.any, &size, os.O.NONBLOCK);

    var connection: Connection = .{
        .fd = conn,
        .ip = address,
    };

    try os.setsockopt(conn, os.IPPROTO.TCP, os.TCP.NODELAY, &mem.toBytes(@as(c_int, 1)));

    return connection;
}

pub fn close_conn(fd: os.fd_t) void {
    os.close(fd);
}

pub fn peek_recv(fd: os.fd_t) !u8 {
    var r: [1]u8 = undefined;
    _ = try os.recv(fd, &r, os.MSG.PEEK);
    return r[0];
}

pub fn conn_send(fd: os.fd_t, buffer: []const u8) !usize {
    var total_sent: usize = 0;
    while (total_sent < buffer.len) {
        total_sent += os.send(fd, buffer, os.MSG.NOSIGNAL) catch |err| switch (err) {
            error.WouldBlock => continue,
            else => |e| return e,
        };
    }
    return total_sent;
}

pub fn full_recv(fd: os.fd_t, buffer: []u8) !usize {
    var total_recv: usize = 0;
    while (total_recv < buffer.len) {
        total_recv += try os.recv(fd, buffer, 0);
    }
    return total_recv;
}
