#!/bin/sh
# Publishes a version of Nachtzug to the public GitHub repository, github.com/mjohanning99/nachtzug, the way
# Amulets of Anubis is published: one commit per version, by Marvin Johanning, with only the public files.
#
#     tools/publish-github.sh 0.1.0
#
# It takes the last commit of this repository (so commit everything first), copies the files listed in FILES into a
# clone of the public repository (in ~/.cache/nachtzug-github unless NACHTZUG_PUBLIC says otherwise), commits them
# as "Version 0.1.0", tags v0.1.0 and pushes. The tag starts the AppImage build and the release on GitHub
# (.github/workflows/release.yml); the push to main rebuilds the website (pages.yml). Needs git and the GitHub CLI,
# logged in as mjohanning99. Everything private (RELEASING.md, CLAUDE.md, the Forgejo workflows, the history) stays
# here, and so do the working notes listed in PRIVATE.
set -eu
VERSION=${1:?which version? e.g. tools/publish-github.sh 0.1.0}
REPO=mjohanning99/nachtzug
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PUB=${NACHTZUG_PUBLIC:-$HOME/.cache/nachtzug-github}
FILES=".clang-format .editorconfig .github .gitignore AGENTS.md CHANGELOG.md CMakeLists.txt LICENSE LICENSES LICENSING.md README.md assets docs i18n site src tools"
# working notes inside those folders that stay private: the owner's polish list, the roadmap and the refactor plan
PRIVATE="docs/polish-list.md docs/refactor-plan.md docs/roadmap.md"

[ -z "$(git -C "$ROOT" status --porcelain)" ] || { echo "Commit everything first: the last commit is what gets published."; exit 1; }
grep -q "^## $VERSION " "$ROOT/CHANGELOG.md" || { echo "CHANGELOG.md has no section \"## $VERSION\" yet."; exit 1; }
python3 "$ROOT/tools/metainfo.py" --check >/dev/null ||
    { echo "Run tools/metainfo.py and commit: the release notes for software centres are behind CHANGELOG.md."; exit 1; }
CRED='!gh auth git-credential'

if [ ! -d "$PUB/.git" ]; then
    mkdir -p "$PUB"
    git -C "$PUB" init -q -b main
    git -C "$PUB" remote add origin "https://github.com/$REPO.git"
fi
if git -C "$PUB" -c credential.helper="$CRED" fetch -q origin main 2>/dev/null; then
    git -C "$PUB" checkout -q -B main origin/main
fi
git -C "$PUB" tag -l "v$VERSION" | grep -q . && { echo "v$VERSION is already published."; exit 1; }

# the public files of the last commit, and nothing else
find "$PUB" -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +
EXCLUDE=$(for f in $PRIVATE; do printf " :(exclude)%s" "$f"; done)
git -C "$ROOT" archive HEAD $FILES $EXCLUDE | tar -x -C "$PUB"
for f in $PRIVATE; do [ ! -e "$PUB/$f" ] || { echo "$f would have been published; stopping."; exit 1; }; done

EMAIL=$(gh api user --jq '"\(.id)+\(.login)@users.noreply.github.com"')
export GIT_AUTHOR_NAME="Marvin Johanning" GIT_AUTHOR_EMAIL="$EMAIL" GIT_COMMITTER_NAME="Marvin Johanning" GIT_COMMITTER_EMAIL="$EMAIL"
git -C "$PUB" add -A
git -C "$PUB" commit -q -m "Version $VERSION"
git -C "$PUB" tag -a "v$VERSION" -m "Version $VERSION"
# one after the other: GitHub can drop the tag's event when both arrive in one push, and then nothing is released
git -C "$PUB" -c credential.helper="$CRED" push -q origin main
git -C "$PUB" -c credential.helper="$CRED" push -q origin "v$VERSION"
echo "Published v$VERSION: https://github.com/$REPO (website: https://mjohanning99.github.io/nachtzug/)"
