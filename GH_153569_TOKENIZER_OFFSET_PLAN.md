# gh-153569 tokenizer offset migration: status, plan, and handoff

Last checked: 2026-09-07

Tracking issue: https://github.com/python/cpython/issues/153569

This document records the current state of the tokenizer offset migration,
the plan recovered from the original development conversations, the source
branches and commits, the review boundaries that were agreed after the first
attempt was too large, and the validation needed before each remaining change
is submitted.

## Executive status

The issue remains open. Four prerequisite PRs are merged:

| PR | Scope | State |
|---|---|---|
| [#153585](https://github.com/python/cpython/pull/153585) | Split the tokenizer lexer into focused files | Merged; merge commit `a2d3787105d1` |
| [#153587](https://github.com/python/cpython/pull/153587) | Add decoded source, spans, locations, and offset-only cursor primitives | Merged; merge commit `f54fd2ab6e1f` |
| [#156472](https://github.com/python/cpython/pull/156472) | Common tokenizer reader and decoder | Merged |
| [#156482](https://github.com/python/cpython/pull/156482) | Return token spans and unify decoded storage | Merged; merge commit `09117bc3173b` |

The active review stack now has eight drafts. The first remains in
`python/cpython`; the seven dependent drafts are in `pablogsal/cpython`,
each based on its predecessor branch so GitHub shows the incremental diff.
The former cumulative CPython PR #156654 is closed and links to this split.

| PR | Scope | Validated tip |
|---|---|---|
| [python/cpython#156484](https://github.com/python/cpython/pull/156484) | Source spans and derived locations | `0b9a33b57560c55baebd9aed35fbdf220aec1944` |
| [pablogsal/cpython#138](https://github.com/pablogsal/cpython/pull/138) | Reader ownership and source views | `730212bc8195d23d2ba4a7d1ada3c10fd122690b` |
| [pablogsal/cpython#139](https://github.com/pablogsal/cpython/pull/139) | Formatted-string frames and transitions | `04b05de3e514f175ff160f8a921c07acbbe3ab58` |
| [pablogsal/cpython#140](https://github.com/pablogsal/cpython/pull/140) | Indentation and logical-line state | `db92c94c066d76c1bbf4becb518d7fa12f2e0849` |
| [pablogsal/cpython#141](https://github.com/pablogsal/cpython/pull/141) | Unused cursor removal | `d74ab27f045f40c4ef079e679c24195133f7ea3f` |
| [pablogsal/cpython#142](https://github.com/pablogsal/cpython/pull/142) | Unused source lookup removal | `4b7786605c9b107d4b548c934a48d560694bc937` |
| [pablogsal/cpython#143](https://github.com/pablogsal/cpython/pull/143) | Explicit diagnostic state | `ba20f3810ab37e3c2a028a0f3376cd3c78e145a2` |
| [pablogsal/cpython#144](https://github.com/pablogsal/cpython/pull/144) | Opaque API and persistent offsets | `6f220e47077282617d9c5f50c5cf6dfb8de9931b` |

Storage ownership, remaining persistent offsets, explicit diagnostics, and
the opaque consumer API remain implemented across this stack. Validation
tooling remains a separate follow-up.

The August 27 branch inventory and prototype commits below are historical
recovery material. The active stack above supersedes the old instruction to
carve out a reader/decoder PR.

## Why this work exists

The tokenizer has historically represented source positions with pointers
into buffers that can move. Growing or replacing a buffer requires every live
pointer to be found and rebased correctly. Tokens, the cursor, error state,
f-string state, and consumers have all depended on this convention. Missing a
single pointer creates use-after-free, corruption, or incorrect-location bugs.

The old implementation also has separate input paths for strings, UTF-8
strings, files, `readline()` callables, and interactive input. Those paths
duplicate underflow, newline, encoding, and buffer-ownership behavior. Fixes
can therefore land in one path without fixing the others.

The architectural goals, adjusted for the current storage decision, are:

1. One decoded-storage abstraction owns retained bytes and preserves bounded
   streaming windows.
2. Positions are offsets into that source, never persistent interior pointers.
3. Tokens and errors describe half-open source spans.
4. Persistent scanner positions use offsets; temporary pointer caches stay
   within the scanning/storage boundary. The unused cursor prototype is removed
   in #156654 as part of the opaque API cutover.
5. A common reader/decoder pipeline handles every source kind.
6. Lexer and f-string state move from pointers to offsets and spans.
7. Pegen and `_tokenize` consume an opaque tokenizer API rather than internal
   fields.
8. The old reader implementations and compatibility surface disappear after
   all consumers have moved.

There is no public C tokenizer API that needs a deprecation cycle. The public
contract is Python-level behavior: token streams, token numbering, source
text, positions, exception types and messages, incomplete-input behavior,
interactive prompt timing, and tolerant tokenization.

## Decisions made in the original conversations

These decisions supersede parts of the early `TOKENIZER_REDESIGN.md` design
document:

- Do not call the implementation `tokenizer2`.
- Do not leave a second legacy tokenizer behind after the cutover.
- Do not add a runtime switch between two implementations.
- Do not submit the integration rewrite as one large PR.
- Keep each intended PR independently buildable and testable.
- Every review branch must build and pass its relevant tests at its exact tip.
- Behavior coverage belongs in the PR that needs it; there is no standalone
  coverage PR.
- Keep each PR conceptually focused even if that means temporarily preserving
  compatibility entry points.
- Avoid changes whose rename/deletion churn makes the implementation hard to
  review or revert.
- Keep the historical integration stack as reference. Maintain the active
  review stack by updating the earliest affected PR and propagating through
  its dependents; remove merged dependencies when rebasing onto upstream.

The original attempt at the second PR accidentally combined `SourceText`, the
cursor, the reader/decoder, errors, lexer state, pegen, `_tokenize`, and legacy
deletion. It produced a 50-file change with roughly 5,266 additions and 3,241
deletions. That was rejected as unreviewable. PR #153587 was force-rewritten
to contain only the source/cursor foundation. This failure is why the branch
boundaries in this document are strict.

## Foundation history

### Lexer split: PR #153585

Number scanning and string/f-string scanning were moved out of the lexer
mega-file into focused files. Relevant behavior coverage was included in the
same PR. This was intended to be a behavior-neutral structural change that
makes the later offset conversion easier to review.

### Source and cursor primitives: PR #153587

As merged, the source foundation provided the following primitives. Some were
never adopted by the production lexer and are removed by #156654:

- `_PyTok_SourceText`, which owns decoded source bytes.
- `_PyTok_Span`, a half-open byte range into the source.
- Offset-to-line and offset-to-location lookup.
- Sparse line checkpoints rather than one heavyweight record per line.
- Implicit-newline and NUL metadata.
- `_PyTok_Cursor`, which contains an offset and cached line bounds rather than
  pointers that need rebasing.
- Explicit line-boundary affinity.
- Transient source views. Views must not survive an append or clear, while
  offsets and spans remain meaningful.
- Atomic logical-line append semantics. Bytes and line metadata cannot be
  left partially committed if allocation fails.
- An O(1) implicit-line predicate added after downstream API review.

The API was reviewed against the planned string, file, `readline()`, and
incremental-decoder consumers before merge. A possible split between byte
append and line registration was rejected because it permits inconsistent
intermediate state and worse allocation-failure semantics. If bulk insertion
is ever needed, the preferred extension is a transactional multi-line append,
not exposing partially registered source data.

## Historical fork branch inventory (August 27)

The counts below are relative to `upstream/main` as checked on 2026-08-27.

| Fork branch | Ahead | Purpose at that date | Diff or top-commit size | Submit directly? |
|---|---:|---|---|---|
| `gh-153569-tokenizer-lexer-split` | 0 | Historical PR #153585 branch, now pointing at upstream after merge | No remaining diff | No work remains |
| `gh-153569-tokenizer-offset-api` | 0 | Historical PR #153587 branch, now pointing at upstream after merge | No remaining diff | No work remains |
| `gh-153569-tokenizer-coverage` | 1 | Preserved behavior tests | 5 files, +188/-3 | No; dissolve into functional PRs |
| `gh-153569-tokenizer-offset-conversion` | 2 | Coverage plus the combined migration implementation | 58 files, +5,035/-3,932 in total | Never |
| `gh-153569-tokenizer-offsets` | 5 | Full rebased integration stack | 63 files, +5,627/-3,936 in total | Never |
| `gh-153569-tokenizer-readline-chunks` | 4 | Integration stack through the readline fix | 58 files, +5,141/-3,936 in total | No; top commit is later source material |
| `gh-153569-tokenizer-validation-tools` | 5 | Integration stack through validation tooling | Same tip as the full stack | No; top commit is later source material |
| `gh-153569-tokenizer-redesign` | 1 | Squashed integration/safety snapshot | 62 files, +5,528/-3,936 | Never |

The branch names can be misleading. `readline-chunks` and
`validation-tools` contain their entire dependency stack. They are not small
standalone diffs against upstream. These historical branches must not replace
the current review stack. Reuse their implementation ideas only within the
appropriate current review boundary.

## Historical rebased source commits

The integration stack recorded on August 27 was:

| Commit | Change | Individual size | Intended treatment in August |
|---|---|---|---|
| `2d43b0f5158` | Expand tokenizer behavior coverage | 5 files, +188/-3 | Distribute tests into the PRs they protect |
| `7fd9ad7e10e` | Base the tokenizer API on source offsets | 54 files, +4,847/-3,929 | Reference only; split into reader/decoder, offset state, and API cutover |
| `b62ebe81b96` | Avoid materializing unused parser token text | 1 file, +21/-8 | Do not include in gh-153569 work; overlaps gh-153568/#153576 |
| `0af9c4d9afe` | Preserve readline chunks across incremental decoding | 2 files, +89 | Reference for a later dedicated PR |
| `98fb37b1a8f` | Add tokenizer validation tools | 5 files, +486 | Reference for the final validation PR |

The squashed recovery snapshot is `f08d53933b1`, titled `Tokenizer redesign
integration snapshot`.

Before the rebase, the same source material had these hashes:

| Original commit | August 27 equivalent |
|---|---|
| `c044cdf75ad` | `2d43b0f5158` |
| `485acfcba70` | `7fd9ad7e10e` |
| `dcfc2213fa6` | `b62ebe81b96` |
| `ff5f002bfa2` | `0af9c4d9afe` |
| `f13f458e9ca` | `98fb37b1a8f` |

Never cherry-pick either `485acfcba70` or `7fd9ad7e10e` into a review
branch. Both are integration checkpoints, not review units.

## Current PR scopes after redistribution

### #156482: token spans and unified source storage

SourceText owns decoded bytes for every input kind. Its logical base advances
when a streaming window is discarded; discard reuses the allocation and
clears line metadata. File/readline staging buffers remain separate from the
canonical decoded source. Prepared and interactive input retain their source.

The reader temporarily saves and restores persistent pointers around append
in this prerequisite PR. The source allocator forces relocation and poisons
old allocations in debug builds. The final PR removes pointer rebasing after
all persistent scanner positions become offsets.

Tests protect relocation in f/t strings, both extra-token modes, interactive
multiline input, discard/reuse, implicit-line metadata reset, and logical
offset limits. The bitset byte count avoids addition overflow on 32-bit builds.

### #156484: consolidate tokenizer state around source spans

Use one frame per active formatted string, with an inline common-case slot
and a growable nesting stack. Frames hold kind, quote, opening location,
mode, replacement depth, expression span, and comment spans. Raw-string
context travels with emitted and cached parser tokens.

The reader owns input-specific state, tokenizer construction, and temporary
pointer relocation. Locations and failures derive from existing scanner
state, removing duplicate counters and flags. RetainedSource distinguishes
prepared/interactive input from streaming windows; SourceLineView owns the
retained-source line scan.

The existing unused cursor and sparse index remain unchanged in this PR.
Their removal belongs to #156654's API cutover. Scanner positions remain
pointers here; all decoded storage is still owned by SourceText from #156482.

### #156654: finish offsets, diagnostics, and the opaque API cutover

All persistent scanner positions are offsets, including token starts and
line starts. SourceText is the sole owner of decoded bytes; the reader no
longer saves or restores interior pointers. Temporary byte views remain
within their documented lifetimes. The small character-read loop is inline;
refill, callbacks, and NUL validation stay in a separate function.

String, BOM, UTF-8, and invalid-identifier errors report explicit text and
locations without changing the scanner's position. Source-backed string
errors additionally retain a reporting location and text span for parser
reinterpretation as incomplete input. This preserves multi-line exception
text. Exceptions own the rendered text; no borrowed raw decoder line is
stored in persistent state. Existing done codes retain their classifications.

The single consumer header, tokenizer.h, exposes an opaque tok_state and
value records for tokens, views, observations, and diagnostics. Pegen and
_tokenize use configuration, input-control, token, source-view, and diagnostic
operations. They no longer include lexer state or inspect its fields.
The unused cursor, source location/index lookup, and old source span-view API
are removed along with their tests and build entries.

Token views preserve multiline TokenInfo.line and character-column behavior.
SourceLineView owns the retained-source line scan, with 1-based clamping and
borrowed non-NUL-terminated views. Windows core/freezer and standalone PEG
extension builds include the new API implementation.

## Remaining implementation plan

### Validation tooling


Reference material: historical commit `98fb37b1a8f`.

- Add repeatable differential token-stream and error-location checks over
  normal source corpora, including the standard library, tests, and focused
  encoding/newline/f-string inputs.
- Add repeatable throughput and peak-memory measurements, including large
  streaming input and long active multiline constructs.
- Place new fuzz coverage in `python/library-fuzzers`, following review
  feedback, rather than introducing it into these implementation PRs.

Tools may be used locally before a tooling PR is ready. There is no standalone
behavior-coverage PR: regression tests and forced-relocation checks stay in
the implementation PRs they protect.

### Historical slices that are not new PR instructions

The July 18 sequence listed six follow-ups after #153587, and the August 27
handoff reduced that estimate to five. Those counts are superseded by the
merged reader/decoder, the current three-PR stack, and the remaining validation tooling above. Do not infer a fixed final PR
count from the old estimates.

The incremental `readline()` chunk-preservation prototype is commit
`0af9c4d9afe`. The current reader handles multiple logical lines returned by
one callback and has tests for chunk tails and callback timing. Preserve and
validate that behavior; do not recreate the obsolete dedicated chunk-fix PR
instruction without identifying a remaining gap.

The unused parser-token-text optimization was tracked separately by gh-153568
and [#153576](https://github.com/python/cpython/pull/153576). The historical
`b62ebe81b96` commit overlaps that work and must stay out of this stack. Its
current issue/PR status is not tracked by this handoff. Any future omission
of token text needs an audited proven-dead token list: f/t-string starts,
Barry-as-FLUFL handling, and retyped keyword tokens can still need token bytes.

## Public behavior that must not change

Each functional PR must protect at least the following:

- Token numbers and names generated from `Grammar/Tokens`.
- `tokenize` stream shape, including `ENCODING`, `NL` versus `NEWLINE`,
  comments, implicit newlines, trailing `DEDENT`/`ENDMARKER`, and CRLF text.
- Character column offsets exposed by Python APIs even though internal spans
  use decoded UTF-8 byte offsets.
- F-string and T-string splitting, including escaped-brace column gaps.
- Exact syntax error and `TokenError` types, messages, and locations.
- `IndentationError` and `TabError` behavior.
- Incomplete-input behavior used by `codeop`, the REPL, and
  `PyCF_ALLOW_INCOMPLETE_INPUT`.
- Interactive prompt timing.
- `readline()` callback timing and side effects.
- Tolerant `extra_tokens` mode used by `_pyrepl` while highlighting incomplete
  and invalid input.
- Syntax-warning behavior for malformed numeric literals.
- `_tokenize.TokenizerIter` arguments and token-line semantics.

## Architectural invariants to preserve

Preserve these constraints in this stack and future tooling:

1. Retained source bytes keep their logical offsets when storage relocates or
   an earlier streaming window is discarded. Offsets do not promise that
   discarded source remains accessible.
2. Source storage is the sole owner of canonical decoded bytes.
3. Consumers retain offsets or spans, not source pointers.
4. Returned source views are bounded and transient.
5. Temporary scanning pointers must be refreshed at every operation that can
   relocate their source storage; callers must not predict allocator growth.
6. Lexer lookahead must not pull a later logical line because doing so changes
   interactive prompts and callback timing.
7. Backward movement through already loaded source must not rewind or invoke
   the reader again.
8. Allocation failure must not partially commit source bytes or metadata.
9. Diagnostic locations are independent of scanner position. Rendered
   exceptions own their text; supplementary source-backed diagnostic spans
   remain retained in terminal tokenizer state.
10. Keep one frame per active formatted string with a mode and replacement
    depth, one inline slot, and a growable nesting stack. This intentionally
    differs from the original proposal of separate frames for each body,
    replacement-expression, and format-spec context. Changing to that older
    proposal is not a requirement for this stack.

## Source retention and memory

The current decision preserves bounded streaming storage for file and
`readline()` input. Consumed windows can be released/reset while logical
`buf_offset` advances. A long active token or formatted string can still
require a large retained window; bounded streaming does not mean a fixed
constant-memory limit independent of the active construct.

Prepared and interactive input retain decoded `SourceText`, supporting their
source views and diagnostics. Unified storage preserves these source-kind retention semantics. Streaming
input is not retained in full.

The original integration prototype instead kept all decoded source in one
contiguous append-only buffer. Its reported 100 MiB workload peak of about
119.1 MiB is historical evidence for that prototype, not a measurement of the
current implementation. The former plan to accept full streaming retention
and consider a low-watermark scheme later is superseded by the current
bounded-streaming decision. Measure current throughput and peak memory on
both ordinary input and long active constructs before drawing conclusions.

## Validation history

The original six-commit prototype reported the following before the branches
were rebased:

- Every original commit passed a complete CPython suite independently,
  approximately 51,265-51,270 tests and 479 runnable test files.
- 6,052 differential cases produced no differences.
- Focused ASan and UBSan runs passed.
- Refleak checks passed.
- `regen-all`, `patchcheck`, and C-global analysis passed.
- Windows project XML validation passed.
- The 100 MiB memory workload peaked at approximately 119.1 MiB.
- No tokenizer or lexer source file in the final prototype exceeded 798
  lines.

These results describe the historical prototype, not an endorsement of the
rebased integration branches as ready to merge. Rebasing changes commit IDs
and upstream context. Each carved PR must be rebuilt and retested on its own
tip.

## September 6 redistribution into two architectural steps

The user requested coherent, more balanced review boundaries. The first PR
consolidates state and source-span ownership; the second finishes persistent
offsets, explicit diagnostics, and opaque consumers, and removes the obsolete
API surface. The initial redistribution used one commit per PR; the reviewable
commit sequences below replace that history without changing either final tree.

| Incremental review | Added | Deleted |
|---|---:|---:|
| #156484 against main | 846 | 862 |
| #156654 against #156484 | 693 | 1,158 |

Both PRs target upstream main, so GitHub displays the dependent PR's cumulative
diff until #156484 lands. After that merge, rebase #156654 to expose only its
remaining cutover diff.

The combined tree is exactly the previously reviewed tree from `7e5d82cb08a`:
`a9d4c7117674d2151e6d7d3faac7028232498e02`. No final implementation changes
were added or lost. The intermediate state was independently reviewed for
source retention, frame/token lifetime, old API compatibility, and Unix,
Windows core/freezer, and PEG build integration.

RetainedSource and token rawness moved into the first step to keep that
intermediate state correct. Existing cursor/index code stays identical to
main until its removal in the second step. PR titles and descriptions explain
only the changes and dependencies; validation details remain in this handoff.

The first PR passed 52,027 full debug tests, 98 PEG tests, 368 reference-leak
checks, and 612 debug ASan/UBSan tests. Its 156 token/AST corpus results,
557 syntax outcomes, and 243 incomplete-input results match the baseline.
Logs are under `/tmp/tokenizer-pr-review/build-foundation` and
`build-foundation-asan`, named `balanced-*.log`. Differential results are in
`balanced-foundation-comparisons.json`; static checks for both PRs are in
`balanced-static-checks.json`.

The second PR also passed 52,027 full debug tests, 98 PEG tests, 369
reference-leak checks, and 613 debug ASan/UBSan tests. Logs are
`build-accessor/balanced-*.log` and `asan-candidate/balanced-*.log`.
The unchanged final tree reused its existing binary for the full run; its
banner still names 7e5d82. Build-version metadata was then refreshed to the
new commit and focused tokenizer/source tests passed again.

Those endpoint checks used `state-foundation` for #156484 and `pr-156654`
for #156654, under `/tmp/tokenizer-pr-review`. The current review worktrees
are listed below.

## September 7 split into small draft PRs

The user requested at most 700 added and 500 deleted lines in every PR and
all new PRs as drafts. These are the verified GitHub display sizes, not
sizes measured against an undisplayed dependency:

| PR | Added | Deleted | Commits |
|---|---:|---:|---:|
| [python/cpython#156484](https://github.com/python/cpython/pull/156484) | 308 | 315 | 2 |
| [pablogsal/cpython#138](https://github.com/pablogsal/cpython/pull/138) | 210 | 263 | 2 |
| [pablogsal/cpython#139](https://github.com/pablogsal/cpython/pull/139) | 463 | 358 | 2 |
| [pablogsal/cpython#140](https://github.com/pablogsal/cpython/pull/140) | 241 | 178 | 1 |
| [pablogsal/cpython#141](https://github.com/pablogsal/cpython/pull/141) | 4 | 303 | 1 |
| [pablogsal/cpython#142](https://github.com/pablogsal/cpython/pull/142) | 16 | 380 | 1 |
| [pablogsal/cpython#143](https://github.com/pablogsal/cpython/pull/143) | 167 | 79 | 1 |
| [pablogsal/cpython#144](https://github.com/pablogsal/cpython/pull/144) | 567 | 450 | 3 |

Diagnostic line views move with reader/source ownership, keeping the
formatted-string PR focused on frames, token context, and transitions.
The former unused-API commit is split into cursor removal and source lookup
removal; the cursor-only boundary retains all source lookup implementations
and tests until their own removal. The final cutover remains three focused
commits: opaque consumers, persistent offsets, and complete token results.

All eight boundaries built and passed the eight focused modules, with
496–498 tests per boundary. The reordered reader/source boundary was built
and tested separately; seven other final boundary trees exactly match the
freshly tested snapshots. Windows project/filter and Unix source registrations
and whitespace checks pass. Independent review found no missing cursor
references or build/test dependencies at the new removal boundary.

The final tree, `6f65c5bfc2160b2035b0c8b329054f91a93ea0cf`, is identical
to previously published `0eb2ce01076`. The split preserves every implementation
change, the 52,027-test final full-suite result, differential outcomes, and
performance measurements documented in the preceding implementation round.
Artifacts are under `/tmp/tokenizer-pr-review`:
`small-pr-published-series.json`, `small-pr-final-validation.json`,
`small-pr-github-verification.json`, and the logs in `build-small-pr-series`.
The complete final history is local branch `tokenizer-small-pr-order` in
`small-pr-order`, at `6f220e47077`.

CPython repository rules rejected creation of temporary review-base branches
with GH013 (“Cannot create ref due to creations being restricted”). The
atomic push created no upstream branches, and repository rules were not
changed. The user explicitly chose the fork for the dependent drafts.

The main Tests workflow only runs automatically for main/release PR bases.
It was dispatched explicitly on all seven fork head branches. Those remote
runs are pending; local success is not a claim that fresh CI is green.
The first draft uses the normal upstream-main PR workflow.

The fork PRs are review artifacts. After each predecessor lands, rebase the
next branch onto current upstream main and open its corresponding CPython
landing PR as a draft. GitHub cannot move an existing PR between repositories.

## September 7 layout state and complete token results

#156484 adds `0c5a5214d28`, its seventh focused commit. `layout.c` owns
indentation checks, logical-line starts, continuations, pending indentation
tokens, and newline classification. One embedded layout state pairs normal
and alternate columns in each indentation level. The reader's implicit-newline
provenance and the scanner's bracket depth remain in their existing state.
The change adds 241 lines and removes 178 across 12 files; most of the new
file is code moved out of the normal scanner.

#156654 has five additional commits: `355914a795a`, `ca42d71ed0a`,
`e3991f5c668`, `5c4b417dcf8`, and `0eb2ce01076`. The final commit puts
kind in the existing token result alongside span, locations, raw context,
and metadata. `Get` replaces that result and returns void; `GetView` no
longer accepts a separate kind. Parser keyword classification and its
synthetic newline remain explicit parser operations. Borrowed views retain
their existing lifetime: adding a kind does not make a line view independent
of later source mutation.

Token initialization covers every field. Reusing a token releases its old
metadata, and failed parser arena transfers leave that reference with the
source token for cleanup. Cleanup is an inline `Py_CLEAR`, avoiding an
out-of-line call for the usual token without metadata. This adds 36 lines
and removes 28 across seven files. On this LP64 build the transient token
is 56 bytes instead of 48; cached parser tokens do not grow. `tok_state`
remains 2,896 bytes. Neither change adds an allocation.

GitHub displays twelve cumulative commits for #156654 until #156484 lands.
The layout conversion to offsets stays in the persistent-offset commit;
the opaque API commit routes implied dedents through the layout operation.
Independent reviews checked ownership, layout behavior, efficiency, build
registration, and the replayed commit boundaries without outstanding findings.
PR descriptions discuss the implementation and dependencies only.

The first endpoint passed 52,027 full debug tests, 497 focused tests, and
270 reference-leak checks. The final second endpoint passed 52,027 full debug
tests, 270 reference-leak checks, 98 PEG tests, and the standalone C contract
harness. A preceding full run also passed before cleanup was inlined; the
full run was repeated after that change. Release assembly confirms there is
no longer an out-of-line cleanup call in Get or its consumers.
Both endpoints match the previous `a497ca0a5b5` implementation on 156 token/AST
files, 557 syntax diagnostics, 243 incomplete-input outcomes, and 663 layout
cases. Each layout case checks normal and extra-token streams, AST or error
results in exec and single modes, and incomplete-input classification.

Artifacts are under `/tmp/tokenizer-pr-review`: `layout-token-comparisons.json`,
`layout-token-static.json`, `compare_layout.py`, and `token-result-check.c`.
The static checks cover both endpoint diffs, patchcheck, and Unix/Windows/PEG
source registrations. The standalone C harness checks token kinds, raw
context, metadata reuse, line views, fatal-status overrides, and error tokens
without Python exceptions, without adding exported test-only APIs.
Logs are `build-state-review/layout-{build,focused,full,refleak}.log` and
`build-opaque-review/layout-token-final-{build,full,refleak,peg,contract}.log`.
The complete source snapshot, `formatted-string-state-machine.md`, now
contains seven complete files including layout and the tokenizer header.

Twelve paired, alternating release samples on CPU 2 compared the previous
`a497ca0a5b5` binary with the final implementation, after local builds and
full tests finished. Median paired process-CPU changes were:

| Workload | Change |
|---|---:|
| Ordinary compile | +1.01% |
| Ordinary tokenize | +0.59% |
| Extra-token tokenize | +1.32% |
| Formatted compile | +2.65% |
| Formatted tokenize | +0.37% |
| Nested-layout compile | +0.53% |
| Nested-layout tokenize | +0.64% |

The tokenization measurements suggest a small cost, about 0.4–1.3%, for the
clearer layout operations and complete token-result contract. Compilation
measurements remain noisy; this is not evidence of performance neutrality
or a speedup. These are exploratory microbenchmarks, not a full pyperformance
run. Results and binary provenance are in
`layout-token-performance-{raw,summary,provenance}.json`. The noisier first
run, before cleanup inlining and while tests were running, is preserved as
`initial-layout-token-performance-*` and was not used for the final numbers.

## September 6 explicit formatted-string transitions

At this stage #156484 had six focused commits. Its new final commit, `324737d6de8`,
centralizes formatted-string transitions in `Parser/lexer/string.c`.
#156654 retained four additional commits, rebased onto that cleanup:
`7a33bf0a52a`, `ffc4f9814db`, `3fbf7591630`, and `a497ca0a5b5`.
GitHub shows ten cumulative commits for #156654 while both PRs target main.

The machine uses the existing three enum modes and one frame per active
formatted string. Dispatch explicitly handles expression, middle, and
format-specification modes. Semantic operations own field entry, punctuation
and metadata completion, field closing, and debug-expression marking.
Generic Python operators and bracket matching remain in the ordinary scanner.
No additional state fields, allocation paths, or text copies were introduced.

The cleanup preserves the ordering of metadata capture, bracket updates,
`!=` recognition, debug `=`, nesting-limit diagnostics, and nested format
continuations. In particular, closing a nested field still returns to MIDDLE.
All mode/depth/debug state mutations now live in `string.c`.

Independent reuse, quality, and efficiency reviews informed the extraction;
the exact initial diff had no correctness findings. The final C99 refinement
replaced a combined completion hook with named closing/debug operations and
made enum-mode dispatch explicit. The first three #156654 commits replayed
unchanged; the final offset commit adapts the saved expression start to an
offset in the extracted entry path.

Both final PR tips passed 52,027 full debug tests. Focused checks passed
497 tests for #156484 and 498 for #156654; reference-leak checks passed 267
and 268 tests, respectively. The final #156654 build also passed 98 PEG tests.
Both tips matched their previous trees on 156 token/AST files, 557 syntax
outcomes, and 243 incomplete-input outcomes. Patchcheck, diff whitespace,
and build-file consistency checks passed. Final independent review found no
correctness or reuse issues.

Logs are `build-state-review/transitions-c99-{build,full,refleak}.log` and
`build-opaque-review/transitions-final-{build,focused,full,refleak,peg}.log`.
Differential and static results are `transitions-c99-comparisons.json` and
`transitions-final-static.json`. A complete source view of the five lexer
files is `formatted-string-state-machine.md`. All are under
`/tmp/tokenizer-pr-review`.

Release comparison used identical inputs and twelve alternating paired
samples on CPU 2. Median process-CPU changes were +1.18% ordinary compilation,
-0.77% ordinary tokenization, -0.02% extra-token tokenization, -1.60% formatted
compilation, and -1.31% formatted tokenization. No consistent slowdown appeared,
but outliers and wide intervals make small performance effects inconclusive.
These results do not prove neutrality or a speedup. Artifacts are
`transition-performance-{raw,summary,provenance}.json`. Static inspection found
no new per-character or ordinary-token calls; the normal scanner's compiled
code shrank by 448 bytes.

The compatibility requirement is no regressions; behavior improvements are
accepted. Existing removal of a tokenization MemoryError and correction of
invalid-UTF-8 diagnostic columns are intentionally retained.

## Earlier September 6 reviewable commit sequences

At this stage #156484 had five commits, with four additional commits in
#156654. Both PRs targeted main, so GitHub showed nine cumulative commits
for #156654. Each commit formed a complete, buildable change.

| PR | Commit | Change |
|---|---|---|
| #156484 | `baef09eb7dc` | Store formatted-string text as source spans |
| #156484 | `0b9a33b5756` | Derive locations and failures from scanner state |
| #156484 | `8f92f4e89f4` | Move input state and relocation into the reader |
| #156484 | `2b393f8f610` | Keep active formatted-string frames and token context |
| #156484 | `177eca581b4` | Borrow diagnostic lines through the source API |
| #156654 | `0f978add630` | Remove unused cursor and source lookup APIs |
| #156654 | `8689442e347` | Report diagnostics without rewinding the scanner |
| #156654 | `438567c1ab7` | Hide tokenizer state behind the consumer API |
| #156654 | `fefec399906` | Store scanner positions as logical source offsets |

The #156484 endpoint tree is `a83052357f8101363468f5e0909834c020ff2056`,
identical to the previous `3ef307f12bf` tip. The #156654 endpoint tree remains
`a9d4c7117674d2151e6d7d3faac7028232498e02`, identical to `924080632af`.
All requested implementation changes are preserved exactly.

An independent agent reviewed the boundaries and found no outstanding issues.
The scanner-bookkeeping commit includes all affected initialization and field
consumers. Reader ownership introduces retained-source access together with
its consumers. Explicit diagnostics and opaque access each work before the
final conversion from pointers to offsets.

All five #156484 commits built and passed their focused suites: 496 tests for
the first three and 497 for the final two. All four #156654 commits built and
passed 152, 496, 498, and 498 focused tests, respectively. The diagnostic,
opaque-API, and offset commits also matched the baseline for 557 syntax and
243 incomplete-input outcomes; the last two matched 156 token/AST cases.
The opaque-API step passed 98 PEG tests, and the offset step passed 369
reference-leak checks. Reparenting the four commits onto the five-commit
#156484 chain preserved every intermediate tree.

Both rewritten PR tips passed fresh full debug runs of 52,027 tests each.
The #156484 tip also passed 98 PEG tests and 368 reference-leak checks.
Logs are `commit-series-full.log` in each current build directory, plus
`commit-series-peg.log` and `commit-series-refleak.log` in `build-state-review`.
The #156654 PEG and reference-leak results above cover the identical trees
before reparenting.

Every commit passed diff whitespace and Windows project/source consistency
checks. Both endpoint diffs passed patchcheck and build-file consistency
checks. Evidence is in `commit-series-static.json`,
`commit-series-final-static.json`, `build-opaque-review/p3-commit-validation.json`,
and `build-opaque-review/p3-reparent-trees.json` under
`/tmp/tokenizer-pr-review`.

Current source worktrees are `state-review-commits` for #156484 and
`opaque-review-commits` for #156654. Build and test logs are in
`build-state-review` and `build-opaque-review`, respectively, all under
`/tmp/tokenizer-pr-review`. Earlier ASan/UBSan results above remain applicable
to these identical endpoint trees. PR descriptions contain no validation
sections or instructions.

## Earlier September 6 rebase before redistribution

#156482 landed as `09117bc3173b6854f3d614fb0efe79bca4b4cc63`. Both
remaining PRs are rebased onto that commit. #156484 contains only the two
cleanup commits; #156654 retains its four commits on top of #156484.
`git range-diff` reports every replayed commit unchanged. No conflicts or
implementation edits were needed, and the merged prerequisite is absent from
the remaining review diffs.

Both debug builds, PEG tests, reference-leak checks, incremental/full diff
checks, patchcheck, and Windows project/source checks passed. Full debug
suites ran sequentially to avoid the earlier shared perf mapping failures.
#156484 passed 52,022 tests; #156654 passed 52,027. Each passed 98 PEG tests;
reference-leak checks passed 364 and 369 tests respectively.

Both initial build banners contained a stale dirty suffix after rebasing.
The source worktrees and commit diffs were verified clean. Build-version
metadata was refreshed and focused tokenizer/source tests passed again; the
full-suite logs retain their original banners.

Validation logs are in `debug-156484/landed-rebase-*.log` and
`build-accessor/landed-rebase-*.log` under `/tmp/tokenizer-pr-review`.
Static results are in `landed-rebase-static-checks.json`.

## September 5 review before #156482 landed

The then-current tips 6117ae337c2, da9dfb7d5ce, and 6a267edcb6f
included upstream main `7a918411a30`. Review covered each
incremental PR and its full diff against main, with independent reuse,
correctness/API, and efficiency passes. No implementation work remains for
the three requested migrations.

The previous #156654 CI merge failed to compile after upstream #156901 added
pointer-based diagnostic calls. Rebasing the stack and passing the explicit
reporting-line view fixes that integration. Review also found a byte/character
column mismatch for invalid UTF-8 following non-ASCII text. The reporting
cursor now derives the character column once. New byte-compilation and FILE
input tests expect column 7; current main incorrectly reports column 6.
Borrowed-view invalidation and source ownership contracts were corrected.
Three unreachable pointer-era cursor checks were removed from the normal
scanner after profiling; cursor offsets are always nonnegative.

| PR | Full debug validation at the recorded tip |
|---|---|
| #156482 | 52,011 tests; only shared perf-profiler resource failures; all 14 profiler tests passed in the sequential rerun |
| #156484 | 52,009 tests; only shared perf-profiler resource failures; all 14 profiler tests passed in the sequential rerun |
| #156654 | 52,014 tests, successful without retry |

P1/P2 simultaneous full runs exhausted shared perf mapping resources
(`perf_event_mlock_kb`); their sequential profiler reruns passed. The original
full runs are not classified as unconditional successes. Platform/resource
skips remain. P3 also passed 98 PEG-generator tests, 369 tests under `-R 3:3`
with no measured reference leaks, and 613 tests under debug ASan/UBSan with
ASan leak detection disabled. Debug builds force source relocation.

The exact final P3 binary matches current main on 156 complete token/AST
files, 557 syntax outcomes, and 243 incomplete-input outcomes. The corpus is
read from the current-main checkout, so both binaries consume identical
files. The new UTF-8 regression tests separately verify the intended fix.

Every incremental/full diff passes whitespace and patch checks. Windows
core/freezer project XML and source/filter entries agree; the standalone PEG
build includes the new API source. Worktrees are clean. Native Windows and
macOS validation is delegated to GitHub CI.

Nine alternating release measurements pinned to CPU 31 compared the exact
final tip with current main, using the same GCC and configure settings:

| Workload | Current main | Final stack | Change |
|---|---:|---:|---:|
| Compile four standard-library modules | 27.290 ms | 26.569 ms | -2.64% |
| Compile 1,000 formatted-string assignments | 8.076 ms | 5.918 ms | -26.72% |
| Tokenize 10,000 short assignments | 22.127 ms | 23.030 ms | +4.08% |

The cursor-check cleanup reduced the tokenization gap from +6.26% in the
preceding quiet run. A residual short-line overhead remains; these results
are not a no-regression claim or a general performance guarantee. Three
100 MiB streaming runs per binary gave median peak RSS of 17,868 KiB on main
and 17,828 KiB on the final stack. No additional API complexity is proposed
on the current evidence.

Evidence under `/tmp/tokenizer-pr-review`:

- `rebased-final-static-checks.json`
- `debug-156482/rebased-full.log` and `rebased-perf-sequential.log`
- `debug-156484/rebased-full.log` and `rebased-perf-sequential.log`
- `build-accessor/unified-cursor-validation-final.json`
- `asan-candidate/review-cursor-tests.log`
- `build-accessor/unified-cursor-comparisons-final.json`
- `fresh-review-final-performance.json`

## Earlier full-migration validation (superseded tips)

This section records superseded tips bf25f7b0c498, a6102b57e550, and
f72cb2b2757f. All three source worktrees were clean. Linux
x86-64 builds use GCC 16.2.1 and separate directories for each PR.

| PR | Full debug validation | Other validation |
|---|---|---|
| #156482 | All-resource run: 52,462 tests; curses passed with a corrected TERM | Source discard/offset/metadata boundaries; patchcheck |
| #156484 | Fresh clean-banner default run: 51,926 tests, no failures | All-resource run: 52,460 tests; patchcheck |
| #156654 | Default run: 51,930 tests, no failures | PEG: 98 tests; reference-leak checks: 270 tests; debug ASan/UBSan: 495 tests; patchcheck |

The first two all-resource runs encountered `curses.error: newterm() returned
NULL` with TERM=dumb. The same failure occurred on the unchanged baseline.
Both final tips passed all 183 curses tests with TERM=xterm-256color (three
skips). P2's first build banner also reported `-dirty` despite its clean
worktree; its index and build metadata were refreshed, then the fresh full
suite passed with the clean a6102b57e55 banner. P3's full run required no retry.
Default full suites executed 478 passing test modules, with 15 platform skips
and 11 resource-denied modules.

Final P3 comparisons against the previous PR tip afd9d72be50 match exactly:

- 156 standard-library/test files: complete token streams and AST locations.
- 557 syntax doctest compilation outcomes, including exception attributes.
- 243 incomplete-input outcomes, including exception type, text, and positions.

The expanded incomplete-input comparison caught 28 text differences while
removing string rewinds. The explicit diagnostic span fixes them; regression
coverage asserts the exact multiline exception arguments for ordinary, f-,
and t-strings, with non-ASCII source and both single/exec input.

ASan/UBSan used --with-pydebug, --without-pymalloc, and
ASAN_OPTIONS=detect_leaks=0. Separate -R 3:3 runs checked tokenizer, f/t strings,
source C tests, and codeop reference counts. The forced-relocation debug path
was exercised. No native Windows or macOS runtime test was performed locally;
core/freezer project XML, source entries, and standalone PEG build integration
were checked.

The new opaque API initially exposed a short-line throughput regression.
The final implementation keeps the small character-read loop inline, puts
input offsets and source storage together, and uses one borrowed token-view
operation in _tokenize. Nine alternating release samples pinned to CPU 31,
using the exact final P3 binary and previous PR tip, gave these medians:

| Workload | Previous tip | Final tip | Change |
|---|---:|---:|---:|
| Compile four standard-library modules | 29.753 ms | 30.068 ms | +1.06% |
| Compile 1,000 formatted-string assignments | 6.637 ms | 6.322 ms | -4.75% |
| Tokenize 10,000 short assignments | 22.207 ms | 22.502 ms | +1.33% |

These local samples do not establish a general speedup or no-regression
statistical guarantee. Earlier samples of the same source gave -0.27%, -2.15%,
and +0.75%, respectively. Three runs draining 100 MiB of 4 KiB source lines
peaked at about 17 MiB RSS in both builds. Active multiline constructs can
require larger retained windows.

Three independent agents reviewed reuse, code quality, efficiency, ownership,
and API contracts. Their findings drove the 32-bit bit-count fix, build-entry
corrections, explicit incomplete-input context, token-view operation, and
scanner inlining. The public header exposes neither SourceText nor lexer
frame layout. All incremental/full PR diffs pass whitespace checks.

Exact commands, versions, hashes, and logs are under /tmp/tokenizer-pr-review:

- build-accessor/unified-comparisons-final.json
- build-accessor/unified-full-final.log
- debug-156482/unified-full.log
- debug-156484/unified-full-clean-banner.log
- asan-candidate/unified-final-tests.log
- unified-final-performance.json
- final-static-checks.json

Before this update, the old P2 Windows free-threading job failed in untouched
asyncio TaskGroup cancellation-message handling from #155439. Tokenizer tests
passed in that job. This was not independently reproduced on Windows and is
not classified as a proven baseline flake. Updated heads require fresh CI.

## Earlier September 5 cleanup validation (superseded tips)

The earlier tested tips were b5addc5d049, 056e86ee18b, and afd9d72be50. Local
builds used Linux x86-64, GCC 16.2.1, and separate build directories for each
PR. The source worktrees are clean. All three final full-suite runs passed.

| PR | Full debug suite | Focused validation |
|---|---|---|
| #156482 | Passed; 51,926 tests run | PEG generator: 98 tests; tokenizer/f-/t-string/C source reference-leak checks passed |
| #156484 | Full retry passed; 51,924 tests run | All 14 profiler tests passed in isolation; PEG generator: 98 tests; reference-leak checks passed |
| #156654 | Passed at final `afd9d72be50`; 51,927 tests run | PEG generator: 98 tests; reference-leak checks passed; ASan/UBSan debug run: 480 tests passed |

The complete suites used default resource settings; each ran 493 of 504 test
modules, with platform/resource skips. The PEG generator was run separately
with its CPU resource enabled. No native Windows or macOS execution was done
locally. Windows project XML parsed successfully for every tip.

The first #156484 run failed
`test_perf_profiler.test_pre_fork_compile` because its child returned empty
stdout. The cause is unconfirmed; the isolated rerun passed all 14 profiler
tests, and the fresh full-suite retry passed. The initial build banner also included `-dirty`; the worktree was
verified clean and build-version metadata was refreshed before the full retry.
Do not silently classify the first run as a pass.

Commands, from each separate debug build directory:

```sh
../pr-N/configure --with-pydebug --without-ensurepip
make -j8
./python -m test -j8 --timeout=180
./python -m test -u cpu test_peg_generator
./python -m test -R 3:3 test_tokenize test_fstring test_tstring test_capi.test_tokenizer
make patchcheck
```

`N` is the PR number; the #156654 build directory is named `build-accessor`.
`make patchcheck` completed, and its checks were also run against each actual
PR predecessor to avoid comparing unrelated upstream changes. Configure
regeneration was not needed. Incremental diffs pass `git diff --check`.

The final #156654 sanitizer build uses:

```sh
../pr-156654/configure --with-pydebug --without-ensurepip --with-address-sanitizer --with-undefined-behavior-sanitizer --without-pymalloc
make -j12
ASAN_OPTIONS=detect_leaks=0 ./python -m test -j4 test_tokenize test_fstring test_tstring test_syntax test_source_encoding test_repl test_capi.test_tokenizer
```

Leak detection in ASan was disabled; the separate debug reference-leak runs
cover reference-count stability. The debug sanitizer configuration exercises
the forced-relocation branch. A release sanitizer run also passed the same
seven modules before the final assertion/documentation refinement.

Release comparisons use the original #156654 head `9ce1fd9d9c5a` as baseline,
matching configure/compiler settings, and alternating samples pinned to one
CPU. Complete token streams and ASTs including locations matched for 156
standard-library/test files. All 557 syntax doctest compilation results
matched, including 533 exception type/message/text/location tuples.

Nine-sample medians before the final debug-only assertion/header refinement:

| Workload | Original | Updated | Change |
|---|---:|---:|---:|
| Compile four standard-library modules | 26.732 ms | 26.886 ms | +0.57% |
| Compile 1,000 formatted-string assignments | 6.165 ms | 6.106 ms | -0.95% |
| Tokenize 10,000 short assignments, draining tokens | 21.794 ms | 21.527 ms | -1.22% |

The rebuilt final release head was also compared on 10,000 continued lines
inside an interactive formatted string: 43.307 ms original versus 43.041 ms
updated (-0.61%). These are local samples showing small timing differences,
not evidence of a general speedup or a statistical no-regression guarantee.

Draining a 100 MiB byte-token stream with 4 KiB lines peaked at roughly
17.3 MiB RSS in both builds (three runs each). This measures bounded streaming
input; it is not a worst-case bound for an arbitrarily long active construct.
Debug relocation deliberately adds copying and temporary peak allocation.

Three independent final review passes checked reuse, code quality, and
efficiency. An additional API-contract pass checked naming, borrowed lifetime,
byte-length semantics, clamping, exception preservation, and consistency with
nearby tokenizer views. The accepted refinement was a required-output-pointer
assertion and explicit header contract; no generic view wrapper was added.

Local commands/scripts/results are under `/tmp/tokenizer-pr-review`, including
`compare_tokenizer.py`, `compare_diagnostics.py`, `release-comparison.json`,
`interactive-comparison.json`, and the per-build `tests-*.log` files. These
artifacts describe this validation run and are not additions to the CPython
implementation PRs.

## Validation required for each PR

At minimum:

1. Build a CPython debug configuration from clean state.
2. Run focused tokenizer, syntax, encoding, f-string/T-string, codeop, C API,
   pegen, and `_tokenize` tests relevant to the slice.
3. Run the complete test suite on the exact commit proposed for review.
4. Run `make patchcheck` and the relevant regeneration checks.
5. Check Unix and Windows build-file changes whenever sources are added,
   removed, or renamed.
6. Run differential token-stream and error-location testing for any behavior
   migration.
7. Run sanitizer testing when source ownership, decoder lifetime, views, or
   spans change.
8. Record exact commands and results in the development handoff.

No PR should rely solely on the validation of the combined prototype.

## Review and branch workflow

For the active stack:

1. Make a correction in the earliest PR introducing its relevant code.
2. Propagate that correction through dependent branches, adapting it to their
   final representation rather than blindly applying conflicting hunks.
3. Inspect each PR's incremental diff against its predecessor as well as the
   complete diff against its GitHub base. Keep unrelated edits out.
4. Build and test each resulting tip independently, including the complete
   debug suite and the focused tests protecting that PR's changes.
5. Update the existing PR branches and descriptions after validation. Record
   failures and limitations accurately. No merge is part of this task.
6. When a prerequisite merges, rebase its dependents onto the merged upstream
   state and verify that the remaining diffs contain only their intended work.

For later implementation PRs, use the historical integration commits only as
reference. Recreate the smallest coherent change from its prerequisite and
include only the coverage it needs. The combined conversion commit and the
historical coverage branch remain unsuitable review units.

## Known traps

- A green integration stack is not automatically reviewable.
- The coverage branch is source material, not PR zero.
- `offset-conversion` contains several future PRs in one commit.
- `readline-chunks` and `validation-tools` include their entire dependency
  stack despite their narrow names.
- The squashed redesign branch is a recovery snapshot, not a review branch.
- The unused-token-text commit overlaps separately tracked gh-153568 work.
- Source views are transient; spans and offsets are the durable values.
- Internal offsets are decoded-byte offsets, while Python-facing columns are
  character offsets in several consumers.
- Newline synthesis differs among string, file, and `readline()` modes.
- A `readline()` callback may return more than one logical line in one chunk.
- Peeking across a line can change visible REPL behavior.
- Error positions that were formerly produced by pointer rewinds must become
  explicit locations without changing their observable values.
- Multi-line `TokenInfo.line` values require a line-range view, not merely a
  view of the token's starting line.
- Plain-file, string, and interactive pegen metadata behavior are not
  interchangeable.
- Deleting compatibility code belongs after consumer migration, not in the
  source/storage or state PRs.

## Conversation and handoff provenance

The implementation and plan were recovered from these local Codex sessions:

- Session `019f4eaa-e404-7fb2-8f49-634511ebc5f0`, started 2026-07-11. Title:
  `cjeck TOKENIZER_REDESIGN.md and start working on this in a subtree`.
  Local log:
  `/home/pablogsal/.codex/sessions/2026/07/11/rollout-2026-07-11T01-54-10-019f4eaa-e404-7fb2-8f49-634511ebc5f0.jsonl`.
  This session created the original integration stack, negotiated the PR
  boundaries, opened #153585 and #153587, and corrected the oversized second
  PR.
- Session `019f7539-5fa4-7c11-b4c9-d1a7ff96f794`, started 2026-07-18. Title:
  `find out the worktree where we are doing the tokenizer stuff and look at
  the review of https://github.com/python/cpython/pull/153587 and tell me what
  we should do`. Local log:
  `/home/pablogsal/.codex/sessions/2026/07/18/rollout-2026-07-18T13-35-22-019f7539-5fa4-7c11-b4c9-d1a7ff96f794.jsonl`.
  This session recovered the plan, reviewed the source/cursor API against
  downstream consumers, and restated the remaining six-PR sequence.

The original local handoff was written outside the repository at:

`/home/pablogsal/github/python/TOKENIZER_OFFSET_REDESIGN_HANDOFF.md`

Several other July 11 and July 18 session IDs are forked review-agent sessions
from these conversations. They are useful for detailed API-review evidence
but are not separate planning conversations.

## Local recovery worktrees

At the August 27 handoff, useful local recovery locations included:

- `/home/pablogsal/github/python/worktrees/tokenizer-offsets`: rebased full
  integration stack, branch `tokenizer-offsets`, tip `98fb37b1a8f`.
- `/home/pablogsal/github/python/worktrees/tokenizer-redesign`: squashed
  integration snapshot, branch `tokenizer-redesign`, tip `f08d53933b1`.
- `/home/pablogsal/github/python/worktrees/tokenizer-offsets-baseline`:
  detached historical lexer-split boundary at `7f0ccc42889`.
- `/home/pablogsal/github/python/worktrees/tokenizer-offsets-boundary`:
  detached historical incremental-decoder boundary at `ff5f002bfa2`.
- `/home/pablogsal/github/python/worktrees/tokenizer-lexer-split` and
  `/home/pablogsal/github/python/worktrees/tokenizer-offset-api`: historical
  review worktrees now synchronized to upstream after their PRs merged.

Local worktrees are recovery aids. The fork branches are the durable remote
copies.

## Immediate next action

Review the eight drafts in the order shown above and check fresh CI for their
recorded heads. Land python/cpython#156484 first. After each predecessor lands,
rebase the next fork branch and open its CPython landing PR as a draft.
The fork drafts keep each dependent review within the agreed size limits.
No PR was merged as part of this split.

All three requested migrations remain implemented. Validation tooling remains
a separate follow-up.
