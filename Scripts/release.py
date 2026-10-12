"""Builds a release of the game and drafts it on GitHub.

    python Scripts/release.py 0.8.0                      # both Shipping packages, their zips, a draft release
    python Scripts/release.py 0.8.0 --archives-only      # the zips alone, to look at before drafting
    python Scripts/release.py 0.8.0 --platforms Windows  # one platform

The release is built from HEAD, which must be clean, already pushed to GitHub (the draft points at it) and carry the
version in Config/DefaultGame.ini's ProjectVersion. Each platform's Shipping package is built, run and archived by
package.py; its zips are then named by the version in out/releases: Weltall-<version>-<Platform>.zip for players and
...-symbols.zip, which stays with whoever builds releases (it turns a player's crash report into source lines). The
notes go beside them as Weltall-<version>-notes.md.

Without --archives-only a draft release is made with the GitHub CLI (gh, logged in), the players' zips attached and
the notes as its text, and the commit is tagged v<version> here. Nothing is published: the draft waits on the
release page to be read, edited and published by hand, which creates the tag on GitHub. GitHub attaches the tag's
source code to the release by itself.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
RELEASES = REPO / "out" / "releases"
PLATFORMS = ["Windows", "Linux"]
VERSION = re.compile(r"\d+\.\d+\.\d+")
PROJECT_VERSION = re.compile(r"^ProjectVersion=(.*)$", re.MULTILINE)

NOTES = """\
Weltall {version}, a native port of *Xenogears* (PlayStation, 1998). It contains no game data: it runs only with
your own copy of the game, the USA release's two discs.

## Playing

1. Download the archive for your system below and unpack it anywhere.
2. Put your two disc images in its `Games` folder: BIN/CUE, or CHD made from it, is best (the movies keep their
   sound); ISO/IMG works, with silent movies.
3. Run `Weltall.exe` on Windows 10 or 11 (64-bit, DirectX 12), or `Weltall.sh` on Linux (64-bit, Vulkan).

Settings and saves are kept in `%LOCALAPPDATA%\\Weltall` on Windows, and on Linux in `~/.config/Weltall` and
`~/.local/share/Weltall/Saves`. Saves from a PlayStation memory card image can be copied in on the Options' Saves page.
The README has the controls and the rest.

## What to expect

This is the first release, and the whole game plays, both discs, every part checked against the original's own code.
So far it has been played by hand for over ten hours without a hang, and that playthrough is still going; automated
play covers the rest of the story: expect to play it through from start to finish. The Linux build has only been run inside
Windows' own Linux subsystem so far, drawn by the processor rather than a graphics card: on a Linux machine, please
report whatever goes wrong.

If the game stops, the screen says why and names its log file: please attach that file to an issue, with a few words
on what you were doing. If it crashes instead, zip and attach the newest folder in
`%LOCALAPPDATA%\\Weltall\\Saved\\Crashes` (on Linux, `~/.config/Epic/Weltall/Saved/Crashes`).
Reported issues are being fixed as they come in.

Not in this release: the Japanese discs, playing the second disc without the first one in the `Games` folder, a
message log, auto-advancing text, a quick save, analog movement, and the Battling arena's El-Fenrir and analog
steering.
"""


def unwrapped(text):
    """Text with each paragraph and list item on one line: GitHub shows a release's notes with every line break kept."""
    lines = []
    for line in text.splitlines():
        joins = lines and lines[-1].strip() and line.strip() and not line.startswith("#") and not lines[-1].startswith("#")
        if joins and not re.match(r"\d+\. ", line):
            lines[-1] += " " + line.strip()
        else:
            lines.append(line)
    return "\n".join(lines) + "\n"


def git(*arguments):
    return subprocess.run(["git", "-C", str(REPO), *arguments], capture_output=True, text=True, check=True).stdout.strip()


def refuse(message):
    sys.exit(f"release: {message}")


