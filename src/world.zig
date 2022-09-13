const std = @import("std");

var alloc: *std.mem.Allocator = undefined;
pub var worldData: []u8 = undefined;

pub fn init(allocator: *std.mem.Allocator) !void {
    alloc = allocator;
    worldData = try allocator.alloc(u8, 256 * 64 * 256);
}

pub fn deinit() void {
    alloc.free(worldData);
}
