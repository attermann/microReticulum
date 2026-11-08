# Releasing microReticulum

Releases are cut by two GitHub Actions workflows plus one manual step:

1. **Prepare Release** (`release-prepare.yml`) uses Claude to generate a `CHANGELOG.md` entry and release notes from all changes since the previous tag. It also bumps the version in `library.json` and `library.properties`, then pushes everything to a `release/<version>` branch.
2. **You** open a pull request from that branch, review it, and merge it. The workflow doesn't open the PR itself. Letting `GITHUB_TOKEN` create PRs needs a repository setting that also lets Actions *approve* PRs, and CI doesn't run on PRs opened by `GITHUB_TOKEN`.
3. **Tag Release** (`release-tag.yml`) runs when the PR is merged. It creates the annotated tag `<version>` with the release notes as the tag message, plus a draft GitHub Release.

One-time setup: add a `RELEASE_ANTHROPIC_API_KEY` repository secret, under **Settings → Secrets and variables → Actions**. Use a key reserved for release generation, separate from keys used by other automation. No other repository settings are needed.

## From the GitHub portal

1. Open **Actions → Prepare Release → Run workflow**.
2. Enter the version (for example `0.5.1`, with no `v` prefix). Leave **Previous release tag** blank to use the most recent tag.
3. When the run finishes, open its summary page and click **open the release pull request**. The summary also shows the generated release notes.
4. On the PR form, paste the release notes as the description if you want, then create the PR. Review it, and edit `CHANGELOG.md` and `release/<version>/RELEASE_NOTES.md` on the PR branch if needed. CI runs on the PR as usual.
5. Merge the PR. Tag Release creates the tag automatically.
6. Open **Releases**, review the draft release, and publish it.

## From the command line

```bash
gh workflow run release-prepare.yml --ref master -f version=0.5.1
gh run watch                          # wait for Prepare Release to finish

# Open the PR, using the generated release notes as its description
git fetch origin release/0.5.1
git show origin/release/0.5.1:release/0.5.1/RELEASE_NOTES.md | \
  gh pr create --base master --head release/0.5.1 --title "Release 0.5.1" --body-file -

gh pr view release/0.5.1 --web       # review and edit, then merge:
gh pr merge release/0.5.1 --squash
gh run watch                          # wait for Tag Release to finish
git fetch --tags && git show 0.5.1    # tag message contains the release notes
gh release edit 0.5.1 --draft=false   # publish the draft release
```

## If something goes wrong

- **Prepare Release failed before pushing the branch.** Fix the cause and run it again. The generator's input and output are uploaded as the `release-<version>` workflow artifact, so you can inspect what it produced.
- **You want to regenerate a release branch.** Delete the `release/<version>` branch (`git push origin --delete release/<version>`) and run Prepare Release again. It refuses to overwrite an existing branch.
- **The branch was merged without a PR, or Tag Release didn't run.** Start it by hand from `master` before anything else is pushed, because it tags the latest commit on the branch you run it from:
  ```bash
  gh workflow run release-tag.yml --ref master -f version=0.5.1
  ```

## Previewing locally

To preview the generated documents without pushing a branch, run the generator in a virtualenv:

```bash
python3 -m venv .venv-release
.venv-release/bin/pip install -r scripts/release/requirements.txt
export ANTHROPIC_API_KEY=...
.venv-release/bin/python scripts/release/generate_release_notes.py collect  --version 0.5.1
.venv-release/bin/python scripts/release/generate_release_notes.py generate --version 0.5.1
```

The output is written to `release/work/CHANGELOG_ENTRY.md` and `release/work/RELEASE_NOTES.md`. The `apply` subcommand makes the same repository changes as the workflow. Releases should still normally go through the workflow, so that the tag is created from a merged PR.
