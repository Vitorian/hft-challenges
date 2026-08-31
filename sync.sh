#!/bin/bash
# Sync common files and challenge scaffolding from the public hft-challenges repo.
#
#   * Pulls in NEW challenge directories that don't exist locally yet (the dir
#     list is read from UPSTREAM, not from what you already have).
#   * Mirrors every scaffolding file in each challenge — harness headers, CMake,
#     benchmark driver, replay/triathlon headers, README, fetch scripts, the
#     challenge.yaml, etc. — everything EXCEPT solution/.
#   * Never overwrites an existing solution/. A brand-new challenge's baseline
#     solution/ is seeded once so you have a starting point to edit.
#   * Self-updates: if upstream ships a newer sync.sh it adopts it and re-runs,
#     so script fixes reach you automatically on your next sync.
set -euo pipefail

UPSTREAM_URL="https://github.com/Vitorian/hft-challenges.git"
BR="main"

# Must be run from repo root
if [ ! -d ".git" ]; then
    echo "Error: run this from the root of your challenges repo." >&2
    exit 1
fi

# Add upstream remote if not present
if ! git remote get-url upstream &>/dev/null; then
    echo "Adding upstream remote: $UPSTREAM_URL"
    git remote add upstream "$UPSTREAM_URL"
fi

echo "Fetching upstream..."
git fetch upstream "$BR"
REF="upstream/$BR"

# Self-update: adopt the canonical sync.sh and re-exec so the new logic (and any
# new challenges it knows how to pull) take effect in this same run. The re-exec
# can't loop: after the checkout the script matches upstream, so the next pass
# skips this block. Don't hand-edit sync.sh in a synced repo — edit it in the
# public hft-challenges repo, which is the source of truth.
if ! git diff --quiet "$REF" -- sync.sh 2>/dev/null; then
    echo "Updating sync.sh from upstream and re-running ..."
    git checkout "$REF" -- sync.sh
    exec bash "$0" "$@"
fi

# Shared harness / helpers — always mirror upstream.
echo "Syncing common/ ..."
git checkout "$REF" -- common/ 2>/dev/null || true

# Per-challenge scaffolding. Enumerate from UPSTREAM so brand-new challenges are
# created locally, not just the ones you already have.
new=0
while IFS= read -r dir; do
    [ -n "$dir" ] || continue

    # Every tracked file in this challenge upstream, minus solution/.
    mapfile -t files < <(git ls-tree -r --name-only "$REF" -- "$dir" | grep -v "^$dir/solution/" || true)
    if [ "${#files[@]}" -gt 0 ]; then
        if [ -d "$dir" ]; then
            echo "Syncing $dir/ (keeping your solution/) ..."
        else
            echo "NEW $dir/ — adding scaffolding ..."
            new=$((new + 1))
        fi
        git checkout "$REF" -- "${files[@]}"
    fi

    # Seed the baseline solution/ ONLY if you don't already have one (never
    # clobber). Any existing file counts — C++ uses solution.h/.cpp, Rust mod.rs.
    if [ -z "$(ls -A "$dir/solution" 2>/dev/null)" ]; then
        mapfile -t sol < <(git ls-tree -r --name-only "$REF" -- "$dir/solution/" || true)
        if [ "${#sol[@]}" -gt 0 ]; then
            git checkout "$REF" -- "${sol[@]}"
            echo "  seeded $dir/solution/ (baseline — edit this)"
        fi
    fi
done < <(git ls-tree -d --name-only "$REF" | grep '^challenge-' || true)

# Root docs.
git checkout "$REF" -- README.md 2>/dev/null || true

echo ""
if [ "$new" -gt 0 ]; then
    echo "Added $new new challenge(s)."
fi
echo "Done. Your solution/ files are untouched."
echo "Review changes with: git diff --cached"
