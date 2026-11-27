# The Nibble++ page's platform: Nibble++ as three pure functions, which the
# wasm exports to the page as `status`, `after` and `accepts`
# (web/plus.js; roc/PlusWasm.roc is the app). It is main.roc's twin: the
# same host.wasm (web/platform/host.c), the same shape, but a program is up
# to 30 lines, so its code is a U64, which the page passes as a BigInt.
# Numbers in, numbers out: nothing is kept between calls.
platform ""
	requires {
		accepts : U64, U64 -> U64,
		after : U64, U64, U64 -> U64,
		status : U64, U64, U64 -> U64
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

status_for_host : U32, U64, U32 -> U32
status_for_host = |input, code, lines| status(input.to_u64(), code, lines.to_u64()).to_u32_wrap()

after_for_host : U32, U64, U32 -> U32
after_for_host = |input, code, steps| after(input.to_u64(), code, steps.to_u64()).to_u32_wrap()

accepts_for_host : U64, U32 -> U32
accepts_for_host = |code, lines| accepts(code, lines.to_u64()).to_u32_wrap()
