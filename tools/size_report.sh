#!/usr/bin/env bash
#
# size_report.sh — measure binary-size impact of a single compile flag
# across PlatformIO embedded targets. Designed for ongoing efficiency
# audits across any microReticulum subsystem, not just Provisioning.
#
# How it works: builds the chosen PIO project twice, once with -D<FLAG>
# appended via PLATFORMIO_BUILD_FLAGS and once with -U<FLAG>. Reports
# per-section deltas plus an optional top-symbol breakdown for one env.
#
# Note: the comparison is meaningful only if the flag actually gates
# code that the chosen project's firmware references. The root project's
# main.cpp is a stub that never calls Reticulum::start(), so its embedded
# builds always link the same minimal set regardless of flag — hence the
# default project is an example that actually exercises the library.
#
# Usage:
#   tools/size_report.sh [--project <dir>] [--flag <NAME>]
#                        [--symbol-filter <regex>]
#                        [--detail <env>] [<env> ...]
#
# Defaults:
#   --project        examples/lora_transport
#   --flag           RNS_USE_PROVISIONING
#   --symbol-filter  RNS::<flag-derived-namespace>  (Provisioning by default)
#   envs             all embedded envs in the project's platformio.ini
#
# Built .elf files cache under .size_report/<env>/{on,off}.elf so follow-up
# --detail passes don't rebuild.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CACHE_DIR="$ROOT_DIR/.size_report"

PROJECT_DIR="$ROOT_DIR/examples/lora_transport"
FLAG="RNS_USE_PROVISIONING"
SYMBOL_FILTER=""
DETAIL_ENV=""
ENVS=()

while [[ $# -gt 0 ]]; do
	case "$1" in
		--project)       PROJECT_DIR="$2"; shift 2 ;;
		--project=*)     PROJECT_DIR="${1#--project=}"; shift ;;
		--flag)          FLAG="$2"; shift 2 ;;
		--flag=*)        FLAG="${1#--flag=}"; shift ;;
		--symbol-filter) SYMBOL_FILTER="$2"; shift 2 ;;
		--symbol-filter=*) SYMBOL_FILTER="${1#--symbol-filter=}"; shift ;;
		--detail)        DETAIL_ENV="$2"; shift 2 ;;
		--detail=*)      DETAIL_ENV="${1#--detail=}"; shift ;;
		-h|--help)
			sed -n '2,30p' "$0"; exit 0 ;;
		-*) echo "Unknown option: $1" >&2; exit 2 ;;
		*)  ENVS+=("$1"); shift ;;
	esac
done

# Default symbol filter: derive from flag name. RNS_USE_PROVISIONING →
# "RNS::Provisioning". RNS_USE_FS → no obvious mapping, so fall back to
# the flag's own name (which won't match symbols but is honest).
if [[ -z "$SYMBOL_FILTER" ]]; then
	case "$FLAG" in
		RNS_USE_PROVISIONING) SYMBOL_FILTER="RNS::Provisioning" ;;
		*) SYMBOL_FILTER="$FLAG" ;;
	esac
fi

# Resolve PROJECT_DIR (allow relative paths)
PROJECT_DIR="$(cd "$PROJECT_DIR" && pwd)"
[[ -f "$PROJECT_DIR/platformio.ini" ]] || { echo "no platformio.ini in $PROJECT_DIR" >&2; exit 2; }

# If no envs given, scrape them from the project's platformio.ini, skipping
# any native envs which don't have a flash-size question.
if [[ ${#ENVS[@]} -eq 0 ]]; then
	mapfile -t ENVS < <(
		grep -E '^\[env:[^]]+\]' "$PROJECT_DIR/platformio.ini" \
		| sed -E 's/^\[env:([^]]+)\]/\1/' \
		| grep -vE '^(native|embedded)'
	)
fi

# Toolchain prefix per env (ESP32 family vs nRF52). Adjust here if new boards
# need a different toolchain.
toolchain_prefix() {
	case "$1" in
		wiscore_rak4631|lilygo-techo|nrf52840-dk-adafruit|wiscore_rak4631_dap) echo "arm-none-eabi" ;;
		ttgo-lora32-v21|ttgo-t-beam|lilygo_tbeam_supreme|heltec-lora32-v3|heltec-lora32-v4|heltec_wifi_lora_32_V4|embedded)
			echo "xtensa-esp32-elf" ;;
		*) echo "xtensa-esp32-elf" ;;
	esac
}

toolchain_tool() {
	local prefix="$1" tool="$2" found
	if command -v "${prefix}-${tool}" >/dev/null 2>&1; then
		command -v "${prefix}-${tool}"; return 0
	fi
	found="$(find "$HOME/.platformio/packages" -name "${prefix}-${tool}" -type f 2>/dev/null | head -n1 || true)"
	if [[ -n "$found" ]]; then echo "$found"; return 0; fi
	return 1
}

