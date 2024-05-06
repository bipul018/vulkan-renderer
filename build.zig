const std = @import("std");
const CFlags = &.{"-fsanitize=address"};

//TODO later add explicit options for vertex/fragment shader types
fn add_shader_step(b: *std.Build, step: *std.Build.Step, input_name: []const u8, output_name: []const u8) *std.Build.Step.Run {
    const shader_step =
        b.addSystemCommand(&.{"D:\\prgming\\VulkanSDK\\1.3.280.0\\bin\\glslc.exe"});
    shader_step.addFileArg(.{ .path = input_name });
    shader_step.addArg("-o");
    const output = shader_step.addOutputFileArg(output_name);
    step.dependOn(&shader_step.step);
    b.getInstallStep().dependOn(&b.addInstallFileWithDir(output, .bin, output_name).step);
    return shader_step;
}
fn add_include_dir(step: *std.Build.Step.Run, path: [] const u8) void{
    step.addPrefixedDirectoryArg("-I", .{.path = path});
}
fn add_library_dir(step: *std.Build.Step.Run, path: [] const u8) void{
    step.addPrefixedDirectoryArg("-L", .{.path = path});
}
fn add_library(step: *std.Build.Step.Run, libname: [] const u8) void{
    step.addPrefixedFileArg("-l", .{.path=libname});
}
fn add_source(step: *std.Build.Step.Run, src: [] const u8) void{
    step.addFileArg(.{.path = src});
}
fn add_output(step: *std.Build.Step.Run, out: [] const u8) std.Build.LazyPath{
    return step.addPrefixedOutputFileArg("-o", out);
}
pub fn build(b: *std.Build) void {
    // const target = b.standardTargetOptions(.{});
    // const optimize = b.standardOptimizeOption(.{});
    // const exe_artifact = b.addExecutable(.{
    //     .name = "main",
    //     //.root_source_file = .{ .path = "" },
    //     .target = target,
    //     .optimize = optimize,
    // });

    const exe_compile = b.addSystemCommand(&.{"clang"});

    exe_compile.addArgs( CFlags);
    add_source(exe_compile, "main.c");
    add_source(exe_compile, "common-stuff.c");
    add_source(exe_compile, "render-stuff.c");
    add_source(exe_compile, "window-stuff.c");
    add_source(exe_compile, "device-mem-stuff.c");
    add_source(exe_compile, "LooplessSizeMove.c");
    add_source(exe_compile, "stuff.c");
    add_source(exe_compile, "pipe1.c");
    add_source(exe_compile, "pipeline-helpers.c");

    add_include_dir(exe_compile, "../utilities");
    add_include_dir(exe_compile, "../stb");
    add_include_dir(exe_compile, "../VulkanSDK/1.3.280.0/Include");

    add_library_dir(exe_compile, "../VulkanSDK/1.3.280.0/Lib");
    add_library(exe_compile, "vulkan-1");

    add_library(exe_compile, "gdi32");
    add_library(exe_compile, "user32");
    add_library(exe_compile, "Advapi32");

    //For asan
    add_library_dir(exe_compile, "c:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.38.33130/lib/x64");
    add_library(exe_compile, "clang_rt.asan_dbg_dynamic-x86_64");
    add_library(exe_compile, "clang_rt.ubsan_standalone-x86_64");
        
    const exe_output = add_output(exe_compile, "main.exe");
    b.getInstallStep().dependOn(&b.addInstallFileWithDir(exe_output, .bin, "main.exe").step);

    
    // exe_artifact.addIncludePath(.{ .path = "D:\\prgming\\VulkanSDK\\1.3.280.0\\Include" });
    // exe_artifact.addIncludePath(.{ .path = "d:/prgming/utilities" });
    // exe_artifact.addIncludePath(.{ .path = "D:/prgming/stb" });
    // exe_artifact.addCSourceFile(.{ .file = .{
    //     .path = "main.c",
    // }, .flags = CFlags });
    // exe_artifact.addCSourceFile(.{ .file = .{
    //     .path = "common-stuff.c",
    // }, .flags = CFlags });
    // exe_artifact.addCSourceFile(.{ .file = .{
    //     .path = "render-stuff.c",
    // }, .flags = CFlags });
    // exe_artifact.addCSourceFile(.{ .file = .{
    //     .path = "window-stuff.c",
    // }, .flags = CFlags });
    // exe_artifact.addCSourceFile(.{ .file = .{
    //     .path = "device-mem-stuff.c",
    // }, .flags = CFlags });
    // exe_artifact.addCSourceFile(.{ .file = .{
    //     .path = "LooplessSizeMove.c",
    // }, .flags = CFlags });
    // exe_artifact.addCSourceFile(.{ .file = .{
    //     .path = "stuff.c",
    // }, .flags = CFlags });
    // exe_artifact.addCSourceFile(.{ .file = .{
    //     .path = "pipe1.c",
    // }, .flags = CFlags });
    // exe_artifact.addCSourceFile(.{ .file = .{
    //     .path = "pipeline-helpers.c",
    // }, .flags = CFlags });
    // //exe.linkLibC();
    // b.installArtifact(exe_artifact);

    // exe_artifact.addLibraryPath(.{ .path = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.38.33130\\lib\\x64" });
    // exe_artifact.linkSystemLibrary("clang_rt.asan_dbg_dynamic-x86_64");
    // exe_artifact.linkSystemLibrary("clang_rt.ubsan_standalone-x86_64");
    // exe_artifact.addLibraryPath(.{ .path = "D:\\prgming\\VulkanSDK\\1.3.280.0\\Lib" });
    // exe_artifact.linkSystemLibrary("vulkan-1");

    // //Needed for loopless size move
    // exe_artifact.linkSystemLibrary("gdi32");

    // //exe_artifact.linkSystemLibrary("ws2_32");
    // exe_artifact.linkSystemLibrary("c");

    _ = add_shader_step(b, &exe_compile.step, "shaders/glsl.vert", "shaders/vert-sh.spv");
    _ = add_shader_step(b, &exe_compile.step, "shaders/glsl.frag", "shaders/frag-sh.spv");
    // tool_run.addArgs(&.{"glsl.vert -o vert.spv","-r", // raw output to omit quotes around the selected string
    // });

    b.getInstallStep().dependOn(&b.addInstallFileWithDir(.{ .path = "res/aphoto.jpg" }, .bin, "res/aphoto.jpg").step);
    b.getInstallStep().dependOn(&b.addInstallFileWithDir(.{ .path = "res/starry-night.jpg" }, .bin, "res/starry-night.jpg").step);


    //Add a install step to copy shaders and resources

    // const install_artifact = b.addInstallArtifact(exe_artifact, .{
    //     .dest_dir = .{ .override = .bin },
    // });
    // b.getInstallStep().dependOn(&install_artifact.step);

    // const run_artifact = b.addRunArtifact(exe_artifact);

    // run_artifact.step.dependOn(b.getInstallStep());
    // if (b.args) |args| {
    //     run_artifact.addArgs(args);
    // }

    // const run_step = b.step("run", "Also Start Server");

    // run_step.dependOn(&run_artifact.step);
}
