# Rectangular Block Operations

Rectangular blocks are defined by selecting a rectangular area of text using the cursor in command mode. Unlike line-based operations, rectangular block operations work on a specific column range across multiple lines.

## How to Define a Rectangular Block

1. **Move cursor to the top-left corner** of the desired rectangular block
2. Press **F1** or **^A** to enter command mode
   - The symbol at the cursor position is temporarily replaced by an inverted '@'
   - The cursor jumps to the command line
3. **Use arrow keys** to move cursor to the bottom-right corner of the desired block
   - As soon as you start moving, a message "Area defined by cursor" appears in the command line
   - The cursor returns to the text window
   - You can continue moving the cursor to adjust the selection
4. **Press the action key** (^C, ^Y, or ^O) to execute the operation
   - The rectangular area is defined between the initial and final cursor positions
   - The selection mode is terminated by the action itself
   - Press **ESC**, **F1**, or **^A** to cancel the selection without performing an action

## Operations

### Copy a Rectangular Block (^C in Cmd Mode)

1. Define the rectangular block as described above
2. Press **^C** in command mode
3. The rectangular block of text is copied to the clipboard
4. The block's bounds and content are saved for rectangular paste operations

### Paste a Rectangular Block (^V)

1. Move cursor to the desired insertion position (top-left corner where you want to paste)
2. Press **^V**
3. The rectangular block from the clipboard is inserted at this position
4. Text is inserted on the current line and subsequent lines below it
5. Existing text is shifted to the right to make room for the block

### Delete a Rectangular Block (^Y in Cmd Mode)

1. Define the rectangular block as described above
2. Press **^Y** in command mode
3. The rectangular block of text is deleted
4. Text to the right of the block shifts left to fill the gap

### Insert a Rectangular Block of Spaces (^O in Cmd Mode)

1. Define the rectangular block as described above
2. Press **^O** in command mode
3. A rectangular block of spaces is inserted at the defined position
4. Existing text is shifted to the right to make room for the spaces

## Differences from Line Operations

- **Line operations** (F5/F6 or ^C/^V without area selection in edit mode): Work on entire lines
- **Rectangular operations**: Work on specific column ranges across multiple lines

The clipboard tracks whether it contains a rectangular block or lines, allowing the paste operation to behave appropriately.

## Quick Access from Edit Mode

Even though rectangular block operations require entering command mode, there are equivalent operations that work in edit mode:

- **F5** - Copy current line (works in edit mode)
- **F6** - Paste clipboard (works in edit mode)
- **^C** - Copy line or rectangular block (context-dependent)
- **^V** - Paste clipboard (works in edit mode)

For rectangular blocks specifically, you must:
1. Enter command mode (F1 or ^A)
2. Move cursor to define rectangular area
3. Press ^C, ^Y, or ^O to perform the operation
4. Press ^V to paste (works in edit mode)

## Implementation Details

- The clipboard's `is_rectangular` flag distinguishes rectangular blocks from line blocks
- Rectangular block bounds are stored in `start_line`, `end_line`, `start_col`, `end_col`
- When pasting a rectangular block, the editor inserts the block column-by-column on each affected line
