# The Nibble page's platform: the machine as three pure functions, which the
# wasm exports to the page as `status`, `after` and `accepts`
# (web/machine.js; roc/NibbleWasm.roc is the app). Numbers in, numbers out:
# nothing is kept between calls.
#
# The provides take and give the page's 32-bit numbers and convert to the
# app's U64s. Roc links the app with host.wasm, web/platform/host.c compiled
# by clang (web/build.shale), which holds only the allocator and the runtime
# functions Roc calls. The `requires` form is roc-apps' book_builder
# platform's; the `exports:` list is roc-apps' canvas_apps platform's shape.
platform ""
	requires {
		accepts : U64 -> U64,
		after : U64, U64, U64 -> U64,
		status : U64, U64 -> U64
	}
	exposes []
	packages {}
	provides {
		"accepts": accepts_for_host,
		"after": after_for_host,
		"status": status_for_host,
	}
	targets: {
		inputs_dir: "targets/",
		wasm32: {
			inputs: ["host.wasm", app],
			output: Shared,
			exports: ["status", "after", "accepts"],
			initial_stack_size: 65536,
		},
	}

status_for_host : U32, U32 -> U32
status_for_host = |input, code| status(input.to_u64(), code.to_u64()).to_u32_wrap()

# A negative `k`, as the page never asks, is no steps, as the C's loop had it.
after_for_host : U32, U32, I32 -> U32
after_for_host = |input, code, steps| after(input.to_u64(), code.to_u64(), I32.max(steps, 0).to_u64_wrap()).to_u32_wrap()

accepts_for_host : U32 -> U32
accepts_for_host = |code| accepts(code.to_u64()).to_u32_wrap()
