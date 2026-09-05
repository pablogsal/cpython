# gh-153569 tokenizer offset migration: status, plan, and handoff

Last checked: 2026-09-05

Tracking issue: https://github.com/python/cpython/issues/153569

This document records the current state of the tokenizer offset migration,
the plan recovered from the original development conversations, the source
branches and commits, the review boundaries that were agreed after the first
attempt was too large, and the validation needed before each remaining change
is submitted.

## Executive status

The issue remains open. Three foundation PRs are merged:

| PR | Scope | State |
|---|---|---|
| [#153585](https://github.com/python/cpython/pull/153585) | Split the tokenizer lexer into focused files | Merged; merge commit `a2d3787105d1` |
| [#153587](https://github.com/python/cpython/pull/153587) | Add decoded source, spans, locations, and offset-only cursor primitives | Merged; merge commit `f54fd2ab6e1f` |
| [#156472](https://github.com/python/cpython/pull/156472) | Common tokenizer reader and decoder | Merged |

The active review stack is #156482 → #156484 → #156654. The September 5
updates below have passed the validation recorded in this document.

| PR | Scope | Validated tip |
|---|---|---|
| [#156482](https://github.com/python/cpython/pull/156482) | Return tokenizer tokens as source spans; protect relocation | `b5addc5d0490d07e86119b21596120008e75305a` |
| [#156484](https://github.com/python/cpython/pull/156484) | Remove unused cursor, line index, and span-view API | `056e86ee18b5060016601895f7f24804861953da` |
| [#156654](https://github.com/python/cpython/pull/156654) | Simplify source and formatted-string state; source-owned diagnostic line lookup | `afd9d72be5073db8db08e75471e43f7a1ce94945` |

The larger remaining offset/storage/error-state migration, opaque consumer
API cutover, and validation tooling stay separate follow-ups. This update
changes the existing three PRs and this plan; it does not start those later
implementation PRs.

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
   streaming windows; this ownership unification remains follow-up work.
2. Positions are offsets into that source, never persistent interior pointers.
3. Tokens and errors describe half-open source spans.
4. Persistent scanner positions use offsets; temporary pointer caches stay
   within the scanning/storage boundary. The unused cursor prototype is removed
   in #156484 rather than retained as a second state representation.
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
never adopted by the production lexer and are removed by #156484:

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

## Current PR scopes and September 5 updates

### #156482: token spans and relocation

Keep token span conversion and the behavior coverage that protects it here.
The reader no longer predicts whether source capacity will grow. It saves and
restores persistent buffer positions around every source append that preserves
the active window, leaving allocation policy with the source implementation.

Debug streaming-buffer growth now allocates a replacement while the old
buffer is live, copies the active bytes and terminator, poisons and frees the
old allocation, and restores positions. This makes relocation deterministic
in debug builds. Release builds retain the existing realloc path.

The streaming relocation test now covers both f- and t-strings with
`extra_tokens` enabled and disabled. A multiline interactive f-string test
covers retained-source relocation in the REPL. These tests belong here and
propagate through both dependent PRs.

### #156484: remove unused primitives

Remove the unused cursor and sparse line index. Also remove
`_PyTok_SourceSpanView()`, which has no production consumers. Keep the useful
span types and helpers. Direct source tests continue to verify line append
rules, offsets, implicit-newline flags, complete stored bytes, UTF-8 bytes,
and the terminating NUL.

Remove the unrelated junction-handling edit in `Lib/test/support/os_helper.py`
from this PR's diff. No replacement filesystem cleanup change belongs in this
stack.

### #156654: source and formatted-string state

Keep the smaller formatted-string representation: one inline slot for the
common case and a dynamically growing stack for nested strings. Each active
f- or t-string has one frame containing its kind, quote, opening location,
mode, replacement depth, expression span, and recorded comment spans. Bracket
nesting comes from the existing delimiter stack.

The reader owns buffer relocation and interactive state. The PR removes the
old buffer layer, dummy regular mode, duplicated state and source aliases;
its existing diagnostics use retained tokenizer source. Prefix/quote scanning
and prepared-input normalization remain outside this cleanup's scope.

Move the diagnostic line scan from `pegen_errors.c` into
`_PyTok_SourceLineView()`. Keep parser-relative line-number adjustment and
Unicode decoding in pegen. The accessor returns a transient byte view without
the newline, clamps line numbers to the first/final line, and treats a trailing
newline as an empty final line. Direct tests protect empty input, UTF-8 text,
unterminated tails, and extreme line numbers. A linear scan is appropriate on
this error path; do not restore the deleted sparse index for it.

## Remaining implementation plan

### 1. Remaining offsets, storage ownership, and error state

- Convert remaining persistent scanner positions to offsets and finish
  unifying decoded-storage ownership. Streaming currently owns `tok->buf`
  separately from retained prepared/interactive `SourceText`.
- Preserve bounded retention for streaming sources. Use a logical base offset
  for discarded windows rather than retaining all prior lines to simplify
  offset arithmetic.
- Replace string-diagnostic cursor rewinds and temporary BOM diagnostic state
  changes with explicit diagnostic locations and text views.
- Converge error reporting on explicit state without changing exception types,
  messages, locations, or incomplete-input classification.
- Keep the existing consumer compatibility surface until its dedicated
  migration. Include regression tests with each implementation change.

These are substantial migration steps, not additional cleanup requirements
for the current three PRs. Review boundaries may be split further if needed.

### 2. Opaque API consumer cutover

- Move pegen and `_tokenize.TokenizerIter` away from `tok_state` field access
  to explicit operations, spans, and source views.
- Stop obtaining an already-produced token's raw-string context from the live
  f-string frame. Carry required context through tokens or grammar actions.
- Define token-text and multiline line-view lifetimes, including
  `TokenInfo.line` values that span several physical lines.
- Preserve the clinic signature, `extra_tokens` adjustments, encoding-finder
  entry point, source-kind-specific metadata, and interactive/callback timing.
- Delete compatibility entry points once their consumers have moved. End with
  one tokenizer implementation.

### 3. Validation tooling

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
merged reader/decoder, the current three-PR stack, and the three remaining
work areas above. Do not infer a fixed final PR count from the old estimates.

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

Preserve these constraints during the remaining migration:

1. Retained source bytes keep their logical offsets when storage relocates or
   an earlier streaming window is discarded. Offsets do not promise that
   discarded source remains accessible.
2. Source storage is the sole owner of persistent interior pointers.
3. Consumers retain offsets or spans, not source pointers.
4. Returned source views are bounded and transient.
5. Temporary scanning pointers must be refreshed at every operation that can
   relocate their source storage; callers must not predict allocator growth.
6. Lexer lookahead must not pull a later logical line because doing so changes
   interactive prompts and callback timing.
7. Backward movement through already loaded source must not rewind or invoke
   the reader again.
8. Allocation failure must not partially commit source bytes or metadata.
9. Tokenizer errors should converge on one explicit, sticky error record and
   one raising boundary instead of parallel done-code, return-token, `PyErr`,
   and decoder flags.
10. Keep one frame per active formatted string with a mode and replacement
    depth, one inline slot, and a growable nesting stack. This intentionally
    differs from the original proposal of separate frames for each body,
    replacement-expression, and format-spec context. Changing to that older
    proposal is not a requirement for this stack.

Some of these are final-state goals rather than properties of the current
stack; persistent scanner pointers and consumer field access still remain.
Introduce the remaining invariants incrementally without claiming that the
whole architecture already exists.

## Source retention and memory

The current decision preserves bounded streaming storage for file and
`readline()` input. Consumed windows can be released/reset while logical
`buf_offset` advances. A long active token or formatted string can still
require a large retained window; bounded streaming does not mean a fixed
constant-memory limit independent of the active construct.

Prepared and interactive input retain decoded `SourceText`, supporting their
source views and diagnostics. The remaining ownership unification must
preserve these source-kind retention semantics rather than forcing streaming
input into full-input retention.

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

## September 5 validation status

The tested source tips are the hashes in the active-stack table above. Local
builds use Linux x86-64, GCC 16.2.1, and separate build directories for each
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

Finish validation of #156482, #156484, and #156654 at the candidate tips listed
above. Resolve any change-related failures, record environmental limitations,
and update the existing PR branches and descriptions with the validated
results. Keep their dependency order and the scope boundaries in this plan.

The reader/decoder is already merged. Once this stack is ready, the next
implementation work is the remaining offset/storage/error-state migration,
followed by the opaque consumer API cutover and validation tooling. Those
follow-ups are recorded here for continuity and are not added to this update.
