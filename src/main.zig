///
/// CrossCraft Classic Server
///
const std = @import("std");
const network = @import("network");
const server = @import("server.zig");

/// Main
pub fn main() !void {
    std.debug.print("CrossCraft Classic Server v1.3\n", .{});
    std.debug.print("Initializing Network...\n", .{});

    // Start up the server

    try server.init();
    defer server.deinit();

    try server.run();
}
