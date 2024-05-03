const std = @import("std");
const CFlags = &.{};


//TODO later add explicit options for vertex/fragment shader types
fn add_shader_step(b: *std.Build, step: *std.Build.Step, input_name : [] const u8, output_name :[] const u8) *std.Build.Step.Run{
    const shader_step =
        b.addSystemCommand(&.{"D:\\prgming\\VulkanSDK\\1.3.280.0\\bin\\glslc.exe"});
    shader_step.addFileArg(.{.path =input_name});
    shader_step.addArg("-o");
    const output=shader_step.addOutputFileArg(output_name);
    step.dependOn(&shader_step.step);
    b.getInstallStep().dependOn(&b.addInstallFileWithDir(output, .bin, output_name).step);
    return shader_step;
}
pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const exe_artifact = b.addExecutable(.{
        .name = "main",
        //.root_source_file = .{ .path = "" },
        .target = target,
        .optimize = optimize,
    });
    exe_artifact.addIncludePath(.{ .path = "D:\\prgming\\VulkanSDK\\1.3.280.0\\Include" });
    exe_artifact.addIncludePath(.{ .path = "d:/prgming/utilities" });
    exe_artifact.addCSourceFile(.{ .file = .{
        .path = "main.c",
        }, .flags = CFlags });
    exe_artifact.addCSourceFile(.{ .file = .{
        .path = "common-stuff.c",
        }, .flags = CFlags });
    exe_artifact.addCSourceFile(.{ .file = .{
        .path = "render-stuff.c",
        }, .flags = CFlags });
    exe_artifact.addCSourceFile(.{ .file = .{
        .path = "window-stuff.c",
        }, .flags = CFlags });
    exe_artifact.addCSourceFile(.{ .file = .{
        .path = "device-mem-stuff.c",
        }, .flags = CFlags });
    exe_artifact.addCSourceFile(.{ .file = .{
        .path = "LooplessSizeMove.c",
        }, .flags = CFlags });
    exe_artifact.addCSourceFile(.{ .file = .{
        .path = "stuff.c",
        }, .flags = CFlags });
    //exe.linkLibC();
    b.installArtifact(exe_artifact);

    exe_artifact.addLibraryPath(.{ .path = "D:\\prgming\\VulkanSDK\\1.3.280.0\\Lib" });
    exe_artifact.linkSystemLibrary("vulkan-1");

    //Needed for loopless size move
    exe_artifact.linkSystemLibrary("gdi32");
    
    //exe_artifact.linkSystemLibrary("ws2_32");
    exe_artifact.linkSystemLibrary("c");


    _ = add_shader_step(b, &exe_artifact.step, "shaders/glsl.vert", "shaders/vert-sh.spv");
    _ = add_shader_step(b, &exe_artifact.step, "shaders/glsl.frag", "shaders/frag-sh.spv");
    // tool_run.addArgs(&.{"glsl.vert -o vert.spv","-r", // raw output to omit quotes around the selected string
    // });

    b.getInstallStep().dependOn(
        &b.addInstallFileWithDir(.{.path = "res/starry-night.jpg"}, .bin, "res/starry-night.jpg").step);

    
    //Add a install step to copy shaders and resources

    const install_artifact = b.addInstallArtifact(exe_artifact, .{
        .dest_dir = .{ .override = .bin },
    });
    b.getInstallStep().dependOn(&install_artifact.step);
    
    const run_artifact = b.addRunArtifact(exe_artifact);

    run_artifact.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_artifact.addArgs(args);
    }

    const run_step = b.step("run", "Also Start Server");
    
    run_step.dependOn(&run_artifact.step);
}
