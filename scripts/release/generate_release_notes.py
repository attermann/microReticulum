#!/usr/bin/env python3
"""Generate microReticulum release notes and a CHANGELOG entry with Claude.

Three subcommands, run in order (the release-prepare workflow runs each as its
own step; they work the same locally):

  collect   Gather commits, referenced PRs and contributors since the previous
            release tag into <work-dir>.
  generate  Send that context, the prompt and the current CHANGELOG.md to the
            Anthropic API; validate and write CHANGELOG_ENTRY.md and
            RELEASE_NOTES.md into <work-dir>.
  apply     Prepend the entry to CHANGELOG.md, write
            release/<version>/RELEASE_NOTES.md, and bump the version in
            library.json and library.properties.

Local dry run:
  python3 -m venv .venv-release
  .venv-release/bin/pip install -r scripts/release/requirements.txt
  export ANTHROPIC_API_KEY=...
  .venv-release/bin/python scripts/release/generate_release_notes.py collect  --version 0.5.1
  .venv-release/bin/python scripts/release/generate_release_notes.py generate --version 0.5.1
  # inspect release/work/*.md; `apply` only if you want the repo changes
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PROMPT_PATH = REPO / ".github" / "prompts" / "release-notes.md"
CHANGELOG_PATH = REPO / "CHANGELOG.md"
DEFAULT_WORK_DIR = REPO / "release" / "work"

MODEL = "claude-opus-5-5"
VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")

CHANGELOG_SECTIONS = [
    "Added", "Changed", "Fixed", "General", "Deprecated", "Migration Notes", "Contributors",
]
NOTES_SECTIONS = [
    "Highlights", "New Features", "Improvements", "API Changes", "Fixes",
    "Build and Test", "Upgrade Notes", "Contributors",
]
# Matches "courtesy of @x" including the misspellings already in history
# ("couresy", "courtey").
CREDIT_RE = re.compile(r"\bcour\w*\s+of\s+@([A-Za-z0-9-]+)", re.IGNORECASE)
PR_SUBJECT_RE = re.compile(r"\(#(\d+)\)|pull request #(\d+)")
PR_REF_RE = re.compile(r"#(\d+)\b")


class ReleaseError(Exception):
    pass


def fail(msg: str) -> None:
    prefix = "::error::" if os.environ.get("GITHUB_ACTIONS") else "error: "
    print(f"{prefix}{msg}", file=sys.stderr)
    sys.exit(1)


def warn(msg: str) -> None:
    prefix = "::warning::" if os.environ.get("GITHUB_ACTIONS") else "warning: "
    print(f"{prefix}{msg}", file=sys.stderr)


def git(*args: str) -> str:
    return subprocess.run(
        ["git", *args], cwd=REPO, check=True, capture_output=True, text=True
    ).stdout


def ref_exists(ref: str) -> bool:
    return subprocess.run(
        ["git", "rev-parse", "--verify", "--quiet", f"refs/tags/{ref}"],
        cwd=REPO, capture_output=True,
    ).returncode == 0


def resolve_previous_tag(explicit: str | None) -> str:
    if explicit:
        if not ref_exists(explicit):
            raise ReleaseError(f"previous tag '{explicit}' does not exist")
        return explicit
    try:
        return git("describe", "--tags", "--abbrev=0").strip()
    except subprocess.CalledProcessError:
        raise ReleaseError("no previous tag found; pass --previous-tag")


# --------------------------------------------------------------------------- collect

def cmd_collect(args: argparse.Namespace) -> None:
    version = args.version
    if not VERSION_RE.match(version):
        raise ReleaseError(f"version '{version}' must look like X.Y.Z")
    if ref_exists(version):
        raise ReleaseError(f"tag '{version}' already exists")

    prev = resolve_previous_tag(args.previous_tag)
    rng = f"{prev}..HEAD"
    if not git("rev-list", "--no-merges", rng).strip():
        raise ReleaseError(f"no commits in {rng}")

    work: Path = args.work_dir
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)

    commits = git(
        "log", rng, "--no-merges", "--date=short", "--stat",
        "--format=commit %H%nAuthor: %an <%ae>%nDate: %ad%n%n%s%n%n%b",
    )
    (work / "commits.txt").write_text(commits)

    # PR numbers from squash subjects "(#NN)" and from merge commits, which
    # --no-merges above leaves out.
    subjects = git("log", rng, "--format=%s")
    pr_numbers = sorted(
        {int(a or b) for a, b in PR_SUBJECT_RE.findall(subjects)}
    )
    prs = []
    if pr_numbers and shutil.which("gh") is None:
        warn("gh CLI not found; PR descriptions will not be included")
    elif pr_numbers:
        for n in pr_numbers:
            r = subprocess.run(
                ["gh", "pr", "view", str(n), "--json", "number,title,body,author,url"],
                cwd=REPO, capture_output=True, text=True,
            )
            if r.returncode == 0:
                prs.append(json.loads(r.stdout))
            else:
                warn(f"could not fetch PR #{n}: {r.stderr.strip()}")
    (work / "prs.json").write_text(json.dumps(prs, indent=2))

    authors = sorted(set(git("log", rng, "--format=%an <%ae>").splitlines()))
    credits = sorted(set(CREDIT_RE.findall(git("log", rng, "--format=%s%n%b"))))
    pr_authors = sorted({p["author"]["login"] for p in prs if p.get("author")})
    lines = ["Commit authors:"] + [f"- {a}" for a in authors]
    lines += ["", "PR authors (GitHub handles):"] + [f"- @{a}" for a in pr_authors]
    lines += ["", "Credited in commit messages:"] + [f"- @{c}" for c in credits]
    (work / "authors.txt").write_text("\n".join(lines) + "\n")

    (work / "meta.json").write_text(json.dumps(
        {"version": version, "previous_tag": prev,
         "date": dt.date.today().isoformat(), "pr_numbers": pr_numbers},
        indent=2,
    ))
    n_commits = len(git("rev-list", "--no-merges", rng).split())
    print(f"collected {n_commits} commits and {len(prs)} PRs since {prev} into {work}")


# --------------------------------------------------------------------------- generate

def _strip_fences(text: str) -> str:
    t = text.strip()
    m = re.match(r"^```(?:markdown|md)?\n(.*)\n```$", t, re.DOTALL)
    return (m.group(1) if m else t).strip() + "\n"


def _check_headings(doc: str, name: str, first_line: str, allowed: list[str]) -> list[str]:
    errs = []
    lines = doc.splitlines()
    if not lines or lines[0].strip() != first_line:
        errs.append(f"{name}: first line must be '{first_line}', got '{lines[0] if lines else ''}'")
    if sum(1 for l in lines if re.match(r"^# ", l)) != 1:
        errs.append(f"{name}: must contain exactly one '# ' heading")

    sections = [l[3:].strip() for l in lines if l.startswith("## ")]
    unknown = [s for s in sections if s not in allowed]
    if unknown:
        errs.append(f"{name}: unexpected sections {unknown}; allowed {allowed}")
    order = [allowed.index(s) for s in sections if s in allowed]
    if order != sorted(order) or len(order) != len(set(order)):
        errs.append(f"{name}: sections out of order or repeated: {sections}")

    # Empty section: a heading followed (ignoring blank lines) by another
    # heading of the same or higher level, or by end of document.
    headings = [(i, len(m.group(1))) for i, l in enumerate(lines)
                if (m := re.match(r"^(#{1,3}) ", l))]
    for idx, (i, level) in enumerate(headings):
        if level == 1:
            continue
        body_end = len(lines)
        for j, lvl in headings[idx + 1:]:
            if lvl <= level:
                body_end = j
                break
        body = [l for l in lines[i + 1:body_end] if l.strip() and not l.startswith("#")]
        if not body:
            errs.append(f"{name}: empty section '{lines[i].strip()}'")
    return errs


def validate(docs: dict[str, str], version: str, known_prs: set[int]) -> list[str]:
    errs = []
    errs += _check_headings(docs["changelog_entry"], "changelog_entry",
                            f"# microReticulum v{version}", CHANGELOG_SECTIONS)
    errs += _check_headings(docs["release_notes"], "release_notes",
                            f"# microReticulum {version}", NOTES_SECTIONS)
    first = next((l for l in docs["release_notes"].splitlines() if l.startswith("## ")), None)
    if first != "## Highlights":
        errs.append("release_notes: first section must be '## Highlights'")
    for name, doc in docs.items():
        stray = sorted({int(n) for n in PR_REF_RE.findall(doc)} - known_prs)
        if stray:
            errs.append(f"{name}: references PR/issue numbers not in the input: {stray}")
    return errs


def cmd_generate(args: argparse.Namespace) -> None:
    # Imported here so `collect` and `apply` work without the SDK installed.
    try:
        import anthropic
        from pydantic import BaseModel, Field
    except ImportError as e:
        req = Path(__file__).with_name("requirements.txt").relative_to(REPO)
        fail(
            f"{e.name} is not installed for {sys.executable}. Install the generator "
            f"dependencies into a virtualenv and run the script with it:\n"
            f"  python3 -m venv .venv-release\n"
            f"  .venv-release/bin/pip install -r {req}\n"
            f"  .venv-release/bin/python {Path(__file__).relative_to(REPO)} generate ..."
        )

    class ReleaseDocs(BaseModel):
        changelog_entry: str = Field(description="New CHANGELOG.md section, Markdown")
        release_notes: str = Field(description="User-facing release notes, Markdown")

    work: Path = args.work_dir
    meta_path = work / "meta.json"
    if not meta_path.exists():
        raise ReleaseError(f"{meta_path} not found; run `collect` first")
    meta = json.loads(meta_path.read_text())
    if meta["version"] != args.version:
        raise ReleaseError(
            f"{work} was collected for {meta['version']}, not {args.version}; re-run `collect`"
        )

    commits = (work / "commits.txt").read_text()
    prs_json = (work / "prs.json").read_text()
    user_content = f"""Release version: {meta['version']}
