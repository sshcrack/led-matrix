#!/usr/bin/env python3
"""
PR Auto-Review Loop — automated TUI that iteratively reviews a PR vs master,
spawns subagent-driven fixes, commits & pushes, then reviews again.
Exits when a fresh review finds zero bugs.
"""

from __future__ import annotations

import argparse
import curses
import json
import os
import re
import signal
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

REPO_DIR = Path(os.environ.get("REPO_DIR", os.getcwd()))
RESULT_FILE = Path("/tmp/pr-autoreview-result.json")

CURSES_COLORS = {
    "green": 1,
    "yellow": 2,
    "red": 3,
    "cyan": 4,
    "magenta": 5,
}


def strip_ansi(text: str) -> str:
    return re.sub(r"\x1b\[[0-9;]*[a-zA-Z]", "", text)


class TUI:
    def __init__(self, stdscr: curses.window) -> None:
        self.stdscr = stdscr
        self.logs: list[tuple[str, str, int]] = []
        self.iteration = 0
        self.total_bugs = 0
        self.bugs_last_review: list[dict] = []
        self.status = "Initializing..."
        self.running = True
        self.start_ts = time.time()
        self._init_colors()
        self.stdscr.nodelay(True)

    def _init_colors(self) -> None:
        curses.start_color()
        curses.use_default_colors()
        curses.init_pair(CURSES_COLORS["green"], curses.COLOR_GREEN, -1)
        curses.init_pair(CURSES_COLORS["yellow"], curses.COLOR_YELLOW, -1)
        curses.init_pair(CURSES_COLORS["red"], curses.COLOR_RED, -1)
        curses.init_pair(CURSES_COLORS["cyan"], curses.COLOR_CYAN, -1)
        curses.init_pair(CURSES_COLORS["magenta"], curses.COLOR_MAGENTA, -1)

    def log(self, msg: str, color: str = "") -> None:
        ts = datetime.now().strftime("%H:%M:%S")
        cp = CURSES_COLORS.get(color, 0)
        self.logs.append((ts, msg, cp))
        self.draw()

    def draw(self) -> None:
        self.stdscr.erase()
        h, w = self.stdscr.getmaxyx()
        if h < 6 or w < 30:
            return

        elapsed = int(time.time() - self.start_ts)
        header = (
            f" PR Auto-Review  |  Iter {self.iteration}  |  "
            f"Bugs found: {self.total_bugs}  |  "
            f"{elapsed // 60}:{elapsed % 60:02d}  |  "
            f"{'●' if self.total_bugs == 0 else '◆'} "
        )
        if len(header) > w - 1:
            header = header[: w - 2]
        self.stdscr.addstr(0, 0, " " * (w - 1), curses.A_REVERSE)
        self.stdscr.addstr(0, 0, header, curses.A_REVERSE)

        max_log = h - 3
        visible = self.logs[-max_log:]
        for i, (ts, msg, cp) in enumerate(visible):
            line = f" {ts} {msg}"
            if len(line) > w - 1:
                line = line[: w - 2]
            try:
                self.stdscr.addstr(i + 1, 0, line, cp)
            except curses.error:
                pass

        status_text = (
            f" Status: {self.status}  |  Ctrl+C stop  |  "
            f"Last review: {len(self.bugs_last_review)} bug(s)  |  "
            f"Logs: {len(self.logs)} "
        )
        self.stdscr.addstr(h - 1, 0, " " * (w - 1), curses.A_REVERSE)
        if len(status_text) > w - 1:
            status_text = status_text[: w - 2]
        self.stdscr.addstr(h - 1, 0, status_text, curses.A_REVERSE)
        self.stdscr.refresh()

    def update_status(self, status: str, bugs: int = 0, bugs_list: list[dict] | None = None) -> None:
        self.status = status
        if bugs:
            self.total_bugs = bugs
        if bugs_list is not None:
            self.bugs_last_review = bugs_list
        self.draw()


