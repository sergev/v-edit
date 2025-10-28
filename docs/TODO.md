# Eliminate Segment Pointers - Implementation Plan

## Project Context
The codebase currently uses `std::list<Segment>` internally but exposes many methods returning/using `Segment*` for backward compatibility. The goal is to eliminate all raw pointers completely.

## Phase 1: Core Infrastructure Changes
- **Remove pointer fields** from Segment class (`next`, `prev`)
- **Change static methods** to return `std::list<Segment>` instead of `Segment*` chains:
  - `copy_segment_chain()`
  - `create_blank_lines()`
  - `cleanup_segments()` → Remove (use RAII)
- **Update Workspace internal implementations** to only use list iterators

## Phase 2: API Migration
- **Remove/deprecate pointer-returning methods:**
  - Remove `chain()` accessor
  - Change `cursegm()` to return `Segment::iterator`
  - Remove `set_cursegm(Segment*)`
- **Update method signatures:**
  - `insert_segments(std::list<Segment>&, int)`
  - `delete_segments(int, int)` → Remove return value
  - `set_chain()` → Simplify/remove
- **Update all usages** in source files (.cpp) to use iterators/lists

## Phase 3: Test Migration
- **Update test implementations** to use new APIs
- **Remove pointer-based verification** code
- **Add iterator/list-based assertions** for correctness

## Key Benefits
- Eliminates manual memory management
- Modern C++ with RAII
- Safer iterator-based traversal
- Reduced memory leaks/corruption risk
- Simplified API surface

## Implementation Notes
- Many operations already use list iterators internally
- External APIs need wrapper/compatibility layer during transition
- Tests will need significant refactoring but simpler verification logic
