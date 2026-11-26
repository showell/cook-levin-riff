# The Nibble page's platform: the machine as three pure functions, which
# web/platform/host.zig exports to the page as `status`, `after` and
# `accepts` (web/machine.js; roc/NibbleWasm.roc is the app). Numbers in,
# numbers out: the host builds no Roc value and keeps none between calls.
#
# Its pattern is roc-js-interpreter's js/wasm/platform (a wasm32 Archive that
# Zig links with the host), cut down to functions of plain numbers, whose
# `requires` form is roc-apps' book_builder platform's.
platform ""
	requires {
		accepts : U64 -> U64,
		after : U64, U64, U64 -> U64,
		status : U64, U64 -> U64
	}
	exposes []
	packages {}
	provides {
		"roc_accepts": accepts_for_host,
		"roc_after": after_for_host,
		"roc_status": status_for_host,
	}
	targets: {
		wasm32: {
			inputs: [app],
			output: Archive,
		},
	}

status_for_host : U64, U64 -> U64
status_for_host = |input, code| status(input, code)

after_for_host : U64, U64, U64 -> U64
after_for_host = |input, code, steps| after(input, code, steps)

accepts_for_host : U64 -> U64
accepts_for_host = |code| accepts(code)
