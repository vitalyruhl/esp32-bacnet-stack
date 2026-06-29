# Repository Settings

Current GitHub repository:

- URL: <https://github.com/vitalyruhl/esp32-bacnet-stack>
- Visibility: public
- Issues: enabled
- Wiki: disabled
- Discussions: not enabled
- GitHub Project: `ESP32 BACnet Stack` (#6)

## Security

- Dependabot alerts: enabled by repository API.
- Dependabot security updates: enabled by repository API.
- Dependabot version updates: configured for GitHub Actions in
  `.github/dependabot.yml`.
- Dependency graph: available for the public repository.
- Secret scanning: enabled by repository API.
- Secret scanning push protection: enabled by repository API.
- Secret scanning non-provider patterns and validity checks: not enabled.
- Code scanning is not enabled.
- CodeQL/code scanning should only be enabled when it is appropriate for the
  repository and does not introduce blocking noise unrelated to the supported
  C++/PlatformIO workflow.

## Branch Protection

The default branch is `main`.

Branch protection is configured for `main` through GitHub branch protection.

- Pull requests are required before merging.
- Required status checks are enabled with strict/up-to-date branch checks.
- Required status check: `build-and-test` from the `PlatformIO` workflow.
- Admin enforcement is disabled so the owner can recover repository access.
- Signed commits are not required.
- Linear history is not required.
- Force pushes are not allowed.
- Branch deletion is not allowed.
- Conversation resolution is not required.
- No repository ruleset is configured.
