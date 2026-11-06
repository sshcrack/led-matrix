#!/usr/bin/env bash
# Sanitizer runtime options for LED_MATRIX_SANITIZE builds.
#
#   source scripts/sanitizers/env.sh            # in your shell
#   scripts/sanitizers/env.sh <command> [args]  # wrap one command
#
# Only third-party noise is suppressed (see the *.supp files next to this
# script). Existing *SAN_OPTIONS values from the environment are appended so
# they win over these defaults. Reports go to stderr and, when
# SANITIZER_LOG_DIR is set, to <dir>/<tool>.<pid>.
# Works when sourced from bash or zsh (zsh has no BASH_SOURCE).
if [ -n "${ZSH_VERSION:-}" ]; then
    eval '_lm_san_src="${(%):-%x}"'
else
    _lm_san_src="${BASH_SOURCE[0]}"
fi
_lm_san_dir="$(cd "$(dirname "${_lm_san_src}")" && pwd)"
_lm_log=""
if [[ -n "${SANITIZER_LOG_DIR:-}" ]]; then
    mkdir -p "$SANITIZER_LOG_DIR"
    _lm_log=":log_path=${SANITIZER_LOG_DIR}"
fi

# detect_leaks: LSan at exit. abort_on_error: SIGABRT so systemd-coredump /
# coredumpctl gets a core. halt_on_error=1 is the default for ASan.
# Plugins are dlclose()d on unload; sanitizer builds skip that, but keep
# fast_unwind_on_malloc off for readable allocation stacks through DSOs.
export ASAN_OPTIONS="detect_leaks=1:abort_on_error=1:strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:fast_unwind_on_malloc=0:suppressions=${_lm_san_dir}/asan.supp${_lm_log:+${_lm_log}/asan}${ASAN_OPTIONS:+:${ASAN_OPTIONS}}"
export LSAN_OPTIONS="suppressions=${_lm_san_dir}/lsan.supp:print_suppressions=0${LSAN_OPTIONS:+:${LSAN_OPTIONS}}"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1:suppressions=${_lm_san_dir}/ubsan.supp${_lm_log:+${_lm_log}/ubsan}${UBSAN_OPTIONS:+:${UBSAN_OPTIONS}}"
export TSAN_OPTIONS="second_deadlock_stack=1:history_size=4:halt_on_error=0:suppressions=${_lm_san_dir}/tsan.supp${_lm_log:+${_lm_log}/tsan}${TSAN_OPTIONS:+:${TSAN_OPTIONS}}"
# GraphicsMagick (statically linked) uses OpenMP; libgomp's barriers are
# invisible to TSan and produce false races. One OpenMP thread avoids them.
export OMP_NUM_THREADS="${OMP_NUM_THREADS:-1}"
unset _lm_san_dir _lm_san_src _lm_log

if [ -z "${ZSH_VERSION:-}" ] && [ "${BASH_SOURCE[0]}" = "$0" ] && [ $# -gt 0 ]; then
    exec "$@"
fi