def check_source(version):
    """Refuses unless HEAD is clean, carries the version and has no release tag yet."""
    if git("status", "--porcelain"):
        refuse("the working tree has changes: commit them first")
    ini = (REPO / "Config" / "DefaultGame.ini").read_text(encoding="utf-8")
    found = PROJECT_VERSION.search(ini)
    if found is None or found.group(1).strip() != version:
        refuse(f"Config/DefaultGame.ini's ProjectVersion is {found.group(1).strip() if found else 'missing'}, not {version}")
    if subprocess.run(["git", "-C", str(REPO), "rev-parse", "-q", "--verify", f"refs/tags/v{version}"],
                      capture_output=True).returncode == 0:
        refuse(f"v{version} is tagged already")


def check_pushed(commit):
    """Refuses unless gh is logged in and GitHub has the commit the draft will point at."""
    try:
        found = subprocess.run(["gh", "api", f"repos/{{owner}}/{{repo}}/commits/{commit}", "--silent"], cwd=REPO,
                               capture_output=True, text=True)
    except FileNotFoundError:
        refuse("the GitHub CLI (gh) is not installed")
    if found.returncode != 0:
        refuse(f"GitHub does not have {commit[:7]} (push it first; is gh logged in?): {found.stderr.strip()}")


def build(platform, commit):
    """The platform's Shipping package of commit built, run and archived by package.py; the archives' paths."""
    print(f"release: building the Shipping package for {platform}")
    status = subprocess.run([sys.executable, str(REPO / "Scripts" / "package.py"), "Shipping", "--archive",
                             "--platform", platform, "--revision", commit]).returncode
    if status != 0:
        refuse(f"the {platform} package failed (exit status {status}): see package.py's output above")
    revision = (REPO / "out" / "builds" / "Shipping" / platform / "Revision.txt").read_text(encoding="utf-8").strip()
    if revision != git("rev-parse", "--short", commit):
        refuse(f"the {platform} package is of {revision}, not of {commit[:7]}")
    return RELEASES / f"Weltall-{revision}-{platform}.zip", RELEASES / f"Weltall-{revision}-{platform}-symbols.zip"


def main():
    sys.stdout.reconfigure(line_buffering=True)
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("version", help="the release's version, as ProjectVersion has it (0.8.0)")
    parser.add_argument("--platforms", nargs="+", default=PLATFORMS, choices=PLATFORMS, help="the platforms to release (default: both)")
    parser.add_argument("--archives-only", action="store_true", help="write the zips and the notes, but tag and draft nothing")
    args = parser.parse_args()
    if not VERSION.fullmatch(args.version):
        parser.error(f"a version is three numbers, as 0.8.0: not {args.version}")

    check_source(args.version)
    commit = git("rev-parse", "HEAD")
    if not args.archives_only:
        check_pushed(commit)

    players = []
    for platform in args.platforms:
        archive, symbols = build(platform, commit)
        named = RELEASES / f"Weltall-{args.version}-{platform}.zip"
        archive.replace(named)
        if symbols.is_file():
            symbols.replace(RELEASES / f"Weltall-{args.version}-{platform}-symbols.zip")
        players.append(named)
        print(f"release: {named.relative_to(REPO)}")
    notes = RELEASES / f"Weltall-{args.version}-notes.md"
    notes.write_text(unwrapped(NOTES.format(version=args.version)), encoding="utf-8")
    print(f"release: {notes.relative_to(REPO)}")
    if args.archives_only:
        return 0

    drafted = subprocess.run(["gh", "release", "create", f"v{args.version}", *map(str, players), "--draft", "--target", commit,
                              "--title", f"Weltall {args.version}", "--notes-file", str(notes)], cwd=REPO)
    if drafted.returncode != 0:
        refuse(f"gh could not make the draft (exit status {drafted.returncode})")
    git("tag", f"v{args.version}", commit)
    print(f"release: drafted v{args.version}: read it on the release page, then publish it there")
    return 0


if __name__ == "__main__":
    sys.exit(main())
