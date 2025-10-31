#include "EditorDriver.h"

void EditorDriver::SetUp()
{
    editor = std::make_unique<Editor>();

    // Manually initialize editor state needed for tests
    editor->wksp_     = std::make_unique<Workspace>(editor->tempfile_);
    editor->alt_wksp_ = std::make_unique<Workspace>(editor->tempfile_);

    // Open shared temp file
    editor->tempfile_.open_temp_file();

    // Initialize view state
    editor->wksp_->view.basecol = 0;
    editor->wksp_->view.topline = 0;
    editor->cursor_col_         = 0;
    editor->cursor_line_        = 0;
    editor->ncols_              = 80;
    editor->nlines_             = 24;
    editor->insert_mode_        = true; // Use insert mode
}

void EditorDriver::TearDown()
{
    editor.reset();
}

void EditorDriver::CreateBlankLines(unsigned num_lines)
{
    auto blank = Workspace::create_blank_lines(num_lines);
    editor->wksp_->insert_contents(blank, 0);
}

void EditorDriver::CreateLine(int line_no, const std::string &content)
{
    // Ensure enough blank lines exist
    int current_count = editor->wksp_->total_line_count();
    if (line_no >= current_count) {
        auto blank = Workspace::create_blank_lines(line_no - current_count + 1);
        editor->wksp_->insert_contents(blank, current_count);
    }

    // Set the content
    editor->current_line_          = content;
    editor->current_line_no_       = line_no;
    editor->current_line_modified_ = true;
    editor->put_line();
}

void EditorDriver::LoadLine(int line_no)
{
    editor->get_line(line_no);
}

size_t EditorDriver::GetActualCol() const
{
    return editor->get_actual_col();
}

std::string EditorDriver::createTestFile(const std::string &content)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string filename = testName + ".txt";
    std::ofstream f(filename);
    f << content;
    f.close();
    return filename;
}

void EditorDriver::cleanupTestFile(const std::string &filename)
{
    std::filesystem::remove(filename);
}
