CMAKE ?= cmake
BUILD_DIR ?= build

# Release or Debug
BUILD_TYPE ?= Debug

# clang-format binary (override if needed)
CLANG_FORMAT ?= clang-format

# Extra arguments to pass to CMake configure step, e.g.:
# make CMAKE_ARGS="-DCMAKE_PREFIX_PATH=$(shell brew --prefix ncurses)"
CMAKE_ARGS ?=

all:    $(BUILD_DIR)
	$(MAKE) -C $(BUILD_DIR)

$(BUILD_DIR):
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_ARGS)

install: all
	$(CMAKE) --install $(BUILD_DIR)

reindent:
	@echo "Running clang-format on C++ sources..."
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo "Error: $(CLANG_FORMAT) not found in PATH"; exit 1; }
	@$(CLANG_FORMAT) -i *.h *.cpp tests/*.h tests/*.cpp

test:   all
	ctest --test-dir $(BUILD_DIR)/tests

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all install reindent clean
