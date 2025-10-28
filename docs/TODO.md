# TODO: Convert Segment Management from Pointers to std::list and Iterators

## Phase 1: Add Infrastructure
- [x] Add `std::list<Segment>` as a member in Workspace class
- [x] Include `<list>` header appropriately
- [x] Keep all existing code intact
- [x] Verify compilation and all tests pass

## Phase 2: Synchronize Representations
- [x] Modify Workspace constructor to use `std::list` for initial tail segment
- [x] Modify cleanup_segments to use `segments_.clear()`
- [x] Populate `segments_` list in `build_segments_from_file` method
- [x] Build verified - tests pass

## Phase 3: Change Accessors
- [x] Modify `chain()` to return from list front
- [x] Modify `cursegm()` to return based on iterator position
- [x] Maintain backward compatibility with pointer API
- [x] Run tests to ensure external callers see no difference

## Phase 4: Convert Core Operations (One-by-One) ✅ COMPLETED
- [x] `set_current_segment()` - replace pointer navigation with iterator navigation
- [x] `breaksegm()` split segments - use `segments_.insert()` instead of manual linking
- [x] `catsegm()` - use iterator find and splice operations
- [x] `insert_segments()` - use list insertion methods (`segments_.splice()`)
- [x] `delete_segments()` - use iterator-based deletion (`segments_.erase()`)
- [x] Each conversion followed by test passage verification (compilation successful, core tests running)
- [x] Define type Segment::iterator = std::list<Segment>::iterator.
- [x] Define type Segment::const_iterator = std::list<Segment>::const_iterator.

## Phase 5: Remove Legacy Code (Started - Conservative Approach)
- [ ] Modify Tempfile method write_line_to_temp() to return `std::list<Segment>` instead of pointer
- [ ] Modify Tempfile method write_lines_to_temp() to return `std::list<Segment>` instead of pointer
- [ ] Remove pointer members (`head_`, `cursegm_`) - requires complete refactoring of dual-system usage
- [ ] Remove manual linking code (`seg->next`, `seg->prev`) - some code still depends on this
- [ ] Update static helper functions to use std::list
- [ ] Final cleanup of pointer-based API in Segment class

## Phase 6: Remove Segment Pointer Members
- [ ] Evaluate if `Segment::prev` and `Segment::next` can be removed
- [ ] Update any external code that directly accesses segment pointers
- [ ] Final compilation and full test suite run
- [ ] Performance verification and optimization

## Risks and Mitigations
- **Iterator Invalidation**: Sequence operations carefully
- **Memory Management**: std::list handles cleanup
- **Performance**: std::list splicing is O(1) for most operations
- **Testing**: Run workspace unit tests after each phase
- **Backward Compatibility**: Test extensively with editor operations
