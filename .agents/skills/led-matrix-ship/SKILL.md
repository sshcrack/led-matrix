---
name: led-matrix-ship
description: Ship finished changes in the led-matrix repo (sshcrack/led-matrix) as a commit, PR with screenshots, CI watch, and optionally merge, release and deploy to the Pi. Use when the user says to commit/PR/merge/release in led-matrix, or names a tier ("tier 1/2/3", "just PR it", "PR and merge", "ship and release"). Without a stated tier, ask which tier or default to tier 1. "With review" adds an independent review loop before the merge.
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
5. **Scene preview GIFs.** If the change adds or alters any matrix scene (look, behaviour, properties that change the default render), regenerate its preview GIF, and also generate one for every scene that has none yet: `scripts/fetch_scene_previews.sh --if-missing`, list the scenes from `emulator_build/install/bin/preview_gen --dump-manifest` that are previewable but have no `scene_previews/<name>.gif` (`:` in a name becomes `_`, e.g. `shader_scenic_aurora.gif`), then `scripts/generate_scene_previews.sh --scenes <changed,new,missing>`. Look at the new GIFs before going on. Never commit them (they are git-ignored and blocked by the repo-size check); they are published in tier 2 after the merge. Note in the PR body which previews were regenerated.
6. **Watch checks.** Use the harness PR-watch tool when available, otherwise `gh pr checks <n> --watch`. If a check fails, read the log (`gh run view --log-failed`), fix it, push, and watch again. Repeat until all checks are green. CI includes a repo-size check; large binaries or mass GIF regenerations fail it (see `scripts/check_repo_size.sh`).
7. **Stop** for tier 1. Report the PR URL and the check results.

## With review

When the user adds "with review" to a tier (e.g. "tier 2 with review"), run this loop after tier 1 step 3 (the PR exists) and before any merge:

1. **Review.** Start a fresh, independent review of the whole PR with `delegate_task` (role `review`). Use the reviewer the user names, default GPT 6.1 Sol (`{providerInstanceId: "codex", model: "gpt-6.1-sol"}`). Every round is a new task, never a follow-up message to an earlier one. Keep the brief neutral: what the PR does, its key files, what kinds of defects to look for, and "trace each call path fully before reporting". Never mention the round number or what changed since the last round. List design decisions the user has settled and behaviour that is intended, so they are not re-raised. Ask for findings with file:line, severity and a concrete failure scenario, no edits.
2. **Triage.** Verify each finding yourself by reading the code before acting on it. Fix only relevant findings: defects a user can realistically hit in normal or likely use, such as crashes, data loss, a broken install or update, security problems, or wrong behaviour. Do not fix:
   - findings that are already handled (trace them: an exception may be caught further up, or the state after a failure may still work for the user)
   - rare compound failures (a second failure during recovery from a first, a full disk mid-copy, …)
   - theoretical, style-only or out-of-scope findings
   Note each skipped finding with a one-line reason.
3. **Fix and verify.** Fix the relevant findings, build, test the change (say what you could not test), commit and push.
4. **Repeat** from step 1 until a round has no relevant findings, or the user says to stop. Then continue with tier 1 step 6 (green checks) and the rest of the tier.
5. **Report** the number of rounds, what was fixed, and the skipped findings with their reasons.

## Tier 2: merge

After tier 1 is green:

1. Confirm the PR is mergeable (`gh pr view <n> --json mergeable,reviewDecision,statusCheckRollup`). If there are merge conflicts or requested changes, resolve them, push, and re-run tier 1 step 6.
2. Merge with a merge commit (`gh pr merge <n> --merge --delete-branch`); the repo history uses "Merge pull request #N". If the repo rules reject that method, use the one `gh` reports as allowed.
3. Verify with `gh pr view <n> --json state,mergedAt`: state must be `MERGED`.
4. **Publish scene previews** regenerated in tier 1 step 5: `scripts/publish_scene_previews.sh` (it uploads only changed GIFs and deletes assets missing from `scene_previews/`, so the directory must hold the full fetched set plus the new ones). Skip when no scene changed.
5. If the PR is stacked on another PR's branch, merge the base PR first or tell the user instead of merging out of order.

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
