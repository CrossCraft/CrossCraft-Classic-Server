const std = @import("std");
const fs = std.fs;
const broadcaster = @import("broadcaster.zig");
const generator = @import("generator.zig");
const protocol = @import("protocol.zig");
const zlib = @cImport({
    @cInclude("zlib.h");
});

const Location = struct { x: u32, y: u32, z: u32 };

const Update = struct { loc: Location, blk: u8 };

var alloc: *std.mem.Allocator = undefined;
pub const size: usize = 256 * 64 * 256;
pub var worldData: [size]u8 = undefined;
var tick_count: usize = 0;
var seed: u32 = 0;

var update_list: [1024]Update = undefined;
var update_count: usize = 0;

var update2_list: [1024]Update = undefined;
var update2_count: usize = 0;

const RndGen = std.rand.DefaultPrng;
var rnd: RndGen = undefined;

pub fn init(allocator: *std.mem.Allocator) !void {
    alloc = allocator;
    rnd = RndGen.init(@bitCast(u64, std.time.timestamp()));
    seed = rnd.random().int(u32);

    @memset(worldData[0..], 0, worldData.len);

    var file = fs.cwd().openFile("save.ccc", fs.File.OpenFlags{ .mode = .read_only });
    if (file) {
        var f2 = file catch unreachable;
        f2.close();
        load();
    } else |err| {
        //We have an error, so generate world
        generator.generate(seed);
        std.debug.print("Error: {s}\n", .{@errorName(err)});
        std.debug.print("Couldn't load save, generating world.\n", .{});
    }

    save("save.ccc");
}

pub fn getIdx(x: u32, y: u32, z: u32) usize {
    if (x >= 0 and x < 256 and y >= 0 and y < 64 and z >= 0 and z < 256)
        return (y * 256 * 256) + (z * 256) + x;

    return 0;
}

fn rtick() !void {
    var x = rnd.random().int(u8);
    var y = rnd.random().int(u8) % 64;
    var z = rnd.random().int(u8);
    var idx = getIdx(x, y, z);

    var blk = worldData[idx];

    if (blk == 6) {
        worldData[idx] = 0;
        //Sapling
        if (generator.growTree(x, y, z, rnd.random().int(u8) % 3 + 4)) {
            var i: u32 = 0;
            while (i < 6) : (i += 1) {
                var cx = x - 2;
                while (cx <= x + 2) : (cx += 1) {
                    var cz = z - 2;
                    while (cz <= z + 2) : (cz += 1) {
                        var idx2 = getIdx(cx, y + i, cz);
                        var blk2 = worldData[idx2];

                        try set_block(cx, @intCast(u16, y + i), cz, blk2);
                    }
                }
            }
        } else {
            worldData[idx] = 6;
        }

        return;
    }

    var is_dark: bool = false;

    var test_y: u32 = 63;
    while (test_y > y) : (test_y -= 1) {
        var blk2 = worldData[getIdx(x, test_y, z)];

        if (!(blk2 == 0 or blk2 == 6 or blk2 == 18 or blk2 == 20 or blk2 == 37 or blk2 == 38 or blk2 == 39 or blk2 == 40)) {
            is_dark = true;
            break;
        }
    }

    if (blk == 3 and !is_dark) {
        try set_block(x, y, z, 2);
        return;
    }

    if (blk == 2 and is_dark) {
        try set_block(x, y, z, 3);
        return;
    }

    if (blk == 37 or blk == 38) {
        if (is_dark) {
            try set_block(x, y, z, 0);
        }
        return;
    }

    if (blk == 39 or blk == 40) {
        if (!is_dark) {
            try set_block(x, y, z, 0);
        }
        return;
    }
}

fn add_update(upd: Update) void {
    if (update_count + 1 < 1024) {
        update_list[update_count] = upd;
        update_count += 1;
    }
}
fn add_update2(upd: Update) void {
    if (update2_count + 1 < 1024) {
        update2_list[update_count] = upd;
        update2_count += 1;
    }
}

pub fn update_location(x: u32, y: u32, z: u32) void {
    if (x < 1 or y < 1 or z < 1 or x > 254 or z > 254 or y > 62)
        return;

    var idx = getIdx(x, y, z);
    var blk = worldData[idx];

    add_update(Update{ .loc = Location{ .x = x - 1, .y = y, .z = z }, .blk = blk });
    add_update(Update{ .loc = Location{ .x = x + 1, .y = y, .z = z }, .blk = blk });
    add_update(Update{ .loc = Location{ .x = x, .y = y - 1, .z = z }, .blk = blk });
    add_update(Update{ .loc = Location{ .x = x, .y = y, .z = z - 1 }, .blk = blk });
    add_update(Update{ .loc = Location{ .x = x, .y = y, .z = z + 1 }, .blk = blk });
    add_update(Update{ .loc = Location{ .x = x, .y = y, .z = z }, .blk = blk });

    if (blk < 8 and blk > 10) {
        add_update(Update{ .loc = Location{ .x = x, .y = y + 1, .z = z }, .blk = blk });
    }
}

