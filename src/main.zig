///
/// CrossCraft Classic Server
///
const std = @import("std");
const server = @import("server.zig");

/// Main
pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{ .enable_memory_limit = true }){};
    defer {
        const leaked = gpa.deinit();
        if (leaked) {
            _ = gpa.detectLeaks();
        }
    }
    const allocator = gpa.allocator();

    std.debug.print("CrossCraft Classic Server v1.3\n", .{});
    std.debug.print("Initializing Network...\n", .{});

    // Start up the server
    try server.init(allocator, .{});
    defer server.deinit();

    try server.run();
    std.debug.print("Server stopped.\n", .{});
}
