This overlay retains GraphicsMagick 1.3.45 and the build, dependency, copyright,
and patch logic from vcpkg baseline
`add3c85c5382ae789167e92c4d63cffb4e8a903d` (`ports/graphicsmagick`, port revision 1).

Only the source acquisition changes: the official signed release tarball on
SourceForge replaces Heptapod's generated archive, whose anti-bot HTML response
caused Windows CI to fail hash validation. The new archive has its own pinned
SHA-512; the rejected response's hash was not accepted.

Release provenance was verified using the detached `.tar.xz.asc` signature from
the same official release directory and the key published on GraphicsMagick's
security page. GnuPG reported a good signature with fingerprint
`EBDFDB21B020EE8FD151A88DE301047DE1198975`, matching the fingerprint documented at
<https://graphicsmagick.sourceforge.io/download.html>.

Release: <https://sourceforge.net/projects/graphicsmagick/files/graphicsmagick/1.3.45/>

SHA-512:
`2c12a24198a8ee1f86b4c43bf8903d129c891ec2c27ba22287dfc8a5f65c9223b2caa67a37b9a91b10b049ba8425e757738f5d92a9ae70abee76d323af9bd1a0`
