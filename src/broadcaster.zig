const std = @import("std");
const logger = @import("logger.zig");
const Client = @import("client.zig");

/// Broadcast information
/// buf - The packet buffer to send
/// exclude_id - Excludes client with ID -- 0 otherwise
pub const BroadcastInfo = struct { buf: []u8, exclude_id: u8 };

var packet_queue_lock: std.Thread.Mutex = undefined;
var packet_queue: [512]BroadcastInfo = undefined;
var packet_count: usize = 0;

pub fn init() void {
    for (packet_queue) |*b_info| {
        b_info.buf = null;
        b_info.exclude_id = 0;
    }
    packet_count = 0;
}

pub fn deinit() void {
    packet_count = 0;
}

/// Request Broadcast
/// Adds packet to the queue to broadcast to
pub fn request_broadcast(buf: []u8, exclude_id: u8) void {
    packet_queue_lock.lock();
    defer packet_queue_lock.unlock();

    if (packet_count >= packet_queue.len) {
        return;
    }

    packet_queue[packet_count].buf = buf;
    packet_queue[packet_count].exclude_id = exclude_id;
    packet_count += 1;
}

/// Broadcast to all in a list
pub fn broadcast_all(alloc: std.mem.Allocator, client_list: []?*Client) void {
    packet_queue_lock.lock();
    defer packet_queue_lock.unlock();

    if (packet_count == 0 or packet_count >= 512) {
        return;
    }

    for (packet_queue[0..packet_count]) |*b_info| {
        for (client_list) |c| if (c) |client| {
            if (client.id == b_info.exclude_id or !client.is_ready) {
                continue;
            }
            client.send(b_info.buf);
            std.time.sleep(1000 * 1000);
        };

        if (b_info.buf[0] == 0x0D) {
            logger.log_chat(b_info.buf[1..]) catch unreachable;
        }

        alloc.free(b_info.buf);
        b_info.exclude_id = 0;
    }

    packet_count = 0;
}
