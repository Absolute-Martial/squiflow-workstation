# Performance optimization pass - how to pick this up

This copy of the repository has 4 new commits on top of an initial
"pre-optimization baseline" commit (`squiflow-git-history.bundle` carries the
real git history; `.git/` itself was left out of this zip since most zip
tools don't preserve it cleanly). To restore full history:

```
cd squiflow-production-fixed   # this extracted folder
git init
git pull squiflow-git-history.bundle master
git checkout master
```

Then `git log --oneline` shows:

```
1b308a7 docs: record the August 2026 performance-optimization pass
965e194 build(presets): stop hardcoding build parallelism to 2 jobs
b8708a9 build(presets): use Ninja for the Linux verification lane
c9307e5 perf(build): add ccache compiler cache and shared PCH
26c467f chore: import SquiFlow production snapshot (pre-optimization baseline)
```

**Start here:** `docs/plan/build-performance.md` - the full report, in this
project's own evidence-based documentation style: what was measured, what
changed, the exact numbers, two measurement mistakes made and corrected
along the way, and - just as important - what could **not** be verified in
the sandbox this pass ran in (no Qt 6.11, no display server, 1 CPU core) and
what to do about that next.

Nothing in `src/` changed. Every change is in the build configuration
(`cmake/`, `CMakePresets.json`) plus the two documentation files.
