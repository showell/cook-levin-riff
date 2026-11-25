//! The Nibble page's wasm host: the page's three exports onto the Roc
//! machine (../../roc/NibbleWasm.roc on main.roc), and the memory and crash
//! functions Roc calls. Copied from roc-js-interpreter's js/wasm/host.zig,
//! cut to what pure functions of numbers need: no kept value, no effects.
//!
//! - `status(n, code)`: run_progam's Status for program `code` on input `n`
//!   (none 0, accepted 1, rejected 2);
//! - `after(n, code, k)`: the machine after the first `k` ops, packed: AX in
//!   bits 0-1, halted in bit 2, the Status in bits 3-4;
//! - `accepts(code)`: the code of the language the program accepts.
//!
//! The module imports nothing: the page instantiates it with no import
//! object, so a Roc crash (a caller's bug, as an op digit past 3) is a trap,
//! which the page sees as a RuntimeError.
//!
//! Memory is the wasm allocator's, freed as the Roc frees it: each
//! allocation carries its size in the sixteen bytes before what Roc is
//! given, for `roc_dealloc` and `roc_realloc`.

const std = @import("std");

extern fn roc_status(input: u64, code: u64) callconv(.c) u64;
extern fn roc_after(input: u64, code: u64, steps: u64) callconv(.c) u64;
extern fn roc_accepts(code: u64) callconv(.c) u64;

pub export fn status(n: u32, code: u32) u32 {
    return @intCast(roc_status(n, code));
}

/// A negative `k`, as the page never asks, is no steps, as the C's loop had it.
pub export fn after(n: u32, code: u32, steps: i32) u32 {
    return @intCast(roc_after(n, code, @intCast(@max(steps, 0))));
}

pub export fn accepts(code: u32) u32 {
    return @intCast(roc_accepts(code));
}

// ---- memory and crashes -----------------------------------------------------

const header = 16;
const wasm_allocator = std.heap.wasm_allocator;

fn allocate(len: usize, alignment: usize) [*]u8 {
    if (alignment > header) @trap();
    const total = header + len;
    const mem = wasm_allocator.alignedAlloc(u8, .fromByteUnits(header), total) catch @trap();
    const words: [*]usize = @ptrCast(@alignCast(mem.ptr));
    words[0] = total;
    return mem.ptr + header;
}

fn release(ptr: *anyopaque) void {
    const base: [*]u8 = @as([*]u8, @ptrCast(ptr)) - header;
    const words: [*]usize = @ptrCast(@alignCast(base));
    const total = words[0];
    const mem: []align(header) u8 = @alignCast(base[0..total]);
    wasm_allocator.free(mem);
}

fn roc_alloc(len: usize, alignment: usize) callconv(.c) *anyopaque {
    return allocate(len, alignment);
}

fn roc_realloc(ptr: *anyopaque, len: usize, alignment: usize) callconv(.c) *anyopaque {
    const old: [*]u8 = @ptrCast(ptr);
    const words: [*]usize = @ptrCast(@alignCast(old - header));
    const was = words[0] - header;
    const new = allocate(len, alignment);
    @memcpy(new[0..@min(was, len)], old[0..@min(was, len)]);
    release(ptr);
    return new;
}

fn roc_dealloc(ptr: *anyopaque, alignment: usize) callconv(.c) void {
    _ = alignment;
    release(ptr);
}

fn roc_crashed(bytes: [*]const u8, len: usize) callconv(.c) void {
    _ = .{ bytes, len };
    @trap();
}

fn roc_dbg(bytes: [*]const u8, len: usize) callconv(.c) void {
    _ = .{ bytes, len };
}

fn roc_expect_failed(bytes: [*]const u8, len: usize) callconv(.c) void {
    _ = .{ bytes, len };
}

comptime {
    @export(&roc_alloc, .{ .name = "roc_alloc" });
    @export(&roc_realloc, .{ .name = "roc_realloc" });
    @export(&roc_dealloc, .{ .name = "roc_dealloc" });
    @export(&roc_crashed, .{ .name = "roc_crashed" });
    @export(&roc_dbg, .{ .name = "roc_dbg" });
    @export(&roc_expect_failed, .{ .name = "roc_expect_failed" });
}
