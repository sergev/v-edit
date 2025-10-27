#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include <string>
#include <vector>

//
// Clipboard class for managing copy/paste operations.
// Supports both line-based and rectangular block operations.
//
class Clipboard {
public:
    Clipboard();

    // Access to clipboard content
    bool is_empty() const;
    bool is_rectangular() const;
    int get_start_line() const;
    int get_end_line() const;
    int get_start_col() const;
    int get_end_col() const;
    const std::vector<std::string> &get_lines() const;

    // Basic operations
    void clear();

    // Copy operations - take content from source and store in clipboard
    void copy_lines(const std::vector<std::string> &source, int startLine, int count);
    void copy_rectangular_block(const std::vector<std::string> &source, int line, int col,
                                int width, int height);

    // Get clipboard data for operations that need to access it
    struct BlockData {
        std::vector<std::string> lines;
        int start_line;
        int end_line;
        int start_col;
        int end_col;
        bool is_rectangular;
    };
    BlockData get_data() const;

    // Set clipboard data (for deserialization)
    void set_data(bool rect, int s_line, int e_line, int s_col, int e_col,
                  const std::vector<std::string> &lines);

    // Paste operations - apply clipboard content to a target buffer
    void paste_into_lines(std::vector<std::string> &target, int afterLine);
    void paste_into_rectangular(std::vector<std::string> &target, int afterLine, int atCol);

    // Serialization support
    void serialize(std::ostream &out) const;
    void deserialize(std::istream &in);

private:
    std::vector<std::string> lines_;
    int start_line_;
    int end_line_;
    int start_col_;
    int end_col_;
    bool m_is_rectangular_;
};

#endif
