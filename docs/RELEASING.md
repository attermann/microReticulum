# Releasing microReticulum

Releases are cut by two GitHub Actions workflows. **Prepare Release** (`release-prepare.yml`) uses Claude to generate a `CHANGELOG.md` entry and release notes from all changes since the previous tag, bumps the version in `library.json` and `library.properties`, and opens a `release/<version>` pull request. Merging that PR runs **Tag Release** (`release-tag.yml`), which creates the annotated tag `<version>` with the release notes as its message, plus a draft GitHub Release.

One-time setup: add an `ANTHROPIC_API_KEY` repository secret, and enable **Settings → Actions → General → Allow GitHub Actions to create and approve pull requests**.

## From the GitHub portal

1. Open **Actions → Prepare Release → Run workflow**.
2. Enter the version (for example `0.5.1`, with no `v` prefix). Leave **Previous release tag** blank to use the most recent tag.
3. When the run finishes, open the `Release <version>` pull request. Review it, and edit `CHANGELOG.md` and `release/<version>/RELEASE_NOTES.md` on the PR branch if needed.
4. Merge the PR. The tag is created automatically.
5. Open **Releases**, review the draft release, and publish it.

## From the command line

```bash
gh workflow run release-prepare.yml --ref master -f version=0.5.1
gh run watch                          # wait for the run to finish
gh pr view release/0.5.1 --web        # review and edit, then merge:
gh pr merge release/0.5.1 --squash
gh run watch                          # wait for Tag Release to finish
git fetch --tags && git show 0.5.1    # tag message contains the release notes
gh release edit 0.5.1 --draft=false   # publish the draft release
```

The PR is opened with the workflow's token, so CI doesn't run on it automatically. Close and reopen the PR to run CI.

To preview the generated documents locally without opening a PR, run the generator in a virtualenv:

```bash
python3 -m venv .venv-release
.venv-release/bin/pip install -r scripts/release/requirements.txt
export ANTHROPIC_API_KEY=...
.venv-release/bin/python scripts/release/generate_release_notes.py collect  --version 0.5.1
.venv-release/bin/python scripts/release/generate_release_notes.py generate --version 0.5.1
```

The output is written to `release/work/CHANGELOG_ENTRY.md` and `release/work/RELEASE_NOTES.md`. The `apply` subcommand makes the same repository changes as the workflow, but releases should normally go through the workflow so that the tag is created from the merged PR.
