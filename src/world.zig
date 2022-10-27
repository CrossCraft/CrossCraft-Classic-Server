const std = @import("std");
const fs = std.fs;
const zlib = @cImport({
    @cInclude("zlib.h");
});

var alloc: *std.mem.Allocator = undefined;
pub const size: usize = 256 * 64 * 256;
pub var worldData: [size]u8 = undefined;
var tick_count: usize = 0;

const RndGen = std.rand.DefaultPrng;
var rnd : RndGen = undefined;

pub fn init(allocator: *std.mem.Allocator) !void {
    alloc = allocator;
    rnd = RndGen.init(0);

    @memset(worldData[0..], 0, worldData.len);

    var file = fs.cwd().openFile("save.ccc", fs.File.OpenFlags{ .mode = .read_only });
    if (file) {
        var f2 = file catch unreachable;
        f2.close();

        load();
    } else |err| {
        //We have an error, so generate world
        //TODO: Generate world

        var x : u32 = 0;
        var z : u32 = 0;
        while(z < 256) : (z += 1) {
            while(x < 256) : (x += 1) {
                worldData[getIdx(x, 0, z)] = 7;
            }
        }
        std.debug.print("Error: {s}\n", .{@errorName(err)});
        std.debug.print("Couldn't load save, generating world.\n", .{});
    }

    save("save.ccc");
}

pub fn getIdx(x: u32, y: u32, z: u32) usize {
    return (y * 256 * 256) + (z * 256) + x;
}

fn rtick() void {
    var x = rnd.random().int(u8);
    var y = rnd.random().int(u8) % 64;
    var z = rnd.random().int(u8);
    var idx = getIdx(x, y, z);
    
    var blk = worldData[idx];

    if(blk == 6) {
        //Sapling
        std.debug.print("TODO: Sapling!\n", .{});
        return;
    }

    var is_dark : bool = false;

    if(y + 1 >= 64)
        return;

    var blk2 = worldData[getIdx(x, y + 1, z)];
    if(blk2 == 0 or blk2 == 6 or blk2 == 18 or blk2 == 20 or blk2 == 37 or blk2 == 38 or blk2 == 39 or blk2 == 40) {
        is_dark = true;
    }

    if(blk == 3 and !is_dark) {
        // set
        return;
    }
    
    if(blk == 2 and is_dark) {
        // set
        return;
    }

    if(blk == 37 or blk == 38) {
        if (is_dark) {
            // set
        }
        return;
    }

    if(blk == 39 or blk == 40) {
        if (!is_dark) {
            // set
        }
        return;
    }
}

pub fn update() void {
    //TODO: Update the world with UpdateList

    if(tick_count % 5 == 0) {
        var i : usize = 0;
        while(i < 256 * 4) : (i += 1) {
            rtick();
        }
    }

    tick_count += 1;

    // Autosave every 5 minutes
    if (tick_count % 6000 == 0) {
        save("save.ccc");
    }

    // Backup every 6 hours
    if (tick_count % 432000 == 0) {
        var tstamp = std.time.timestamp();
        var fname = std.fmt.allocPrint(alloc.*, "save.ccc.{}", .{tstamp}) catch unreachable;
        defer alloc.free(fname);
        save(fname);
    }
}

pub fn load() void {
    var save_file: zlib.gzFile = zlib.gzopen("save.ccc", "rb");
    _ = zlib.gzrewind(save_file);

    var version: u32 = 0;
    _ = zlib.gzread(save_file, &version, @sizeOf(u32));

    if (version != 3) {
        std.debug.print("Please, update your save file to version 3 using the legacy server.\n", .{});
        unreachable;
    }

    var toss: [3]f32 = undefined;
    _ = zlib.gzread(save_file, &toss[0], @sizeOf([3]f32));

    _ = zlib.gzread(save_file, worldData[0..], 256 * 64 * 256);
    _ = zlib.gzclose(save_file);
}

pub fn save(name: []const u8) void {
    std.debug.print("Autosaving...\n", .{});

    var save_file: zlib.gzFile = zlib.gzopen(name.ptr, "wb");

    const version: u32 = 3;
    _ = zlib.gzwrite(save_file, &version, @sizeOf(u32));

    var toss: [3]u32 = [_]u32{ 256, 64, 256 };
    _ = zlib.gzwrite(save_file, &toss[0], @sizeOf([3]u32));
    _ = zlib.gzwrite(save_file, worldData[0..], 256 * 64 * 256);
    _ = zlib.gzclose(save_file);
}

pub fn deinit() void {
    
}