def run_opencode(
    prompt: str,
    model: str | None,
    label: str,
    tui: TUI,
    timeout: int = 600,
) -> tuple[bool, str]:
    cmd = ["opencode", "run", "--dangerously-skip-permissions"]
    if model:
        cmd += ["-m", model]
    cmd.append(prompt)
    model_label = model or "(default)"
    tui.log(f"Starting opencode ({label}) — {model_label}", "cyan")
    tui.update_status(f"{label}...")
    tui.draw()

    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=False,
            bufsize=0,
            cwd=str(REPO_DIR),
        )
    except FileNotFoundError:
        tui.log("opencode binary not found in PATH", "red")
        tui.update_status("opencode not found — abort")
        return False, ""

    assert proc.stdout is not None
    os.set_blocking(proc.stdout.fileno(), False)

    buf = b""
    accumulated: list[str] = []
    start = time.time()
    interrupted = False

    while True:
        rc = proc.poll()

        try:
            chunk = proc.stdout.read(65536)  # type: ignore[union-attr]
            if chunk:
                buf += chunk
                while b"\n" in buf:
                    raw_line, buf = buf.split(b"\n", 1)
                    text = raw_line.decode("utf-8", errors="replace").rstrip()
                    clean = strip_ansi(text)
                    if clean:
                        accumulated.append(clean)
                        tui.log(clean, "cyan")
        except (BlockingIOError, ValueError):
            pass

        if rc is not None:
            if buf:
                text = buf.decode("utf-8", errors="replace").rstrip()
                clean = strip_ansi(text)
                if clean:
                    accumulated.append(clean)
                    tui.log(clean, "cyan")
            break

        if time.time() - start > timeout:
            proc.terminate()
            tui.log(f"TIMEOUT after {timeout}s", "red")
            tui.update_status(f"Timeout ({label})")
            return False, "\n".join(accumulated)

        key = tui.stdscr.getch()
        if key == 3:
            proc.terminate()
            interrupted = True
            tui.log("Interrupted — Ctrl+C", "red")
            tui.update_status(f"Interrupted ({label})")
            break

        tui.draw()
        time.sleep(0.05)

    proc.wait(5)
    combined = "\n".join(accumulated)

    if rc == 0 and not interrupted:
        tui.log(f"Complete ({label})", "green")
        return True, combined
    else:
        tui.log(f"Exit code: {rc} ({label})", "red" if rc != 0 else "yellow")
        tui.update_status(f"Done ({label})")
        return True, combined


def parse_bugs(output: str, tui: TUI) -> list[dict]:
    if RESULT_FILE.exists():
        try:
            raw = RESULT_FILE.read_text()
            RESULT_FILE.unlink(missing_ok=True)
            data = json.loads(raw)
            bugs = data if isinstance(data, list) else data.get("bugs", [])
            n = len(bugs)
            tui.log(f"Parsed result file: {n} bug(s)", "cyan")
            return bugs
        except (json.JSONDecodeError, Exception) as exc:
            tui.log(f"Result file parse error: {exc}", "yellow")

    patterns = [
        r"<REVIEW_RESULT>\s*(\{.*?\})\s*</REVIEW_RESULT>",
        r"```json\s*(\{.*?\})\s*```",
        r"(\{\s*\"bugs_found\".*?\})",
    ]
    for pat in patterns:
        m = re.search(pat, output, re.DOTALL)
        if m:
            try:
                data = json.loads(m.group(1))
                bugs = data if isinstance(data, list) else data.get("bugs", [])
                n = len(bugs)
                tui.log(f"Parsed from output marker: {n} bug(s)", "cyan")
                return bugs
            except (json.JSONDecodeError, Exception):
                continue

    output_clean = strip_ansi(output)
    if "NO_BUGS_FOUND" in output_clean.upper() or "no bugs found" in output_clean.lower():
        tui.log("Output says no bugs found", "green")
        return []

    if re.search(r"bugs?[\s_]found[\s:]*0", output_clean, re.IGNORECASE):
        tui.log("Output says 0 bugs found", "green")
        return []

    if re.search(r"bugs?[\s_]found[\s:]*[1-9]", output_clean, re.IGNORECASE):
        tui.log("Bugs reported in output but couldn't parse structured list", "yellow")
        return [{"description": "Unparseable bug report — see output"}]

    return []


