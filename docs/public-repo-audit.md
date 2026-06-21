# Public Repo Audit

Run this checklist before changing repository visibility or cutting a public v1 release.

## Public-path content

Audit `README.md`, `docs/quickstart.md`, public module docs, configuration examples, firmware flashing docs, troubleshooting docs, `SECURITY.md`, and `LICENSE` for:

- secrets, passwords, API keys, token secrets, private keys, and `.env` content;
- private IPs, hostnames, usernames, absolute maintainer paths, and live device identifiers presented as public workflow;
- examples that require maintainer-only lab state;
- references to unsupported v1 features as if they were supported;
- admin/write credentials where read-only credentials are expected.

## Maintainer Operations Notes

Files under `docs/operations/` may remain as evidence only when they are clearly marked as Maintainer Operations Notes and are not the public install path.

## Generated and local artifacts

Confirm `.gitignore` excludes private firmware config, PlatformIO output, logs, captures, local config files, caches, and `.tmp-tests` output.

## Safe placeholders

Prefer public placeholders such as:

- `pve.example`
- `module-llm.local`
- `REPLACE_WITH_READ_ONLY_TOKEN_SECRET`
- `/dev/ttyACM0` as an example serial device only

## Release decision

Record the audit result in the release issue or release notes. Any unresolved secret, license, security, or private-assumption finding blocks Public Release Readiness.
