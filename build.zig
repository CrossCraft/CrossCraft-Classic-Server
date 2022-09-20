const std = @import("std");

const pkgs = struct {
    const network = std.build.Pkg {
        .name = "network",
        .source = .{ .path = "network/network.zig" },
    };
};

pub fn build(b: *std.build.Builder) void {

    const target = b.standardTargetOptions(.{});
    const mode = b.standardReleaseOptions();

    const exe = b.addExecutable("Classic-Server", "src/main.zig");
    exe.setTarget(target);
    exe.setBuildMode(mode);
    exe.addPackage(pkgs.network);
    exe.addSystemIncludePath("/usr/include");
    exe.linkSystemLibrary("zlib");
    exe.linkLibC();
    exe.use_stage1 = true;
    exe.install();

    const run_cmd = exe.run();
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the server");
    run_step.dependOn(&run_cmd.step);

    const exe_tests = b.addTest("src/main.zig");
    exe_tests.setTarget(target);
    exe_tests.setBuildMode(mode);
}
