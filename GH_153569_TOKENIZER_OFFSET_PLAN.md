# gh-153569 tokenizer offset migration: status, plan, and handoff

Last checked: 2026-08-27

Tracking issue: https://github.com/python/cpython/issues/153569

This document records the current state of the tokenizer offset migration,
the plan recovered from the original development conversations, the source
branches and commits, the review boundaries that were agreed after the first
attempt was too large, and the validation needed before each remaining change
is submitted.

## Executive status

The issue is open. Its first two foundation PRs are merged into CPython's
`main` branch:

| PR | Scope | State | Merge commit | Final GitHub diff |
|---|---|---|---|---|
| [#153585](https://github.com/python/cpython/pull/153585) | Split the tokenizer lexer into focused files | Merged 2026-07-11 | `a2d3787105d1` | 12 files, +1,224/-957 |
| [#153587](https://github.com/python/cpython/pull/153587) | Add decoded source, spans, locations, and offset-only cursor primitives | Merged 2026-08-26 | `f54fd2ab6e1f` | 14 files, +923/-2 |

The next implementation PR should be the reader/decoder slice. No clean
reader/decoder review branch exists yet. The implementation exists only as
part of a much larger integration commit and must be carved out rather than
submitted as-is.

All preserved fork branches listed below were rebased onto the current
`upstream/main` tip and pushed to `pablogsal/cpython` before this document was
written. They are source material and recovery points. Most are not valid PR
boundaries.

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

The intended architecture replaces those root causes:

1. One decoded source object owns the source bytes.
2. Positions are offsets into that source, never persistent interior pointers.
3. Tokens and errors describe half-open source spans.
4. A cursor stores its offset and cached line bounds.
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
- Use one independently testable commit per intended PR.
- Every review branch must build and pass its relevant tests at its exact tip.
- Behavior coverage belongs in the PR that needs it; there is no standalone
  coverage PR.
- Keep each PR conceptually focused even if that means temporarily preserving
  compatibility entry points.
- Avoid changes whose rename/deletion churn makes the implementation hard to
  review or revert.
- Keep the source integration stack untouched as reference and carve each
  review branch from the latest merged upstream state.

The original attempt at the second PR accidentally combined `SourceText`, the
cursor, the reader/decoder, errors, lexer state, pegen, `_tokenize`, and legacy
deletion. It produced a 50-file change with roughly 5,266 additions and 3,241
deletions. That was rejected as unreviewable. PR #153587 was force-rewritten
to contain only the source/cursor foundation. This failure is why the branch
boundaries in this document are strict.

## What the merged foundation provides

### Lexer split: PR #153585

Number scanning and string/f-string scanning were moved out of the lexer
mega-file into focused files. Relevant behavior coverage was included in the
same PR. This was intended to be a behavior-neutral structural change that
makes the later offset conversion easier to review.

### Source and cursor primitives: PR #153587

The merged source foundation provides:

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

## Current fork branch inventory

The counts below are relative to `upstream/main` as checked on 2026-08-27.

| Fork branch | Ahead | Current purpose | Diff or top-commit size | Submit directly? |
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
standalone diffs against upstream. A future PR must be recreated from the
appropriate merged prerequisite and contain only its own logical change.

## Rebased source commits

The current integration stack is:

| Commit | Change | Individual size | Intended treatment |
|---|---|---|---|
| `2d43b0f5158` | Expand tokenizer behavior coverage | 5 files, +188/-3 | Distribute tests into the PRs they protect |
| `7fd9ad7e10e` | Base the tokenizer API on source offsets | 54 files, +4,847/-3,929 | Reference only; split into reader/decoder, offset state, and API cutover |
| `b62ebe81b96` | Avoid materializing unused parser token text | 1 file, +21/-8 | Do not include in gh-153569 work; overlaps gh-153568/#153576 |
| `0af9c4d9afe` | Preserve readline chunks across incremental decoding | 2 files, +89 | Reference for a later dedicated PR |
| `98fb37b1a8f` | Add tokenizer validation tools | 5 files, +486 | Reference for the final validation PR |

The squashed recovery snapshot is `f08d53933b1`, titled `Tokenizer redesign
integration snapshot`.

Before the rebase, the same source material had these hashes:

| Original commit | Current equivalent |
|---|---|
| `c044cdf75ad` | `2d43b0f5158` |
| `485acfcba70` | `7fd9ad7e10e` |
| `dcfc2213fa6` | `b62ebe81b96` |
| `ff5f002bfa2` | `0af9c4d9afe` |
| `f13f458e9ca` | `98fb37b1a8f` |

Never cherry-pick either `485acfcba70` or `7fd9ad7e10e` into a review
branch. Both are integration checkpoints, not review units.

## Remaining implementation plan

### 1. Common reader and decoder

Suggested branch: `gh-153569-tokenizer-reader-decoder`

This is the immediate next PR.

Scope:

- Add a common reader around the merged source store.
- Add the decoder stage needed to handle BOMs, PEP 263 cookies, codec lookup,
  decoding, newline normalization, and decoded-line insertion.
- Support the existing source kinds: prepared strings, UTF-8 strings, files,
  `readline()` callables, and interactive input.
- Preserve existing tokenizer-facing constructors and entry points during this
  PR so consumers do not move yet.
- Preserve prompt timing and `readline()` callback timing. Lexer lookahead
  must not unexpectedly pull a later line.
- Preserve source-kind-specific implicit-newline behavior.
- Fold in reader, encoding, cookie, BOM, newline, file, string, and basic
  incremental-input tests needed to protect this slice.

Explicitly out of scope:

- Migrating lexer token markers to spans.
- Migrating f-string state or error state to spans.
- Changing pegen or `_tokenize`.
- Deleting compatibility constructors or old tokenizer implementations.
- The complete-chunk preservation fix from `0af9c4d9afe`; keep that as a
  focused follow-up unless carving the reader proves it is inseparable.
- Validation/fuzz/benchmark tooling.

Implementation method:

- Start from current `upstream/main`, which already contains both merged PRs.
- Create a fresh worktree and one review commit.
- Use `7fd9ad7e10e` only to understand the intended implementation.
- Transplant the smallest coherent set of reader/decoder changes manually.
- Keep the old public-facing behavior and compatibility boundary intact.
- Add only the subset of `2d43b0f5158` tests that protects this PR.

The historical size estimate was 16-22 files, approximately 1,400-1,700
additions and 800-1,200 deletions. That estimate predates the final merge of
#153587 and should not be treated as a target. Conceptual isolation matters
more than matching those numbers.

### 2. Offset-based lexer, f-string, and error state

Suggested branch: `gh-153569-tokenizer-offset-state`

Scope:

- Replace lexer token start/end pointers with `_PyTok_Span` values.
- Replace f-string pointer state and copied debug-expression buffers with
  offsets/spans into retained source.
- Replace pointer-derived error positions with explicit offset/span locations.
- Keep pegen and `_tokenize` on a compatibility surface for this PR.
- Preserve token streams, error messages, error positions, tolerant mode, and
  incomplete-input classification.
- Fold the relevant f-string, syntax-location, tolerant-mode, and token-stream
  coverage into the PR.

This is expected to be the hardest review because lexer and f-string
invariants are subtle. The historical estimate was 12-17 files,
approximately 1,500-2,000 additions and 1,300-1,700 deletions.

Important invariants:

- Source offsets remain stable when storage relocates.
- No raw source pointer survives a source append.
- Spans are half-open.
- Indentation widths remain computed state; they are not source positions.
- Interactive prompt and callback timing remain unchanged.
- F-string/T-string token splitting and column behavior remain exact.
- Error type, message, and location remain exact.
- Incomplete input remains distinguishable from a hard syntax error.

### 3. Opaque API cutover and legacy removal

Suggested branch: `gh-153569-tokenizer-api-cutover`

Scope:

- Move pegen to the opaque tokenizer API.
- Move `_tokenize.TokenizerIter` to source views, spans, and accessors.
- Replace tokenizer field reads and writes with explicit operations.
- Preserve `_tokenize.TokenizerIter`'s clinic signature and `extra_tokens`
  adjustments.
- Preserve pegen error-line and metadata behavior for strings, files, and
  interactive input.
- Keep the standalone encoding-finder entry point working behind the same
  symbol.
- Delete the compatibility surface and obsolete reader implementations only
  after all consumers have moved.
- End with one tokenizer implementation, not `tokenizer2` plus a legacy copy.

The historical estimate was 15-22 files, approximately 1,200-1,600
additions and 900-1,300 deletions. File count may remain broad because the
consumer adaptation is mechanical, but semantic changes should stay out.

### 4. Unused pegen token text

This was part of the original gh-153569 sequence because stable offsets make
token text materialization optional. It is no longer cleanly part of this
issue's remaining branch stack.

PR [#153576](https://github.com/python/cpython/pull/153576), titled
`gh-153568: Don't materialize parser token text that is never read`, is open
under gh-153568. It implements the same optimization independently. The
rebased `b62ebe81b96` commit should therefore not be submitted from a
gh-153569 branch.

If this optimization is revisited, materialization must remain the default
and omission must use an audited proven-dead token list. Several token types
that look structural still have their bytes read by parser actions, including
f/t-string starts, `!=` under Barry-as-FLUFL handling, and some retyped
keyword tokens in error paths.

### 5. Incremental `readline()` chunk preservation

Suggested branch: `gh-153569-tokenizer-incremental-decoder`

Reference commit: `0af9c4d9afe`

Scope:

- Preserve all complete decoded lines returned by one `readline()` call.
- Do not discard the remainder when one callback result contains multiple
  logical lines.
- Preserve chunk-scoped implicit-newline behavior and callback timing.
- Add focused multi-line-chunk and incremental-decoder tests.

The reference change is 2 files and +89 lines. It depends on the common
reader/decoder and should be rebuilt on the merged reader implementation,
not submitted from the stacked fork branch.

### 6. Differential testing, fuzzing, benchmarks, and memory validation

Suggested branch: `gh-153569-tokenizer-validation`

Reference commit: `98fb37b1a8f`

Scope:

- Add a differential tokenizer runner covering the standard library, tests,
  encoding/newline/f-string corpora, and random mutations.
- Compare complete token streams, not only token types.
- For failures, compare exception type, message, and location.
- Add tokenizer fuzz coverage.
- Add repeatable throughput benchmarks.
- Measure peak memory on pathological large input, including a 100 MiB
  `tokenize(readline)` workload.

The reference change adds 486 lines across five files. It should land after
the implementation behavior is stable. The tools can be used locally before
then without making them part of earlier review diffs.

## Current adjusted count of remaining PRs

The July 18 handoff counted six PRs after #153587:

1. Reader/decoder.
2. Offset lexer/f-string/error state.
3. Opaque API cutover.
4. Pegen token spans.
5. Incremental decoder fix.
6. Validation tools.

Today the pegen token-text optimization is already represented by the
separate gh-153568 PR #153576. Under the current issue split, gh-153569 has
five likely implementation PRs remaining: reader/decoder, offset state, API
cutover, incremental decoder, and validation.

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

The prototype and original design were organized around these constraints:

1. Appended source bytes keep the same logical offset for the tokenizer's
   lifetime.
2. Source storage is the sole owner of persistent interior pointers.
3. Consumers retain offsets or spans, not source pointers.
4. Returned source views are bounded and transient.
5. The cursor may cache current-line bounds, but that cache must be refreshed
   at the only operations that can relocate source storage.
6. Lexer lookahead must not pull a later logical line because doing so changes
   interactive prompts and callback timing.
7. Backward movement through already loaded source must not rewind or invoke
   the reader again.
8. Allocation failure must not partially commit source bytes or metadata.
9. Tokenizer errors should converge on one explicit, sticky error record and
   one raising boundary instead of parallel done-code, return-token, `PyErr`,
   and decoder flags.
10. The final lexer state should use real value-semantic frames for f-string
    body, replacement expression, and format-spec contexts.

Some of these are final-state goals rather than properties of the two merged
foundation PRs. They must be introduced incrementally without pretending the
whole architecture already exists.

## Source retention and memory

The integration prototype retains decoded source in one contiguous,
append-only buffer. This makes offsets simple and allows error lines,
interactive source, f-string debug text, and consumer views to refer to the
same store. It can increase memory for streaming `tokenize(readline)` use:
the old implementation often retained only the longest active construct,
while the first offset implementation retains the whole decoded input.

The original plan deliberately accepts full retention for the first cutover
and requires measurement rather than speculation. The prototype's 100 MiB
pathological-input run peaked at about 119.1 MiB rather than the previously
feared 360 MiB. That number was measured on the pre-rebase prototype and must
be reproduced on final code before it is treated as current evidence.

If retention becomes a real problem, the proposed follow-up is a low-watermark
scheme that releases physical pages below the earliest live span while
preserving logical offsets. It is not part of the current PR sequence.

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

## Validation required for each future PR

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

For every remaining PR:

1. Wait until its prerequisite PR is merged.
2. Fetch `upstream/main`.
3. Create a fresh branch and worktree from that exact upstream tip.
4. Use the integration branches only as a readable implementation reference.
5. Recreate the smallest coherent change rather than cherry-picking the
   combined conversion commit.
6. Fold in only the coverage needed by that PR.
7. Keep one review commit unless a reviewer requests otherwise.
8. Build and test the exact tip.
9. Review the actual `upstream/main...HEAD` diff for accidental dependency
   leakage.
10. Push to the fork only when the branch contains the intended slice.

Before publishing, explicitly verify that the diff does not accidentally
contain later work such as pegen adaptation, `_tokenize` conversion, legacy
deletion, the token-text optimization, incremental chunk handling, or
validation tools.

## Known traps

- A green integration stack is not automatically reviewable.
- The coverage branch is source material, not PR zero.
- `offset-conversion` contains several future PRs in one commit.
- `readline-chunks` and `validation-tools` include their entire dependency
  stack despite their narrow names.
- The squashed redesign branch is a recovery snapshot, not a review branch.
- The unused-token-text commit overlaps a different issue and open PR.
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

At the time of this handoff, useful local recovery locations included:

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

Create `gh-153569-tokenizer-reader-decoder` from current `upstream/main` in a
fresh worktree. Read `7fd9ad7e10e` as the implementation prototype, extract
only the common reader/decoder layer and the tests that protect it, preserve
all existing tokenizer-facing entry points, and verify that the diff contains
no lexer-state conversion, pegen or `_tokenize` migration, legacy deletion,
unused-token-text optimization, incremental-chunk follow-up, or validation
tooling.

That PR is the critical path. The offset-state and API-cutover work depends on
it; the incremental-decoder fix depends on its reader implementation; and the
final validation tooling is meaningful only once the migration behavior is
stable.