def check_git_state(tui: TUI, base_branch: str) -> bool:
    try:
        r = subprocess.run(
            ["git", "diff", f"{base_branch}...HEAD", "--stat"],
            capture_output=True, text=True, cwd=str(REPO_DIR),
        )
        if r.returncode != 0:
            tui.log(f"Git diff failed: {r.stderr.strip()}", "red")
            return False
        lines = r.stdout.strip().split("\n")
        tui.log(f"Diff against {base_branch}: {len(lines)} file(s) changed", "cyan")
        return True
    except FileNotFoundError:
        tui.log("git not found in PATH", "red")
        return False


def run_loop(tui: TUI, args: argparse.Namespace) -> None:
    model = args.model
    base = args.base_branch
    max_iters = args.max_iters
    enable_push = not args.no_push

    tui.log("═══ PR Auto-Review Loop ═══", "magenta")
    tui.log(f"Model: {model}", "cyan")
    tui.log(f"Base:  {base}", "cyan")
    tui.log(f"Push:  {enable_push}", "cyan")
    tui.log(f"Dir:   {REPO_DIR}", "cyan")
    tui.log("")

    if not check_git_state(tui, base):
        tui.log("Git state check failed — aborting", "red")
        return

    while tui.running and tui.iteration < max_iters:
        tui.iteration += 1
        tui.log("")
        tui.log(f"── Iteration #{tui.iteration} ──", "magenta")

        # ── Phase 1: Review ──────────────────────────────────
        review_prompt = (
            f"You are reviewing a large PR against `{base}`. "
            f"Only look at the total diff, not individual commits.\n\n"
            f"Spin up subagents to review each area of change. "
            f"Go deep: look for bugs, UB, memory errors, race conditions, "
            f"logic errors, API misuse, wrong defaults, missing error handling.\n\n"
            f"Each subagent reports back to you. SPIN THEM UP IN PARALLEL! Also if you need full git diffs, use /usr/bin/git. Then aggregate all findings.\n\n"
            f"IMPORTANT — write the result to {RESULT_FILE} as a JSON file:\n"
            f'{RESULT_FILE} ← a JSON array of bug objects:\n'
            f'[{{"file":"<path>", "line":<int>, '
            f'"description":"<what>", "severity":"high|medium|low", '
            f'"category":"<bug|regression|smell>"}}]\n\n'
            f"If truly no bugs exist, write an empty array.\n\n"
            f"Do NOT fix anything. Only report.\n"
            f"Double-check your work — be thorough."
        )

        ok, output = run_opencode(review_prompt, model, "Review", tui, timeout=args.review_timeout)
        if not ok:
            tui.log("Review phase failed — retrying", "yellow")
            tui.iteration -= 1
            time.sleep(2)
            continue

        bugs = parse_bugs(output, tui)
        tui.update_status("Review done", bugs_list=bugs)

        for b in bugs:
            sev = b.get("severity", "?")
            fp = b.get("file", "?")
            ln = b.get("line", "")
            desc = b.get("description", "?")
            loc = f":{ln}" if ln else ""
            tui.log(f"  [{sev}] {fp}{loc} — {desc}", "yellow")

        if not bugs:
            tui.log("  No bugs found — PR is clean!", "green")
            tui.update_status("Clean — exiting")
            break

        tui.total_bugs += len(bugs)

        # ── Phase 2: Fix ─────────────────────────────────────
        bugs_json = json.dumps(bugs, indent=2)

        fix_steps = (
            f"1. Run: git add -A\n"
            f"2. Run: git commit -m \"fix: address review findings — iteration {tui.iteration}\"\n"
        )
        if enable_push:
            fix_steps += "3. Run: git push\n"
        else:
            fix_steps += "3. [--no-push active — skip git push]\n"
        fix_prompt = (
            f"Fix every bug listed below. These were found in the PR diff "
            f"against `{base}`.\n\n"
            f"Bug reports:\n{bugs_json}\n\n"
            f"Fix every issue above. After all fixes are applied:\n"
            f"{fix_steps}"
            f"IMPORTANT: verify every single bug above is actually resolved. "
            f"Do not skip any."
        )

        ok, _ = run_opencode(fix_prompt, model, "Fix", tui, timeout=args.fix_timeout)
        if not ok:
            tui.log("Fix phase failed — retrying iteration", "yellow")
            continue

        tui.log(f"Iteration #{tui.iteration} done — reviewing again...", "green")

    # ── Phase 3: CI ──────────────────────────────────────────
    if not args.skip_ci and tui.running:
        tui.log("")
        tui.log("── CI Phase ──", "magenta")

        ci_prompt = (
            "Handle CI for this PR using the `gh` CLI. "
            "The PR is the one associated with the current branch.\n\n"
            "Loop:\n"
            "1. Get the PR number: gh pr view --json number -q .number\n"
            "2. Wait for all CI checks to complete (poll every 30s with "
            "gh pr checks <pr> --json state,conclusion,name)\n"
            "3. If any checks failed or were cancelled:\n"
            "   - Examine what failed and understand the root cause\n"
            "   - Fix the issues\n"
            "   - git add -A && git commit -m \"ci: fix CI failures\" && git push\n"
            "   - Push is REQUIRED to re-trigger CI. Go back to step 2.\n"
            "4. If all checks pass: exit — you're done.\n\n"
            "Be thorough — actually read CI logs/output to understand failures. "
            "Do not blindly retry."
        )

        ok, _ = run_opencode(ci_prompt, model, "CI", tui, timeout=args.ci_timeout)
        if ok:
            tui.log("CI phase complete — all checks passing", "green")
        else:
            tui.log("CI phase did not complete successfully", "yellow")

    # ── Final summary ────────────────────────────────────────
    if tui.iteration >= max_iters:
        tui.log(f"Reached max iterations ({max_iters})", "yellow")
    tui.log("═══ All done ═══", "magenta")
    tui.log(f"Iterations: {tui.iteration}", "cyan")
    tui.log(f"Total bugs reported: {tui.total_bugs}", "cyan")
    tui.update_status("Done — press any key to exit")

    tui.stdscr.nodelay(False)
    tui.stdscr.getch()


