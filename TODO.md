# Debugging KeyHandlingIntegrationTest.CharacterInsertionFooBarQuz

## Current Issue

The test types "foo\nbar\nquz\n" character by character into an empty workspace. After typing 'u' (in "qu"), the workspace has:
- Expected: 3 lines ("foo", "bar", "quz")
- Actual: 4 lines with duplicate data

### Segment Chain After Typing 'u':
```
Segment 0: 1 line at offset 9, lengths={4}     // "foo\n"
Segment 1: 2 lines at offset 22, lengths={4,2} // "bar\n", "q\n" 
Segment 2: 1 line at offset 28, lengths={3}    // "qu\n"
```

**Problem**: Line 2 ("q\n") exists in BOTH segment 1 AND segment 2 (as "qu\n").

## Root Cause Analysis

When `put_line(2, "qu")` is called on a workspace where segment 1 contains lines 1 and 2:
1. `breaksegm(2, true)` is called - splits so cursegm_ points to segment starting at line 2
2. Since the segment has 2 lines, we call `breaksegm(3, false)` to isolate line 2
3. After this split, cursegm_ points to segment starting at line 3
4. We do `--cursegm_` to get back to the segment containing line 2
5. We replace this segment with the new "qu\n" segment

**The Bug**: After `breaksegm(3, false)`, the segment chain is:
- Segment 1a: line 1 ("bar\n") 
- Segment 1b: line 2 ("q\n") 
- Segment after: line 3+

But the split may not be removing line 2 from segment 1a! The `breaksegm` function splits at line 3, creating:
- Original segment keeps lines 0-2 (but should only keep 0-1)
- New segment gets lines 3+

## Required Fix

The issue is in `put_line` when `break_result == 0` and the segment has multiple lines. After calling `breaksegm(line_no + 1, false)` to split after line_no, we need to verify that:
1. The previous segment only contains lines UP TO (but not including) line_no
2. The segment we're replacing only contains line_no
3. The next segment contains lines after line_no

Alternative: Instead of trying to isolate line_no by splitting before and after, we should:
1. Split the segment at line_no (so cursegm_ is at line_no)  
2. If segment has more than 1 line, delete lines after line_no from this segment
3. Replace the segment
4. Ensure the remaining lines are in the next segment

## Next Steps

1. Review the `breaksegm` split logic when splitting a multi-line segment
2. Verify that after `breaksegm(line_no + 1)`, the segment at line_no has exactly 1 line
3. Fix the segment modification to not leave duplicate data
4. Test with the enhanced test to verify the fix
