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
- [ ] Add validation methods to ensure list and pointers stay synchronized
- [ ] Write helper functions to build list from pointers and vice versa
- [ ] Test synchronization with existing functionality

## Phase 3: Change Accessors
- [x] Modify `chain()` to return from list front
- [x] Modify `cursegm()` to return based on iterator position
- [x] Maintain backward compatibility with pointer API
- [x] Run tests to ensure external callers see no difference

## Phase 4: Convert Core Operations (One-by-One)
- [x] `set_current_segment()` - replace pointer navigation with iterator navigation
- [x] `breaksegm()` split segments - use `segments_.insert()` instead of manual linking
- [x] `catsegm()` - use iterator find and splice operations
- [x] `insert_segments()` - use list insertion methods
- [x] `delete_segments()` - use iterator-based deletion
- [ ] Each conversion followed by test passage verification

## Phase 5: Remove Legacy Code
- [ ] Remove pointer members (`head_`, `cursegm_`)
- [ ] Remove manual linking code (`seg->next`, `seg->prev`)
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
