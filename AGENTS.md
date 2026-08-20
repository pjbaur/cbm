# AGENTS.md — Git Workflow Strategy

This file describes the branching and merge strategy for local development
on this repository. `CLAUDE.md` is a symlink to this file.

## Repository layout

- `origin` is the shared upstream repository
  (`resurrecting-open-source-projects/cbm`). It is read-mostly: never push
  local branches to it.
- `master` mirrors `origin/master`. It only changes by merging the
  development branch, and always reflects a known-good state.
- `sandbox` is the running development branch. All work happens here.

```
origin/master == master  (stable, only updated by merges)
     \
      sandbox            (all work happens here)
       \
        topic-x          (optional, per update)
```

## Workflow

1. Do the work on `sandbox`, one logical update per commit.
2. If an update is experimental, touches the same files as another
   in-flight update, or might be discarded, put it on a short-lived
   `topic` branch off `sandbox` instead, and merge it back when done.
3. When a batch of updates is stable and verified, merge it down:
   ```
   git checkout master
   git merge --no-ff sandbox
   ```
   The `--no-ff` merge commit marks the batch boundary and gives a
   free rollback point for the whole batch.
4. Continue on `sandbox`.

## Rules

- Merge direction is one-way: `sandbox` into `master`. Never merge
  untested work into `master`.
- When upstream moves, sync in this order:
  ```
  git fetch origin
  git checkout sandbox
  git merge origin/master     # resolve conflicts here
  git checkout master
  git merge --no-ff sandbox   # or fast-forward if already synced
  ```
- Do not use a Git Flow style layout (develop/release/hotfix branches);
  it is oversized for a solo-maintained project of this scale.
- Commit messages: short imperative subject line, body explaining what
  and why. One logical change per commit.

## Contributing upstream

To submit local work to the upstream project:

1. Fork `resurrecting-open-source-projects/cbm` on GitHub.
2. `git remote add fork <your-fork-url>`
3. Push `sandbox` (or a clean topic branch) to the fork.
4. Open pull requests from there. Upstream accepts small, focused fixes —
   keep each pull request a single logical change.
