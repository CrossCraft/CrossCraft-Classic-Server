const std = @import("std");

var alloc: *std.mem.Allocator = undefined;
pub var worldData: []u8 = undefined;
pub const size: usize = 256 * 64 * 256;

pub fn init(allocator: *std.mem.Allocator) !void {
    alloc = allocator;
    worldData = try allocator.alloc(u8, 256 * 64 * 256);
    @memset(worldData.ptr, 18, worldData.len);
}

pub fn update() void {
    //TODO: Update me
}

pub fn deinit() void {
    alloc.free(worldData);
}
