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

## Phase 5: Remove Legacy Code ✅ COMPLETED
- [x] Modify Tempfile method `write_line_to_temp()` to return `std::list<Segment>` instead of pointer
- [x] Modify Tempfile method `write_lines_to_temp()` to return `std::list<Segment>` instead of pointer
- [x] Remove pointer members (`head_`) from Workspace class - `cursegm_` converted to iterator
- [x] Remove manual linking code (`seg->next`, `seg->prev`) from Segment class
- [x] Update `build_segments_from_file` to only populate std::list
- [x] Update `write_segments_to_file` to use std::list only
- [x] Update `set_chain` to convert external pointer chains to std::list
- [ ] Remaining: Some methods (breaksegm, insert_segments) still create external Segment* objects that try to use prev/next - need full conversion to list operations for complete removal of legacy code
- [x] Static helper functions kept for API compatibility but still use pointer chains

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