def entry(stdscr: curses.window, args: argparse.Namespace) -> None:
    # SIGINT left as default — KeyboardInterrupt handled by outer try/except
    tui = TUI(stdscr)
    try:
        run_loop(tui, args)
    except KeyboardInterrupt:
        pass


def main() -> None:
    parser = argparse.ArgumentParser(
        description="PR Auto-Review Loop — automated review/fix TUI",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Example:\n"
            "  pr-autoreview-loop.py -b master --max-iters 20\n"
            "\n"
            "Stages:\n"
            "  1. Review-Fix loop — diff vs base, find & fix bugs until clean\n"
            "  2. CI loop — wait for GitHub CI, fix failures until all green\n"
        ),
    )
    parser.add_argument("-m", "--model", default=None,
                        help="Model to use (default: opencode's configured default)")
    parser.add_argument("-b", "--base-branch", default="master",
                        help="Base branch for diff (default: master)")
    parser.add_argument("--max-iters", type=int, default=50,
                        help="Max iterations (default: 50)")
    parser.add_argument("--review-timeout", type=int, default=600,
                        help="Review phase timeout in seconds (default: 600)")
    parser.add_argument("--fix-timeout", type=int, default=600,
                        help="Fix phase timeout in seconds (default: 600)")
    parser.add_argument("--ci-timeout", type=int, default=3600,
                        help="CI phase timeout in seconds (default: 3600)")
    parser.add_argument("--skip-ci", action="store_true",
                        help="Skip the CI wait-and-fix phase")
    parser.add_argument("--no-push", action="store_true",
                        help="Skip git push after fixes")
    args = parser.parse_args()

    curses.wrapper(entry, args)


if __name__ == "__main__":
    main()
