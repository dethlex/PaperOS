# PlatformIO pre-build hook: inject the current git tag/description as the
# PAPEROS_VERSION compile-time string, shown in Settings → About → Version.
#
# `git describe --tags --dirty --always` yields:
#   - exactly on a tag:        v0.10.0
#   - N commits past a tag:    v0.10.0-3-gabc1234
#   - uncommitted changes:     v0.10.0-3-gabc1234-dirty
#   - no tags / not a repo:    abc1234  (or "dev" if git is unavailable)
#
# Wired in platformio.ini via `extra_scripts = pre:tools/version_flag.py`.
import subprocess

Import("env")  # noqa: F821  (provided by the PlatformIO/SCons build runtime)


def git_version():
    try:
        out = subprocess.check_output(
            ["git", "describe", "--tags", "--dirty", "--always"],
            stderr=subprocess.DEVNULL,
        )
        v = out.decode("utf-8", "replace").strip()
        return v if v else "dev"
    except Exception:
        return "dev"


env.Append(CPPDEFINES=[("PAPEROS_VERSION", env.StringifyMacro(git_version()))])
