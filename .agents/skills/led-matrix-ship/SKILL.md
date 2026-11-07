---
name: led-matrix-ship
description: Ship finished changes in the led-matrix repo (sshcrack/led-matrix) as a commit, PR with screenshots, CI watch, and optionally merge, release and deploy to the Pi. Use when the user says to commit/PR/merge/release in led-matrix, or names a tier ("tier 1/2/3", "just PR it", "PR and merge", "ship and release"). Without a stated tier, ask which tier or default to tier 1.
---

# Ship led-matrix changes

Repo: `sshcrack/led-matrix` (default branch `master`, `gh` authenticated). Each tier includes the one before it.

| Tier | Does | Done when |
|---|---|---|
| 1 | commit, push, open PR with screenshots, watch checks | every check is green |
| 2 | tier 1, then merge the PR | PR is merged |
| 3 | tier 2, then release a new version and deploy it to the Pi (`ledmat`) | the GitHub release exists and `build_upload.sh` succeeded |

## Tier 1: PR and green checks

1. **Branch.** Never commit to `master`. If on `master`, create a branch first (`feat/...`, `fix/...`).
2. **Commit.** Stage only files this task changed (no `git add -A`; skip build dirs, `.env`, scratch files). Write a short imperative message that says why. Commit.
3. **Push and open the PR.** `git push -u origin HEAD`, then `gh pr create` with a body of what changed, why, and how it was verified. Link the PR to the thread if the harness provides a link tool.
4. **Screenshots.** For any visible change (UI, effect, scene, web app), capture screenshots or a short video and attach them with `scripts/pr_media.sh --pr <n> --comment <files...>` (uploads to the rolling `pr-media` release and prints embeddable markdown). See AGENTS.md for how to capture: emulator, `scripts/capture_desktop_preview.sh`, `scripts/render_scene_video.sh`, `scripts/preview_shadertoy_shader.sh`. For non-visual changes (CI, docs, refactors) skip this and say so in the PR body.
5. **Watch checks.** Use the harness PR-watch tool when available, otherwise `gh pr checks <n> --watch`. If a check fails, read the log (`gh run view --log-failed`), fix it, push, and watch again. Repeat until all checks are green. CI includes a repo-size check; large binaries or mass GIF regenerations fail it (see `scripts/check_repo_size.sh`).
6. **Stop** for tier 1. Report the PR URL and the check results.

## Tier 2: merge

After tier 1 is green:

1. Confirm the PR is mergeable (`gh pr view <n> --json mergeable,reviewDecision,statusCheckRollup`). If there are merge conflicts or requested changes, resolve them, push, and re-run tier 1 step 5.
2. Merge with a merge commit (`gh pr merge <n> --merge --delete-branch`); the repo history uses "Merge pull request #N". If the repo rules reject that method, use the one `gh` reports as allowed.
3. Verify with `gh pr view <n> --json state,mergedAt`: state must be `MERGED`.
4. If the PR is stacked on another PR's branch, merge the base PR first or tell the user instead of merging out of order.

## Tier 3: release and deploy

After tier 2, release from an up-to-date `master`. The workflow `.github/workflows/create-release.yml` runs on a pushed `v*` tag and requires the tag to match `set(VERSION x.y.z)` in `CMakeLists.txt`.

1. `git checkout master && git pull --ff-only`.
2. Pick the next version from the latest tag (`git tag --sort=-v:refname | head -1`). Default to a **patch** bump; use minor for a new feature the user called out, and ask before a major bump.
3. Edit the single `set(VERSION ...)` line in `CMakeLists.txt`, commit as `Prepare version X.Y.Z release` (matches history), and push to `master`. If `master` is protected, do this through a small PR and merge it like tier 2.
4. Tag and push: `git tag vX.Y.Z && git push origin vX.Y.Z`.
5. **Deploy to the Pi.** From the repo root on up-to-date `master` (the release commit), run `./build_upload.sh`. It cross-compiles the `.deb` (capped at 4 jobs, slow) and installs it on the `ledmat` ssh host. Run it as a background command and wait for it. The step is done when it exits 0; if it fails, report the output and do not retry blindly.
6. Watch the release workflow (`gh run list --workflow create-release.yml -L 1`, then `gh run watch <id>`). Fix and report if it fails; never delete or move a published tag without asking.
7. Verify `gh release view vX.Y.Z` exists, then report the release URL and the deploy result.

## Rules

- Do the tier the user named, no more. Tier 1 never merges; tier 2 never releases or deploys.
- Report faithfully: failing checks, skipped screenshots, or an unverified step get stated, not glossed over.
- Never force-push, bypass checks, or use `--admin` to merge unless the user explicitly says so.
