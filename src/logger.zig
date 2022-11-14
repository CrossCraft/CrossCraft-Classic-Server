const std = @import("std");
const fs = std.fs;

var log_file: fs.File = undefined;

pub fn log_chat(msg: []const u8) !void {
    var tstamp = std.time.timestamp();
    var buf: [512]u8 = undefined;

    var slice = try std.fmt.bufPrint(buf[0..], "{}: {s}\n", .{ tstamp, msg });
    std.debug.print("{s}", .{slice});
}
