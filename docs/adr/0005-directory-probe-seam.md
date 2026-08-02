# 0005 - A directory probe seam sits between path resolution and the disk

Status: Accepted, 2026-08-03

## Context

Path resolution has to answer questions that only a filesystem can answer: does
this exist, is it a directory or a file, can this account actually write into
it. Those answers decide whether the application starts. Testing them against a
real disk means the interesting cases, a read-only program-data folder, a file
sitting where a directory belongs, a creation that fails halfway, are either
unreachable or require administrative setup.

## Decision

`DirectoryProbe` is a three-method interface: inspect, create the tree, check
writability. `LocalDirectoryProbe` implements it with the standard library.
`FakeDirectoryProbe` implements it in memory and can be told to block creation
or writes under any prefix. `PathResolver` takes the interface by reference
through its constructor and contains no filesystem call of its own.

Writability is proven by writing, not by reading a permission bit. An access
mask on Windows can say yes while a network share, a quota, or a policy says
no.

## Alternatives rejected

- Calling `std::filesystem` directly from the resolver: the failure paths that
  matter would be untestable, which is the same as untested.
- A wide filesystem abstraction: three methods are what resolution needs, and
  a larger surface would be a fake nobody can implement correctly.

## Consequences

Every interesting failure is a unit test that runs in milliseconds on any
machine. The real implementation is small enough to review by eye and is
additionally exercised against a temporary directory in the same suite.
