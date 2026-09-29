# vcpkg overlay ports

Ports here override the ones in the vcpkg registry. They are wired in by
`../vcpkg-configuration.json`, so a normal manifest-mode configure picks them up
with no extra flags.

## libssh2 — 1.11.1#4

The registry ships `libssh2` at `1.11.1#3`, whose only patches are build-system
fixes (`cmake-config.diff`, `pkgconfig.diff`). Three CVEs against 1.11.1 are
fixed upstream **only as commits on master** — libssh2 has published no release
since 1.11.1 (2024-10-16), and the vcpkg registry has no newer port. So neither
a baseline bump nor a version override can fix this; the fixes have to be
carried as patches.

This port is the registry port, `port-version` bumped to 4, plus:

| Patch | CVE | Upstream commit | Fixes |
|---|---|---|---|
| `patches/cve-2026-55200-transport-packet-length.patch` | CVE-2026-55200 (9.2) | `97acf3dfda80c91c3a8c9f2372546301d4a1a7a8` (PR #2052, 2026-06-12) | Unbounded `packet_length` in `ssh2_transport_read()` overflows `total_num`, so an undersized buffer is allocated for a much larger copy — pre-auth OOB write from a malicious server |
| `patches/cve-2026-58050-publickey-list-overflow.patch` | CVE-2026-58050 (8.3) | `34497525929b9a47f03dfb81887ac896202b7e12` (2026-06-28) | `num_attrs` from a publickey-subsystem response multiplied into an allocation size without a cap |
| `patches/cve-2026-55199-ext-info-loop.patch` | CVE-2026-55199 | `17626857d20b3c9a1addfa45979dadcee1cd84a4` (2026-04-15) | `SSH_MSG_EXT_INFO` handler ignores `_libssh2_get_string()` failures and keeps looping |

Note: public advisories cite the first fix as commit `7acf3df`. No such commit
exists in libssh2/libssh2 — the real SHA is `97acf3df…`, and the advisory text
appears to have lost the leading `9`.

### About the patches

`cve-2026-55199-ext-info-loop.patch` is the upstream commit verbatim.

The other two are rewritten against the 1.11.1 tag. The upstream commits sit on
master, which has since renamed several helpers (`_libssh2_ntohu32` →
`ssh2_ntohu32`, `LIBSSH2_ALLOC` → `SSH2_ALLOC`, `_libssh2_error` → `ssh2_err`),
so the upstream hunks fail to apply — the *context* has drifted, not the fix.
The inserted code is upstream's, line-for-line and identical in effect; each
patch header records the commit it came from and why it was rewritten.

All three were verified to apply cleanly, in the order listed, to a fresh
checkout of the `libssh2-1.11.1` tag — the exact source `vcpkg_from_github`
fetches. **They have not been compiled**: the toolchain is Windows-side.

### When to remove this

As soon as upstream publishes a release containing these fixes and the vcpkg
registry carries a port for it, delete the corresponding patch (or this whole
overlay) and move the baseline forward instead. Carrying patches against a
release tag is the fallback, not the goal.
