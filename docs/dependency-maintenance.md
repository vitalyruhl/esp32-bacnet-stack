# Dependency Maintenance

Dependabot is configured for GitHub Actions in `.github/dependabot.yml`.

GitHub's official Dependabot supported ecosystem list does not include
PlatformIO, so `platformio.ini` platform and library dependency updates are
manual for now.

TODO: Recheck GitHub Dependabot ecosystem support before the first public
release and add PlatformIO automation if it becomes officially supported.