pub fn update_location2(x: u32, y: u32, z: u32) void {
    if (x < 1 or y < 1 or z < 1 or x > 254 or z > 254 or y > 62)
        return;

    var idx = getIdx(x, y, z);
    var blk = worldData[idx];

    add_update2(Update{ .loc = Location{ .x = x - 1, .y = y, .z = z }, .blk = blk });
    add_update2(Update{ .loc = Location{ .x = x + 1, .y = y, .z = z }, .blk = blk });
    add_update2(Update{ .loc = Location{ .x = x, .y = y - 1, .z = z }, .blk = blk });
    add_update2(Update{ .loc = Location{ .x = x, .y = y, .z = z - 1 }, .blk = blk });
    add_update2(Update{ .loc = Location{ .x = x, .y = y, .z = z + 1 }, .blk = blk });
    add_update2(Update{ .loc = Location{ .x = x, .y = y, .z = z }, .blk = blk });

    if (blk < 8 and blk > 10) {
        add_update2(Update{ .loc = Location{ .x = x, .y = y + 1, .z = z }, .blk = blk });
    }
}

fn check_liquid_fill(x: u32, y: u32, z: u32, fill_blk: u8) !void {
    if (x < 1 or y < 1 or z < 1 or x > 254 or z > 254 or y > 62)
        return;

    var check_blk = worldData[getIdx(x, y, z)];
    if (check_blk == 0) {
        try set_block(@intCast(u16, x), @intCast(u16, y), @intCast(u16, z), fill_blk);
        update_location2(x, y, z);
    }
}

fn update_world() !void {
    var i: usize = 0;
    while (i < update_count) : (i += 1) {
        var upd = update_list[i];

        var curr_blk = worldData[getIdx(upd.loc.x, upd.loc.y, upd.loc.z)];

        if (curr_blk >= 8 and curr_blk <= 10) {
            if (upd.loc.x < 1 or upd.loc.y < 1 or upd.loc.z < 1 or upd.loc.x > 254 or upd.loc.z > 254 or upd.loc.y > 62)
                continue;

            //Fill
            try check_liquid_fill(upd.loc.x - 1, upd.loc.y, upd.loc.z, curr_blk);
            try check_liquid_fill(upd.loc.x + 1, upd.loc.y, upd.loc.z, curr_blk);
            try check_liquid_fill(upd.loc.x, upd.loc.y - 1, upd.loc.z, curr_blk);
            try check_liquid_fill(upd.loc.x, upd.loc.y, upd.loc.z - 1, curr_blk);
            try check_liquid_fill(upd.loc.x, upd.loc.y, upd.loc.z + 1, curr_blk);
        }

        if (curr_blk >= 37 and curr_blk <= 40 or curr_blk == 6) {
            var blk_bel = worldData[getIdx(upd.loc.x, upd.loc.y - 1, upd.loc.z)];
            if (blk_bel == 0 or blk_bel == 8 or blk_bel == 9 or blk_bel == 10 or blk_bel == 11) {
                try set_block(@intCast(u16, upd.loc.x), @intCast(u16, upd.loc.y), @intCast(u16, upd.loc.z), 0);
            }
        }

        if (curr_blk == 12 or curr_blk == 13) {
            var blk_bel = worldData[getIdx(upd.loc.x, upd.loc.y - 1, upd.loc.z)];
            if (blk_bel == 0 or blk_bel == 8 or blk_bel == 9 or blk_bel == 10 or blk_bel == 11) {
                try set_block(@intCast(u16, upd.loc.x), @intCast(u16, upd.loc.y - 1), @intCast(u16, upd.loc.z), curr_blk);
                try set_block(@intCast(u16, upd.loc.x), @intCast(u16, upd.loc.y), @intCast(u16, upd.loc.z), 0);
                upd.blk = 0;
                update_location(upd.loc.x, upd.loc.y, upd.loc.z);
            }
        }
    }

    i = 0;
    while (i < update2_count) : (i += 1) {
        update_list[i] = update2_list[i];
    }
    update_count = update2_count;
    update2_count = 0;
}

pub fn update() !void {
    if (tick_count % 10 == 0) {
        try update_world();
        var i: usize = 0;
        while (i < 256 * 4 * 40) : (i += 1) {
            try rtick();
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
        var fname = std.fmt.allocPrintZ(alloc.*, "save.ccc.{}", .{tstamp}) catch unreachable;
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

pub fn set_block(x: u16, y: u16, z: u16, blk: u8) !void {
    var idx = getIdx(x, y, z);
    worldData[idx] = blk;

    var buf = try protocol.create_packet(alloc, protocol.Packet.SetBlock);
    try protocol.make_set_block(buf, x, y, z, worldData[idx]);
    broadcaster.request_broadcast(buf, 0);
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

pub fn deinit() void {}
