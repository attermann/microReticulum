# Contributing to microReticulum

Thank you for your interest in contributing to **microReticulum**! Community contributions are greatly appreciated and help make the project better for everyone.

This document describes the expectations for issues, pull requests, and code contributions. Following these guidelines helps keep reviews focused, improves software quality, and reduces the time required to merge contributions.

---

# Before You Start

Before opening an issue or submitting a pull request, please:

- Read the project documentation.
- Search existing issues and pull requests to avoid duplicates.
- Verify that the issue still exists on the latest version of the code.
- Be prepared to explain both the problem and why your proposed solution is appropriate.

---

# Reporting Bugs

A good bug report should make it easy for someone else to reproduce the problem.

Please include as much of the following information as possible:

- microReticulum version
- Platform and hardware
- Operating system (if applicable)
- Compiler and SDK versions (if relevant)
- Configuration used
- Steps required to reproduce the issue
- Expected behavior
- Actual behavior
- Log output
- Packet captures or traces (if applicable)
- Photographs or diagrams when hardware is involved

The easier an issue is to reproduce, the more likely it can be fixed quickly.

---

# Suggesting Features

Feature requests should clearly describe:

- The problem being solved
- Why existing functionality is insufficient
- The proposed solution
- Possible alternatives
- Expected impact on existing users
- Any compatibility concerns

Whenever possible, describe the user problem rather than only the implementation you would like to see.

---

# Pull Request Guidelines

## One Pull Request = One Concern

Please keep pull requests focused on a **single logical change**.

Good examples include:

- Fix one bug
- Add one feature
- Refactor one subsystem
- Improve one piece of documentation

Avoid combining unrelated changes such as:

- Bug fixes
- Feature additions
- Style changes
- Formatting
- Refactoring
- Documentation updates

into the same pull request.

Smaller pull requests are significantly easier to review, test, and merge.

---

## Start With the Problem

Every pull request should begin by clearly explaining:

- What problem exists?
- Who is affected?
- How can the issue be reproduced?
- Why is the current behavior incorrect or undesirable?

Only after defining the problem should the solution be described.

---

## Describe the Solution

Explain:

- How the change solves the problem
- Why this approach was chosen
- Any tradeoffs involved
- Any limitations
- Any compatibility implications

Reviewers should be able to understand the reasoning behind the implementation without having to infer it from the code.

---

## Keep Changes Minimal

Implement only the changes required to solve the stated problem.

Avoid unrelated cleanup while working on a feature or bug fix.

Large-scale refactoring should generally be submitted separately.

---

# Code Quality

Contributions should:

- Be readable and maintainable.
- Follow the existing coding style.
- Avoid unnecessary complexity.
- Prefer clarity over cleverness.
- Keep functions reasonably focused.
- Avoid introducing unnecessary dependencies.

---

# Testing

All contributions should be tested before submission.

At a minimum:

- Verify that the change solves the intended problem.
- Verify that existing functionality continues to work.
- Test edge cases where practical.

## Cross-Platform Changes

microReticulum supports multiple hardware platforms and environments.

If a pull request affects code shared by multiple platforms, it **must be tested on every affected platform** before submission.

Examples include:

- Shared protocol code
- Common libraries
- Serialization
- Packet handling
- Routing logic
- Timing
- Configuration parsing

A pull request should identify:

- Which platforms were tested
- What testing was performed
- Any platforms that were not tested and why

Contributors should not assume that successful testing on one platform guarantees correct behavior on another.

---

# Backward Compatibility

Whenever practical:

- Preserve existing APIs.
- Preserve protocol compatibility.
- Avoid breaking existing deployments.
- Clearly document any unavoidable breaking changes.

---

# Documentation

If a change affects:

- user-visible behavior,
- configuration,
- APIs,
- command-line options,
- or development workflows,

please update the relevant documentation as part of the same pull request.

---

# Commit Messages

Please write clear, descriptive commit messages.

Good examples:

```
Fix route expiration when neighbor timestamps drift

Add configurable neighbor probing interval

Prevent duplicate packet forwarding during route repair
```

Avoid messages such as:

```
fix

changes

misc

cleanup

updates
```

---

# Review Process

All pull requests are reviewed for:

- correctness
- maintainability
- readability
- testing
- compatibility
- project scope

Maintainers may request revisions before merging.

Submitting a pull request does not guarantee that it will be accepted.

---

# Be Respectful

Constructive discussion is encouraged.

Please assume good intent, remain respectful, and focus discussions on technical merits and project goals.

---

# Questions

If you are unsure whether an idea belongs in microReticulum, feel free to open an issue for discussion before investing significant implementation effort.

Early discussion often saves everyone time.

Thank you for helping improve microReticulum!