# Build env in the chosen project, copy .elf to cache.
# Args: env, label (on|off), compile flag override (e.g. -DFOO or -UFOO).
build_env() {
	local env="$1" label="$2" flag="$3"
	local out_dir="$CACHE_DIR/$env"
	mkdir -p "$out_dir"
	local out_elf="$out_dir/$label.elf"

	echo "  building $env [$label]..." >&2
	(
		cd "$PROJECT_DIR"
		# Fullclean so the flag change actually re-compiles affected TUs.
		pio run -e "$env" -t fullclean >/dev/null 2>&1 || true
		PLATFORMIO_BUILD_FLAGS="$flag" pio run -e "$env" >/dev/null 2>&1
	)

	local src_elf="$PROJECT_DIR/.pio/build/$env/firmware.elf"
	[[ -f "$src_elf" ]] || src_elf="$(find "$PROJECT_DIR/.pio/build/$env" -maxdepth 1 -name '*.elf' -type f 2>/dev/null | head -n1 || true)"
	[[ -f "$src_elf" ]] || { echo "  no .elf for $env [$label]" >&2; return 1; }
	cp "$src_elf" "$out_elf"
}

# Sum allocated sections from `size --format=sysv`, excluding alignment-only
# dummy sections that ESP32 builds use to pad to fixed boundaries (those
# absorb the real code-size delta and would otherwise show zero change).
section_sizes() {
	local elf="$1" size_bin="$2"
	"$size_bin" --format=sysv "$elf" 2>/dev/null | awk '
		# Skip dummy padding sections (esp32) — they pad to fixed boundaries
		# and the linker shifts bytes between them and real sections.
		/dummy/ { next }
		# ARM-style: bare .text / .rodata / .data / .bss
		# ESP32-style: .flash.text, .iram0.text, .iram0.vectors,
		#              .flash.rodata, .dram0.data, .dram0.bss
		/^\.text|\.flash\.text|\.iram0\.text|\.iram0\.vectors/   { text   += $2 }
		/^\.rodata|\.flash\.rodata|\.flash\.appdesc/             { rodata += $2 }
		/^\.data|\.dram0\.data|\.iram0\.data/                    { data   += $2 }
		/^\.bss|\.dram0\.bss|\.iram0\.bss/                       { bss    += $2 }
		END { printf "%d %d %d %d\n", text+0, rodata+0, data+0, bss+0 }
	'
}

run_detail() {
	local env="$1"
	local prefix; prefix="$(toolchain_prefix "$env")"
	local nm_bin
	if ! nm_bin="$(toolchain_tool "$prefix" nm)"; then
		echo "skip detail: no ${prefix}-nm" >&2; return
	fi
	local on_elf="$CACHE_DIR/$env/on.elf"
	[[ -f "$on_elf" ]] || { echo "skip detail: no $on_elf" >&2; return; }

	echo
	echo "=== Top 30 symbols matching '$SYMBOL_FILTER' by size ($env) ==="
	# nm --size-sort orders ascending; tail -n30 then descending sort.
	# Format from nm: "<addr_hex> <size_hex> <type> <name>"
	"$nm_bin" --size-sort --print-size --demangle "$on_elf" 2>/dev/null \
		| grep "$SYMBOL_FILTER" \
		| tail -n30 \
		| while read -r addr size_hex type rest; do
			printf "%6d  %s  %s\n" "$((16#$size_hex))" "$type" "$rest"
		done \
		| sort -nr | head -n30

	# Total filtered footprint as a sanity check against the table delta.
	local total=0
	while read -r addr size_hex type rest; do
		total=$((total + 16#$size_hex))
	done < <("$nm_bin" --size-sort --print-size --demangle "$on_elf" 2>/dev/null | grep "$SYMBOL_FILTER")
	echo
	echo "Sum of all '$SYMBOL_FILTER' symbol sizes: $total bytes"
}

mkdir -p "$CACHE_DIR"

echo "Project: $PROJECT_DIR"
echo "Flag:    $FLAG"
echo "Envs:    ${ENVS[*]}"
echo
echo "=== Size impact ($FLAG on vs off) ==="
printf "%-25s %10s %10s %10s %10s %12s\n" "Target" "text Δ" "rodata Δ" "data Δ" "bss Δ" "total Δ"
printf "%-25s %10s %10s %10s %10s %12s\n" "------" "------" "--------" "------" "-----" "-------"

for env in "${ENVS[@]}"; do
	prefix="$(toolchain_prefix "$env")"
	if ! size_bin="$(toolchain_tool "$prefix" size)"; then
		printf "%-25s %s\n" "$env" "skip: no ${prefix}-size"
		continue
	fi

	if ! build_env "$env" "on"  "-D${FLAG}"; then
		printf "%-25s %s\n" "$env" "skip: build failed (on)"
		continue
	fi
	if ! build_env "$env" "off" "-U${FLAG}"; then
		printf "%-25s %s\n" "$env" "skip: build failed (off)"
		continue
	fi

	read -r t1 r1 d1 b1 <<<"$(section_sizes "$CACHE_DIR/$env/on.elf"  "$size_bin")"
	read -r t0 r0 d0 b0 <<<"$(section_sizes "$CACHE_DIR/$env/off.elf" "$size_bin")"

	dt=$((t1-t0)); dr=$((r1-r0)); dd=$((d1-d0)); db=$((b1-b0))
	dtot=$((dt+dr+dd+db))
	printf "%-25s %+10d %+10d %+10d %+10d %+12d\n" "$env" "$dt" "$dr" "$dd" "$db" "$dtot"
done

if [[ -n "$DETAIL_ENV" ]]; then
	run_detail "$DETAIL_ENV"
fi
