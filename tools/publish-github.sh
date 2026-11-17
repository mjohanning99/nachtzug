#!/bin/sh
# Publishes a version of Nachtzug to the public GitHub repository, github.com/mjohanning99/nachtzug, the way
# Amulets of Anubis is published: one commit per version, by Marvin Johanning, with only the public files.
#
#     tools/publish-github.sh 0.1.0
#
# It takes the last commit of this repository (so commit everything first), copies the files listed in FILES into a
# clone of the public repository (in ~/.cache/nachtzug-github unless NACHTZUG_PUBLIC says otherwise), commits them
# as "Version 0.1.0", tags v0.1.0 and pushes. The tag starts the AppImage build and the release on GitHub
# (.github/workflows/release.yml). The same tag goes onto the last commit here and to Forgejo, where it puts the
# website online (.forgejo/workflows/site.yml). Needs git and the GitHub CLI,
# logged in as mjohanning99. Everything private (RELEASING.md, CLAUDE.md, the Forgejo workflows, the history) stays
# here, and so do the working notes listed in PRIVATE. At the end it runs tools/publish-homebrew.sh, which waits for
# the release build and brings the Homebrew tap up to the version (NACHTZUG_NO_HOMEBREW=1 leaves that out).
set -eu
VERSION=${1:?which version? e.g. tools/publish-github.sh 0.1.0}
REPO=mjohanning99/nachtzug
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PUB=${NACHTZUG_PUBLIC:-$HOME/.cache/nachtzug-github}
FILES=".clang-format .editorconfig .github .gitignore AGENTS.md CHANGELOG.md CMakeLists.txt LICENSE LICENSES LICENSING.md README.md assets docs i18n site src tools"
# files inside those folders that stay private: the owner's polish list, the roadmap, the refactor plan, and the
# scripts for the package recipes and the Homebrew tap (packaging/ itself is private)
PRIVATE="docs/polish-list.md docs/refactor-plan.md docs/roadmap.md tools/packaging.py tools/publish-homebrew.sh"

[ -z "$(git -C "$ROOT" status --porcelain)" ] || { echo "Commit everything first: the last commit is what gets published."; exit 1; }
grep -q "^## $VERSION " "$ROOT/CHANGELOG.md" || { echo "CHANGELOG.md has no section \"## $VERSION\" yet."; exit 1; }
python3 "$ROOT/tools/metainfo.py" --check >/dev/null ||
    { echo "Run tools/metainfo.py and commit: the release notes for software centres are behind CHANGELOG.md."; exit 1; }
CRED='!gh auth git-credential'
# git on the public clone, deaf to this computer's git settings: no global or system config, no hooks, no commit
# template, so nothing (a Co-authored-by line, a signature, a changed name) can creep into the public commit
pub() { GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_NOSYSTEM=1 git -C "$PUB" -c core.hooksPath=/dev/null "$@"; }

if [ ! -d "$PUB/.git" ]; then
    mkdir -p "$PUB"
    pub init -q -b main
    pub remote add origin "https://github.com/$REPO.git"
fi
if pub -c credential.helper="$CRED" fetch -q origin main 2>/dev/null; then
    pub checkout -q -B main origin/main
fi
pub tag -l "v$VERSION" | grep -q . && { echo "v$VERSION is already published."; exit 1; }

# the public files of the last commit, and nothing else
find "$PUB" -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +
EXCLUDE=$(for f in $PRIVATE; do printf " :(exclude)%s" "$f"; done)
git -C "$ROOT" archive HEAD $FILES $EXCLUDE | tar -x -C "$PUB"
for f in $PRIVATE; do [ ! -e "$PUB/$f" ] || { echo "$f would have been published; stopping."; exit 1; }; done

EMAIL=$(gh api user --jq '"\(.id)+\(.login)@users.noreply.github.com"')
export GIT_AUTHOR_NAME="Marvin Johanning" GIT_AUTHOR_EMAIL="$EMAIL" GIT_COMMITTER_NAME="Marvin Johanning" GIT_COMMITTER_EMAIL="$EMAIL"
pub add -A
pub commit -q --no-verify -m "Version $VERSION"
pub tag -a "v$VERSION" -m "Version $VERSION"

# before anything leaves this computer: the commit and the tag say "Version x.y.z" and nothing else, and name only
# Marvin Johanning with the GitHub address; otherwise both are taken back and nothing is pushed
ME="Marvin Johanning <$EMAIL>"
undo() {
    echo "The public commit isn't clean ($1); nothing was pushed."
    pub tag -d "v$VERSION" >/dev/null
    if pub rev-parse -q --verify origin/main >/dev/null; then pub reset -q --hard origin/main; else rm -rf "$PUB/.git"; fi
    exit 1
}
[ "$(pub log -1 --format=%B)" = "Version $VERSION" ] || undo "the commit message has more than \"Version $VERSION\""
[ "$(pub log -1 --format='%an <%ae>')" = "$ME" ] || undo "the author isn't $ME"
[ "$(pub log -1 --format='%cn <%ce>')" = "$ME" ] || undo "the committer isn't $ME"
[ "$(pub tag -l --format='%(contents)' "v$VERSION")" = "Version $VERSION" ] || undo "the tag says more than \"Version $VERSION\""
[ "$(pub tag -l --format='%(taggername) %(taggeremail)' "v$VERSION")" = "$ME" ] || undo "the tagger isn't $ME"
if pub log --format=%B origin/main..HEAD 2>/dev/null | grep -qi 'co-authored-by'; then undo "a Co-authored-by line"; fi
# one after the other: GitHub can drop the tag's event when both arrive in one push, and then nothing is released
pub -c credential.helper="$CRED" push -q origin main
pub -c credential.helper="$CRED" push -q origin "v$VERSION"
echo "Published v$VERSION: https://github.com/$REPO"

# the same tag here, on the commit just published, and on Forgejo: it puts the website online for this version
git -C "$ROOT" tag -l "v$VERSION" | grep -q . || git -C "$ROOT" tag -a "v$VERSION" -m "Version $VERSION" HEAD
FORGEJO=$(git -C "$ROOT" remote -v | awk '/git\.heliacal\.systems/ && /\(push\)/ {print $1; exit}')
if [ -n "$FORGEJO" ]; then
    git -C "$ROOT" push -q "$FORGEJO" "v$VERSION" && echo "Tagged v$VERSION on Forgejo: the website goes online at https://nachtzugmusic.com/"
else
    echo "No Forgejo remote here: push the tag from a clone that has one (git push origin v$VERSION) to put the website online."
fi

# then the Homebrew tap, once the release build has made the Mac disk image (private: only where the script is)
if [ -x "$ROOT/tools/publish-homebrew.sh" ] && [ -z "${NACHTZUG_NO_HOMEBREW:-}" ]; then
    "$ROOT/tools/publish-homebrew.sh" "$VERSION"
fi
