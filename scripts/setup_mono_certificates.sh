#!/usr/bin/env bash
# Ubuntu 26.04 dropped ca-certificates-mono, including the cert-sync tool.
set -euo pipefail

privileged=()
if ((EUID != 0)); then privileged=(sudo -n); fi
"${privileged[@]}" apt-get install -y --no-install-recommends mono-devel ca-certificates
"${privileged[@]}" update-ca-certificates

if command -v cert-sync >/dev/null 2>&1; then
    synchronizer=(cert-sync)
else
    # Build Mono's MIT-licensed tool against the installed runtime. Pin and
    # verify its source rather than installing packages from another distro.
    temporary=$(mktemp -d "${RUNNER_TEMP:-${TMPDIR:-/tmp}}/mono-cert-sync.XXXXXX")
    trap 'rm -rf "$temporary"' EXIT
    curl -fsSL --connect-timeout 10 --max-time 60 --retry 2 \
        https://raw.githubusercontent.com/mono/mono/mono-6.12.0.206/mcs/tools/security/cert-sync.cs \
        -o "$temporary/cert-sync.cs"
    echo "29e32fc9a1bda7d2979ade87d85575a2142f9abe1de9074b067f8ca8ba994a64  $temporary/cert-sync.cs" | sha256sum -c -
    # Only the banner depends on Mono's generated build metadata helper.
    sed -i 's/new AssemblyInfo ().ToString ()/"Mono Certificate Store Sync"/' "$temporary/cert-sync.cs"
    mcs -r:Mono.Security "$temporary/cert-sync.cs" -out:"$temporary/cert-sync.exe"
    synchronizer=(mono "$temporary/cert-sync.exe")
fi

"${privileged[@]}" "${synchronizer[@]}" /etc/ssl/certs/ca-certificates.crt
"${synchronizer[@]}" --user /etc/ssl/certs/ca-certificates.crt
