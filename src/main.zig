const std = @import("std");
const network = @import("network");

pub const io_mode = .evented;

pub fn main() !void {
    std.debug.print("CrossCraft Classic Server v1.3\n", .{});
    std.debug.print("Initializing Network...\n", .{});

    try network.init();
    defer network.deinit();
}