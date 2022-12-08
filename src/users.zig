const std = @import("std");
const fs = std.fs;
const net = std.net;

const Address = net.Address;

pub const User = struct {
    name: [16]u8,
    ip: Address,
    banned: bool,
    ip_banned: bool,
    op: bool,
};

var array: std.ArrayList(User) = undefined;

pub fn init(alloc: std.mem.Allocator) !void {
    array = std.ArrayList(User).init(alloc);
    try load_users();
}

pub fn add_user(user: User) !void {
    try array.append(user);
    try save_users();
}

pub fn get_user_name(name: []const u8) ?*User {
    for (array.items) |*item| {
        if (std.mem.eql(u8, item.name[0..], name)) {
            return item;
        }
    }

    return null;
}

pub fn get_user_ip(ip: Address) ?*User {
    for (array.items) |*item| {
        if (item.ip.eql(ip)) {
            return item;
        }
    }

    return null;
}

pub fn ban_user(name: []const u8) !bool {
    var user = get_user_name(name);
    if (user != null) {
        user.?.banned = true;
    } else {
        return false;
    }

    try save_users();
    return true;
}

pub fn ip_ban_user(name: []const u8) !bool {
    var user = get_user_name(name);
    if (user != null) {
        user.?.ip_banned = true;
    } else {
        return false;
    }

    try save_users();
    return true;
}

pub fn unban_user(name: []const u8) !bool {
    var user = get_user_name(name);
    if (user != null) {
        user.?.banned = false;
    } else {
        return false;
    }

    try save_users();
    return true;
}

pub fn ip_unban_user(name: []const u8) !bool {
    var user = get_user_name(name);
    if (user != null) {
        user.?.ip_banned = false;
    } else {
        return false;
    }

    try save_users();
    return true;
}

pub fn op_user(name: []const u8) !bool {
    var user = get_user_name(name);
    if (user != null) {
        user.?.op = true;
    } else {
        return false;
    }

    try save_users();
    return true;
}

pub fn deop_user(name: []const u8) !bool {
    var user = get_user_name(name);
    if (user != null) {
        user.?.op = false;
    } else {
        return false;
    }

    try save_users();
    return true;
}

pub fn save_users() !void {
    var file = try fs.cwd().openFile("users.dat", fs.File.OpenFlags{ .mode = .read_write, .intended_io_mode = .blocking, .lock = .Exclusive });

    var writer = file.writer();
    for (array.items) |*item| {
        try writer.writeAll(std.mem.asBytes(item));
    }

    file.close();
}

pub fn load_users() !void {
    var file = fs.cwd().openFile("users.dat", fs.File.OpenFlags{ .mode = .read_only });

    if (file) |f| {
        const reader = f.reader();

        while (true) {
            var user: User = undefined;
            var size = reader.read(std.mem.asBytes(&user));

            if (size) |s| {
                if (s > 0) {
                    try array.append(user);
                } else {
                    break;
                }
            } else |err| {
                std.debug.print("Error: {s}\n", .{@errorName(err)});
                break;
            }
        }
        f.close();
    } else |err| {
        std.debug.print("Error: {s}\n", .{@errorName(err)});
        var f2 = try fs.cwd().createFile("users.dat", fs.File.CreateFlags{ .truncate = true });
        f2.close();
    }
}

pub fn deinit() void {
    array.deinit();
}
