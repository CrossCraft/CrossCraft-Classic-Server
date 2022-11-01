///
/// CrossCraft Classic Server
///
const std = @import("std");
const network = @import("network");
const server = @import("server.zig");

/// ASYNC IO
/// TODO: Re-evaluate thread models
/// Not supported yet in v0.10.0
/// Requires -fstage1
pub const io_mode = .evented;

/// Main
pub fn main() !void {
    std.debug.print("CrossCraft Classic Server v1.3\n", .{});
    std.debug.print("Initializing Network...\n", .{});

    // Start up the server

    try server.init();
    defer server.deinit();

    try server.run();
}
