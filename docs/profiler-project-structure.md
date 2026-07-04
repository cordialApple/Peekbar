# Why one repo, two targets (not one exe, not two repos)

Companion to `ARCHITECTURE.md` §10. Justifies how `shell_profiler` is packaged
relative to the shell.

## The decision

`browser_shell_os` (the shell) and `shell_profiler` (the observability tool)
live in **one repository / one CMake project** but build to **two separate
executables** — two independent targets that share no source and no link
dependency. The shell emits ETW (TraceLogging); the profiler consumes it out of
process. The only contract between them is the provider name
`BrowserShellOs.Perf` and the event/field names in §10.

## One repository / one CMake project

- **Single source of truth for the ETW contract.** The event names, field
  names, and the name-derived provider GUID are the entire interface. Keeping
  emitter and consumer in one repo means the contract is defined in one place
  and reviewed together.
- **Atomic changes when an event's shape moves.** When a field is added or an
  event renamed, the shell's call site and the profiler's decode/aggregation
  change in the **same commit**. There is no window where a released shell emits
  events a released profiler cannot read.
- **Shared toolchain and CI.** One MSVC/CMake setup, one build invocation, one
  set of compiler/SDK pins. `cmake --build build` produces both; there is no
  second pipeline to keep in sync.
- **Trivial local run of both.** A developer builds the shell and the profiler
  from the same tree and runs them side by side with no cross-repo checkout or
  version pinning.

## Two executables / two build targets

- **Hard rule 8 — the shell stays lean.** No logging framework, no log files,
  no telemetry threads, no metrics UI compiled into the shell. The shell carries
  only the header-only TraceLogging wrapper and its call sites; when nothing is
  listening, events are discarded inside the provider at near-zero cost.
- **Independent cadence.** The profiler's TDH decode, metrics math, and CSV
  export evolve and ship without rebuilding or redeploying the shell.
- **Fault isolation.** A profiler crash, a slow trace-processing loop, or heavy
  aggregation runs in a **different process** and cannot destabilize the dock.
  The shell has zero link-time or run-time dependency on the profiler — delete
  `shell_profiler.exe` and the shell runs identically (§12 row P).
- **Buildable apart.** Either target builds alone
  (`cmake --build build --target shell_profiler` / `--target browser_shell_os`),
  and the profiler also configures standalone (`cmake -B out profiler`), so the
  "two programs" separation is enforced by the build, not just by convention.

## Why not one merged executable

- Violates hard rule 8 directly: it links trace-processing, metrics, and CSV
  code into the shell, bloating the exe the project explicitly keeps minimal.
- Couples lifetimes: the profiler could not be started/stopped/updated
  independently, and could not observe the shell's own startup/shutdown from
  outside.
- Removes fault isolation: a bug in metric aggregation or a runaway trace loop
  would crash or stall the dock — the one process users depend on.
- Kills the "zero cost when not observing" property — the whole point of the
  ETW design is that measurement is out of process and opt-in.

## Why not two separate repositories

- Splits the one contract that binds them across two histories, inviting
  version skew: a shell that emits an event shape the checked-out profiler can't
  decode, with no single commit that proves they agree.
- Loses atomic contract changes — an event rename becomes a coordinated
  two-repo release instead of one commit.
- Duplicates CI, toolchain pins, and SDK configuration for two small Win32/MSVC
  targets that already share an environment.
- Adds cross-repo checkout/pinning friction for what is, in practice, always
  developed and run together.

§10 notes the profiler "may later move to its own repository — nothing binds it
to the shell's build." That option stays open precisely because the two are
already separate targets sharing no code: extracting the `profiler/` directory
is a clean lift. Until there is a reason to (e.g., an external team owning it),
one repo / two targets is the lower-friction, lower-risk structure.
