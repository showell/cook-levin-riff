# The cube page's platform: the rolling cube's rule as one pure function,
# which the wasm exports to the page as `roll`, and the language's longest
# program as `max_length` (web/cube.js;
# roc/CubeWasm.roc is the app). It is main.roc's twin over the same
# host.wasm (web/platform/host.c). Numbers in, numbers out: the page keeps
# the state, and nothing is kept between calls.
platform ""
	requires {
		roll : U64, U64 -> U64,
		max_length : U64 -> U64
	}
	exposes []
	packages {}
	provides {
		"max_length": max_length_for_host,
		"roll": roll_for_host,
	}
	targets: {
		inputs_dir: "targets/",
		wasm32: {
			inputs: ["host.wasm", app],
			output: Shared,
			exports: ["roll", "max_length"],
			initial_stack_size: 65536,
		},
	}

max_length_for_host : U32 -> U32
max_length_for_host = |x| max_length(x.to_u64()).to_u32_wrap()

roll_for_host : U32, U32 -> U32
roll_for_host = |state, move| roll(state.to_u64(), move.to_u64()).to_u32_wrap()
