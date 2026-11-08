# microReticulum Pull Request Policy

This document explains what microReticulum will and won't accept, so you
can tell before you start whether a change is likely to be merged. It
complements [CONTRIBUTING.md](../CONTRIBUTING.md), which covers *how* to
contribute.

Every pull request is reviewed against these rules. The maintainer uses
automated tooling to help prepare reviews, but every merge or close decision
is made by the maintainer personally.

---

## 1. What microReticulum is

A C++ port of the [Reticulum Network Stack](https://reticulum.network/),
built as a **library** (not an application) that runs on 32-bit and better
microcontrollers (such as the ESP32 and nRF52 families) and on native
platforms (Linux, macOS, Raspberry Pi).

The library targets **platforms, not boards**. Nothing in it should be
specific to a particular board, module or variant. Applications built on it, such as
RNode-style firmware, transport nodes and UI clients, are where board
support belongs, and they live in their own repositories, e.g.
[microReticulum_Firmware](https://github.com/attermann/microReticulum_Firmware).

## 2. Project goals (what a PR should move toward)

1. **Wire compatibility with the Python reference implementation.**
   A microReticulum node must interoperate with stock Python RNS nodes.
2. **An API that mirrors the reference implementation.** Class names,
   concepts and call patterns should feel familiar to someone who knows
   Python RNS, with implicit object sharing as today.
3. **Fitting constrained MCUs.** RAM, flash and filesystem space are scarce
   (an nRF52840's internal filesystem is only 28 KB). Code size, heap use,
   fragmentation and stack depth all matter.
4. **Opt-in features.** Optional subsystems are gated behind build flags
   (`RNS_USE_FS`, `RNS_USE_PROVISIONING`, …) so they cost nothing when
   disabled.
5. **Cross-platform everywhere.** One codebase builds on every supported
   target through both PlatformIO and CMake, under C++11 through C++20.
   Code is platform-independent by default. Platform-specific code is
   allowed only where it's unavoidable, such as random number generation,
   and is kept as small and as isolated as possible.
6. **Completing the roadmap.** Open items in the README roadmap — currently
   **Ratchets** and **Channel and Buffer** — are especially welcome.

## 3. In scope

- Bug fixes in protocol, transport, link, resource, crypto, persistence or
  provisioning code.
- Remaining parts of the Reticulum protocol from the roadmap.
- Memory, flash and performance improvements that don't hurt readability
  or compatibility.
- New interfaces or storage adapters that fit the existing abstractions
  (`Interface`, `microStore::File`/`FileSystem`) and aren't tied to a
  particular board.
- Tests (unit tests in `test/`, interop tests in `test_interop/`).
- Documentation fixes and improvements.
- Build-system fixes for PlatformIO or CMake.

## 4. Out of scope (likely to be closed or redirected)

- **Application or firmware features** (UIs, device-specific apps, radio
  front-ends that belong in firmware) → redirect to
  microReticulum_Firmware or a separate project.
- **Protocol behavior that differs from the Python reference** without
  being behind a preprocessor guard that's off by default (see 5.4).
- **New third-party dependencies**, or replacing existing ones (Crypto,
  ArduinoJson, MsgPack, microStore), unless discussed in an issue first.
- **Large refactors, renames or reformatting** not tied to a stated
  problem, or not discussed first.
- **Board- or variant-specific code**: board definitions, variant files,
  pin maps, board-specific build environments, or conditional code for a
  particular board or module. This belongs in the application or firmware
  using the library → redirect to microReticulum_Firmware or the
  contributor's own project.
- **Platform-specific code where a portable approach exists**, and
  **changes that work on only one platform** in shared code paths.
- **Changes to the cryptographic primitives** that aren't fixing a
  demonstrated bug against the reference implementation.

## 5. Requirements for merging

A PR that's missing any of these will be sent back for changes before it
can be merged.

1. **CI passes:** the native test matrix (`native`, `native14`, `native17`,
   `native20`) and the embedded builds, which compile the library for
   ESP32 and nRF52 using a few representative board environments
   (`ttgo-t-beam`, `lilygo_tbeam_supreme`, `ttgo-lora32-v21`,
   `wiscore_rak4631`). PRs from
   first-time contributors may need the maintainer to approve CI before it
   runs; "not yet run" isn't a failure, but it isn't a pass either.
2. **One logical concern per PR.** Unrelated fixes, formatting and
   refactors go in separate PRs.
3. **Problem stated first.** The description says what's broken or missing,
   who is affected and how to reproduce, before the solution.
4. **Divergences are guarded.** Any behavior that differs from the Python
   reference is wrapped in an `RNS_*` preprocessor guard, documented, and
   off by default unless the maintainer has agreed otherwise.
5. **Platforms tested are listed.** Changes to shared code (protocol,
   serialization, packet handling, routing, timing, configuration) list
   which platforms were tested, how, and which weren't.
6. **No breaking API or wire changes** without prior discussion and a
   CHANGELOG entry.
7. **Docs updated** for user-visible behavior, build flags, APIs or
   workflows (README, `docs/`, CHANGELOG as appropriate).
8. **Reserved IDs respected:** provisioning namespace IDs 1–99 are reserved
   for the library.
9. **License:** contributions are accepted under Apache-2.0. Code copied
   from elsewhere must be license-compatible and credited.
10. **Platform-specific code is unavoidable and isolated.** It appears only
    where no portable alternative exists, is confined to as small an area
    as possible rather than spread through shared code, and the PR explains
    why it's needed. Board- or variant-specific code isn't accepted (§4).

## 6. Preferences

These won't block a PR on their own, but they make it faster to review and
more likely to be merged.

- Follows the existing coding style and naming.
- Tests added or updated for behavior changes.
- Heap allocation in hot paths avoided; uses the `RNS_*_ALLOCATOR`
  framework rather than raw allocation for long-lived containers.
- Logging uses the existing log levels and isn't noisy at INFO.
- Small enough to review in one sitting (roughly < 500 changed lines
  excluding tests and docs; bigger needs a good reason).
- Interop impact covered by `test_interop/` where protocol behavior changes.

## 7. What happens after you submit

Each pull request ends up in one of four places:

| Outcome | What it means for you |
|---|---|
| **Merged** | It's in scope and meets the requirements in section 5. |
| **Changes requested** | The idea is welcome, but specific things need fixing first (failing CI, missing tests or docs, an unguarded divergence, too many concerns in one PR). You'll get a list. |
| **Discussion needed** | It raises a design, scope or compatibility question that has to be settled before code review. Expect the conversation to move to an issue. |
| **Closed** | It's out of scope (section 4), duplicates existing work, or conflicts with the project's goals. Where possible you'll be pointed to a better home for it. |

The maintainer has limited time, so reviews may take a while. A PR that
follows this document and the template is the fastest to get through.
If you're unsure whether something fits, open an issue first; that's
always welcome and often saves rework.
