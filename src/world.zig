const std = @import("std");
const fs = std.fs;
const zlib = @cImport({
    @cInclude("zlib.h");
});

var alloc: *std.mem.Allocator = undefined;
pub var worldData: []u8 = undefined;
pub const size: usize = 256 * 64 * 256;
var tick_count: usize = 0;

pub fn init(allocator: *std.mem.Allocator) !void {
    alloc = allocator;
    worldData = try allocator.alloc(u8, 256 * 64 * 256);
    @memset(worldData.ptr, 0, worldData.len);

    var file = fs.cwd().openFile("save.ccc", fs.File.OpenFlags{.mode = .read_only});
    if(file){
        var f2 = file catch unreachable;
        f2.close();

        load();
    } else |err| {
        //We have an error, so generate world
        //TODO: Generate world
        std.debug.print("Error: {s}\n", .{@errorName(err)});
        std.debug.print("Couldn't load save, generating world.\n", .{});
    }

}

pub fn update() void {
    //TODO: Update the world with RTick and UpdateList

    tick_count += 1;

    // Autosave
    if (tick_count % 1200 == 0) {
        save();
    }
}

pub fn load() void {
    var save_file : zlib.gzFile = zlib.gzopen("save.ccc", "rb");
    _ = zlib.gzrewind(save_file);

    var version: u32 = 0;
    _ = zlib.gzread(save_file, &version, @sizeOf(u32));

    if(version != 3){
        std.debug.print("Please, update your save file to version 3 using the legacy server.\n", .{});
        unreachable;   
    }

    var toss: [3]f32 = undefined;
    _ = zlib.gzread(save_file, &toss[0], @sizeOf([3]f32));

    _ = zlib.gzread(save_file, worldData.ptr, 256 * 64 * 256);
    _ = zlib.gzclose(save_file);
}

pub fn save() void {
    //TODO: Save world
}

pub fn deinit() void {
    alloc.free(worldData);
}
