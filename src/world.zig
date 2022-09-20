const std = @import("std");

var alloc: *std.mem.Allocator = undefined;
pub var worldData: []u8 = undefined;
pub const size: usize = 256 * 64 * 256;
var tick_count: usize = 0;

pub fn init(allocator: *std.mem.Allocator) !void {
    alloc = allocator;
    worldData = try allocator.alloc(u8, 256 * 64 * 256);
    @memset(worldData.ptr, 0, worldData.len);
    
    //TODO: Generate World OR Load From Save
}

pub fn update() void {
    //TODO: Update the world with RTick and UpdateList

    tick_count += 1;

    // Autosave
    if(tick_count % 1200 == 0){
        save();        
    }
}

pub fn save() void {
    //TODO: Save world
}

pub fn deinit() void {
    alloc.free(worldData);
}
