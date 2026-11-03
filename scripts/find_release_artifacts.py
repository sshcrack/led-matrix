#!/usr/bin/env python3
"""Find complete artifacts from successful CI with exactly this source tree."""
import io
import json
import os
from pathlib import Path
import re
import subprocess
import zipfile

REQUIRED = {"led-matrix-build", "desktop-linux-build", "desktop-windows-build", "build-provenance"}


def api(path):
    return subprocess.check_output(["gh", "api", path], timeout=20)


def matching_artifacts(manifest, artifacts, tree, version):
    available = {item["name"] for item in artifacts if not item.get("expired", False)}
    return (manifest.get("source_tree") == tree and manifest.get("version") == version
            and REQUIRED <= available)


def main():
    repo = os.environ["GITHUB_REPOSITORY"]
    tree = subprocess.check_output(["git", "rev-parse", "HEAD^{tree}"], text=True).strip()
    version = re.search(r"set\(VERSION ([0-9.]+)\)", Path("CMakeLists.txt").read_text()).group(1)
    result = ""
    try:
        runs = json.loads(api(f"repos/{repo}/actions/workflows/ci.yml/runs?status=success&per_page=20"))["workflow_runs"]
        for run in runs:
            if (run.get("head_repository") or {}).get("full_name") != repo:
                continue
            artifacts = json.loads(api(f"repos/{repo}/actions/runs/{run['id']}/artifacts?per_page=100"))["artifacts"]
            provenance = next((a for a in artifacts if a["name"] == "build-provenance" and not a["expired"]), None)
            if not provenance:
                continue
            try:
                archive = zipfile.ZipFile(io.BytesIO(api(f"repos/{repo}/actions/artifacts/{provenance['id']}/zip")))
                manifest = json.loads(archive.read("build-provenance.json"))
            except (subprocess.CalledProcessError, subprocess.TimeoutExpired, ValueError, KeyError, zipfile.BadZipFile):
                continue
            if matching_artifacts(manifest, artifacts, tree, version):
                result = str(run["id"])
                print(f"Reusing validated artifacts from CI run {result} (tree {tree})")
                break
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired, ValueError, KeyError) as error:
        print(f"Artifact lookup unavailable; building normally: {error}")
    with open(os.environ["GITHUB_OUTPUT"], "a") as output:
        output.write(f"reuse_run_id={result}\n")


if __name__ == "__main__":
    main()
