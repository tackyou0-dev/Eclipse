#!/usr/bin/env bash
#
# PROJECT ECLIPSE - release script.
#
# Runs the engine-free gates, exports the source bundle, tags the release and (when a token
# with write access is configured) pushes it. The push step is deliberately explicit about
# what it needs: a GitHub credential with "Contents: Read and write" on the repository, or
# an authenticated `gh` session.
#
# Usage:
#   scripts/publish.sh                 # verify, bundle, tag, print the push commands
#   scripts/publish.sh --push          # also push the tag and create the release
#   scripts/publish.sh --dry-run       # verify and bundle only
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUNDLE="$REPO_ROOT/../project-eclipse.bundle"
PUSH=0
DRY_RUN=0

for argument in "$@"; do
	case "$argument" in
		--push) PUSH=1 ;;
		--dry-run) DRY_RUN=1 ;;
		*) echo "publish: unknown argument '$argument'" >&2; exit 2 ;;
	esac
done

step() { printf '\n==> %s\n' "$1"; }

step "Gate 1/3 - C++ structure lint"
python3 Tools/ci/lint_cpp.py

step "Gate 2/3 - reference model tests"
python3 -m unittest discover -s Tests/Reference -p 'test_*.py' -v

step "Gate 3/3 - content validation"
python3 Tools/ci/validate_content.py

step "Agent briefs"
python3 scripts/generate_agents.py
python3 scripts/deploy_subagents.py --check

step "Export the source bundle"
python3 Tools/ci/export_bundle.py --quiet --output "$BUNDLE"
ls -l "$BUNDLE"

VERSION="${VERSION:-}"
if [ -z "$VERSION" ]; then
	VERSION="$(git describe --tags --always --dirty 2>/dev/null || echo untagged)"
fi
TAG="eclipse-$VERSION"

step "Release $TAG"
if [ "$DRY_RUN" -eq 1 ]; then
	echo "dry run: would tag $TAG and push"
	exit 0
fi

if git rev-parse "$TAG" >/dev/null 2>&1; then
	echo "publish: tag $TAG already exists"
else
	git tag -a "$TAG" -m "PROJECT ECLIPSE $VERSION"
	echo "publish: created tag $TAG"
fi

if [ "$PUSH" -eq 0 ]; then
	cat <<EOF

Verified and tagged locally. To publish, the credential must be able to write to the
repository (a GitHub App installed on the repository with "Contents: Read and write", or an
authenticated gh session). Then:

    git push origin HEAD
    git push origin $TAG
    gh release create $TAG "$BUNDLE" --title "PROJECT ECLIPSE $VERSION" --notes-file ROADMAP.md

See PUBLISHING.md for the full checklist and the exact failure this guards against.
EOF
	exit 0
fi

step "Push"
git push origin HEAD
git push origin "$TAG"
gh release create "$TAG" "$BUNDLE" --title "PROJECT ECLIPSE $VERSION" --notes-file ROADMAP.md

echo "publish: released $TAG"
