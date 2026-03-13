#!/bin/bash
# Sync common files and challenge scaffolding from the public hft-challenges repo.
# Your solution/ directories are never touched.
set -e

UPSTREAM_URL="https://github.com/Vitorian/hft-challenges.git"
UPSTREAM_BRANCH="main"

# Must be run from repo root
if [ ! -d ".git" ]; then
    echo "Error: run this from the root of your challenges repo."
    exit 1
fi

# Add upstream remote if not present
if ! git remote get-url upstream &>/dev/null; then
    echo "Adding upstream remote: $UPSTREAM_URL"
    git remote add upstream "$UPSTREAM_URL"
fi

# Fetch latest
echo "Fetching upstream..."
git fetch upstream "$UPSTREAM_BRANCH"

# Checkout non-solution files from upstream
echo "Syncing common files..."
git checkout "upstream/$UPSTREAM_BRANCH" -- common/

# Sync per-challenge scaffolding (everything except solution/)
for dir in challenge-*/; do
    if git cat-file -e "upstream/$UPSTREAM_BRANCH:$dir" 2>/dev/null; then
        echo "Syncing $dir (keeping your solution/)..."
        git checkout "upstream/$UPSTREAM_BRANCH" -- "${dir}CMakeLists.txt" "${dir}benchmark.cpp" "${dir}challenge.yaml" "${dir}README.md" 2>/dev/null || true
    fi
done

# Sync root files
git checkout "upstream/$UPSTREAM_BRANCH" -- README.md sync.sh 2>/dev/null || true

echo ""
echo "Done. Your solution/ files are untouched."
echo "Review changes with: git diff --cached"
