#!/usr/bin/env perl
#
# Automated refactoring script to replace deprecated Workspace getter/setter methods
# with direct struct member access.
#
# Usage: perl refactor_workspace_getters_setters.pl
#

use strict;
use warnings;
use File::Find;
use File::Copy;

print "Starting Workspace getter/setter refactoring...\n";
print "This script will update all .cpp files in the current directory and tests/\n\n";

# Find all .cpp files
my @files;
find(sub {
    push @files, $File::Find::name if /\.cpp$/;
}, '.', 'tests');

# Backup files first
print "Creating backups...\n";
foreach my $file (@files) {
    copy($file, "$file.bak") or die "Cannot backup $file: $!";
}

print "\nApplying replacements...\n";

# Process each file
foreach my $file (@files) {
    print "Processing: $file\n";
    
    # Read file content
    open(my $fh, '<', $file) or die "Cannot read $file: $!";
    my $content = do { local $/; <$fh> };
    close($fh);
    
    # Apply replacements
    # Note: Order matters - do setters before getters to avoid double replacement
    
    # File state setters
    $content =~ s/->set_nlines\(([^)]+)\)/->file_state.nlines = $1/g;
    $content =~ s/->set_writable\(([^)]+)\)/->file_state.writable = $1/g;
    $content =~ s/->set_modified\(([^)]+)\)/->file_state.modified = $1/g;
    $content =~ s/->set_backup_done\(([^)]+)\)/->file_state.backup_done = $1/g;
    
    # View state setters
    $content =~ s/->set_topline\(([^)]+)\)/->view.topline = $1/g;
    $content =~ s/->set_basecol\(([^)]+)\)/->view.basecol = $1/g;
    $content =~ s/->set_cursorcol\(([^)]+)\)/->view.cursorcol = $1/g;
    $content =~ s/->set_cursorrow\(([^)]+)\)/->view.cursorrow = $1/g;
    
    # Position state setters
    $content =~ s/->set_line\(([^)]+)\)/->position.line = $1/g;
    $content =~ s/->set_segmline\(([^)]+)\)/->position.segmline = $1/g;
    
    # File state getters
    $content =~ s/->nlines\(\)/->file_state.nlines/g;
    $content =~ s/->writable\(\)/->file_state.writable/g;
    $content =~ s/->modified\(\)/->file_state.modified/g;
    $content =~ s/->backup_done\(\)/->file_state.backup_done/g;
    
    # View state getters
    $content =~ s/->topline\(\)/->view.topline/g;
    $content =~ s/->basecol\(\)/->view.basecol/g;
    $content =~ s/->cursorcol\(\)/->view.cursorcol/g;
    $content =~ s/->cursorrow\(\)/->view.cursorrow/g;
    
    # Position state getters
    $content =~ s/->line\(\)/->position.line/g;
    $content =~ s/->segmline\(\)/->position.segmline/g;
    
    # Write back
    open($fh, '>', $file) or die "Cannot write $file: $!";
    print $fh $content;
    close($fh);
}

print "\nRefactoring complete!\n\n";
print "Next steps:\n";
print "1. Review the changes: git diff\n";
print "2. Build the project: cmake -S . -B build && make -C build\n";
print "3. Run tests: ./build/tests/v_edit_tests\n";
print "4. If everything works, remove backups: rm *.cpp.bak tests/*.cpp.bak\n";
print "5. If there are issues, restore backups:\n";
print "   find . -name '*.cpp.bak' | while read f; do mv \"\$f\" \"\${f%.bak}\"; done\n\n";