Previous release tag: {meta['previous_tag']}
Release date: {meta['date']}

<current_changelog>
{CHANGELOG_PATH.read_text()}
</current_changelog>

<commits>
{commits}
</commits>

<pull_requests>
{prs_json}
</pull_requests>

<contributors>
{(work / 'authors.txt').read_text()}
</contributors>
"""
    known_prs = set(meta["pr_numbers"]) | {int(n) for n in PR_REF_RE.findall(commits)}

    client = anthropic.Anthropic()
    try:
        with client.beta.messages.stream(
            model=MODEL,
            max_tokens=64000,
            thinking={"type": "adaptive"},
            output_config={"effort": "high"},
            betas=["server-side-fallback-2026-07-01"],
            fallbacks="default",
            system=PROMPT_PATH.read_text(),
            messages=[{"role": "user", "content": user_content}],
            output_format=ReleaseDocs,
        ) as stream:
            message = stream.get_final_message()
    except anthropic.AuthenticationError:
        fail("Anthropic API rejected the key (check the ANTHROPIC_API_KEY secret)")
    except anthropic.RateLimitError as e:
        fail(f"Anthropic API rate limit hit after retries: {e.message}")
    except anthropic.BadRequestError as e:
        fail(f"Anthropic API rejected the request: {e.message}")
    except anthropic.APIStatusError as e:
        fail(f"Anthropic API error {e.status_code}: {e.message}")
    except anthropic.APIConnectionError as e:
        fail(f"could not reach the Anthropic API: {e}")

    if message.stop_reason == "refusal":
        fail(f"model declined the request: {message.stop_details}")
    if message.stop_reason == "max_tokens":
        fail("model output hit max_tokens before finishing")
    docs_obj = message.parsed_output
    if docs_obj is None:
        fail(f"no structured output in response (stop_reason={message.stop_reason})")

    docs = {
        "changelog_entry": _strip_fences(docs_obj.changelog_entry),
        "release_notes": _strip_fences(docs_obj.release_notes),
    }
    (work / "CHANGELOG_ENTRY.md").write_text(docs["changelog_entry"])
    (work / "RELEASE_NOTES.md").write_text(docs["release_notes"])
    u = message.usage
    print(f"model={message.model} input_tokens={u.input_tokens} output_tokens={u.output_tokens}")

    errs = validate(docs, args.version, known_prs)
    if errs:
        for e in errs:
            print(f"  - {e}", file=sys.stderr)
        fail(f"generated documents failed validation (outputs left in {work} for inspection)")
    print(f"wrote {work / 'CHANGELOG_ENTRY.md'} and {work / 'RELEASE_NOTES.md'}")


# --------------------------------------------------------------------------- apply

def _replace_once(path: Path, pattern: str, repl: str, flags: int = 0) -> None:
    text = path.read_text()
    new, n = re.subn(pattern, repl, text, count=1, flags=flags)
    if n != 1:
        raise ReleaseError(f"could not find the version field in {path.name}")
    path.write_text(new)


def cmd_apply(args: argparse.Namespace) -> None:
    version = args.version
    work: Path = args.work_dir
    entry = (work / "CHANGELOG_ENTRY.md").read_text().strip()
    notes = (work / "RELEASE_NOTES.md").read_text().strip()

    old = CHANGELOG_PATH.read_text()
    if re.search(rf"^# microReticulum v{re.escape(version)}\s*$", old, re.MULTILINE):
        raise ReleaseError(f"CHANGELOG.md already has a v{version} section")
    CHANGELOG_PATH.write_text(entry + "\n\n" + old)

    notes_dir = REPO / "release" / version
    notes_dir.mkdir(parents=True, exist_ok=True)
    (notes_dir / "RELEASE_NOTES.md").write_text(notes + "\n")

    # Text substitution rather than json.dump, so the tab-indented formatting of
    # library.json is preserved. The top-level "version" comes before the
    # dependency versions, so the first match is the right one.
    lib_json = REPO / "library.json"
    _replace_once(lib_json, r'("version"\s*:\s*")[^"]*(")', rf"\g<1>{version}\g<2>")
    if json.loads(lib_json.read_text())["version"] != version:
        raise ReleaseError("library.json top-level version was not updated")
    _replace_once(REPO / "library.properties", r"^version=.*$", f"version={version}",
                  re.MULTILINE)

    print(f"updated CHANGELOG.md, library.json, library.properties; wrote {notes_dir / 'RELEASE_NOTES.md'}")


# --------------------------------------------------------------------------- main

def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    for name in ("collect", "generate", "apply"):
        sp = sub.add_parser(name)
        sp.add_argument("--version", required=True, help="release version, e.g. 0.5.1")
        sp.add_argument("--work-dir", type=Path, default=DEFAULT_WORK_DIR)
        if name == "collect":
            sp.add_argument("--previous-tag", help="defaults to the most recent tag")
    args = p.parse_args()
    try:
        {"collect": cmd_collect, "generate": cmd_generate, "apply": cmd_apply}[args.cmd](args)
    except ReleaseError as e:
        fail(str(e))
    except subprocess.CalledProcessError as e:
        fail(f"{' '.join(e.cmd)} failed: {e.stderr.strip()}")


if __name__ == "__main__":
    main()
