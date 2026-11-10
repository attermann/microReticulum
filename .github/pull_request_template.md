## Problem

<!-- What is broken or missing? Who is affected? How can it be reproduced? -->

## Solution

<!-- How does this change solve it, why this approach, any tradeoffs or limitations? -->

## Compatibility

- [ ] No change to wire behavior vs. the Python reference implementation
- [ ] Any divergence from the reference is behind an `RNS_*` preprocessor guard (off by default)
- [ ] No breaking public API change (or: described below and added to CHANGELOG)
- [ ] No board- or variant-specific code; any platform-specific code is unavoidable, isolated, and explained below

## Testing

Platforms tested (and how):

- [ ] native (PlatformIO / CMake)
- [ ] ESP32 (board: )
- [ ] nRF52 (board: )
- [ ] Interop with Python RNS (`test_interop/`)

Platforms not tested, and why:

## Docs

- [ ] README / `docs/` / CHANGELOG updated where user-visible behavior, build flags or APIs changed

<!-- Please read CONTRIBUTING.md and .github/PR_POLICY.md before submitting. One logical concern per PR. -->
