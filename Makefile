# rflcpp root Makefile
# Convenience wrapper around CMake for building, testing, and installing.
#
# Common targets:
#   make           - configure + build (Release)
#   make debug     - configure + build (Debug)
#   make examples  - build only the examples
#   make tests     - build and run the tests
#   make install   - install the library (use PREFIX=/path to override)
#   make clean     - remove the build directory
#   make format    - run clang-format over the codebase (if available)
#   make docs      - build documentation (if a docs generator is configured)
#   make help      - print this help

# ---- Configurable options ---------------------------------------------------

BUILD_DIR        ?= build
BUILD_TYPE       ?= Release
PREFIX           ?= /usr/local
JOBS             ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
CMAKE            ?= cmake
CTEST            ?= ctest
CXX              ?= clang++
GENERATOR        ?= Unix Makefiles

# Toggle features
RFLCPP_BUILD_EXAMPLES ?= ON
RFLCPP_BUILD_TESTS    ?= ON
RFLCPP_BUILD_DOCS     ?= OFF

CMAKE_FLAGS = \
    -G "$(GENERATOR)" \
    -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
    -DCMAKE_INSTALL_PREFIX=$(PREFIX) \
    -DCMAKE_CXX_COMPILER=$(CXX) \
    -DRFLCPP_BUILD_EXAMPLES=$(RFLCPP_BUILD_EXAMPLES) \
    -DRFLCPP_BUILD_TESTS=$(RFLCPP_BUILD_TESTS) \
    -DRFLCPP_BUILD_DOCS=$(RFLCPP_BUILD_DOCS) \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# ---- Top-level targets ------------------------------------------------------

.PHONY: all configure build debug release examples tests test install \
        uninstall clean distclean format docs help reconfigure submodules

all: build

# Ensure git submodules (notably Catch2 under submodules/Catch2) are present
# before configuring. Skipped if this isn't a git checkout.
submodules:
	@if [ -f .gitmodules ] && [ -d .git ]; then \
	    if [ ! -f submodules/Catch2/CMakeLists.txt ]; then \
	        echo "Initializing git submodules..."; \
	        git submodule update --init --recursive; \
	    fi; \
	fi

configure: submodules $(BUILD_DIR)/CMakeCache.txt

$(BUILD_DIR)/CMakeCache.txt:
	@mkdir -p $(BUILD_DIR)
	$(CMAKE) -S . -B $(BUILD_DIR) $(CMAKE_FLAGS)

reconfigure:
	@rm -f $(BUILD_DIR)/CMakeCache.txt
	@$(MAKE) configure

build: configure
	$(CMAKE) --build $(BUILD_DIR) --parallel $(JOBS)

release:
	@$(MAKE) BUILD_TYPE=Release build

debug:
	@$(MAKE) BUILD_TYPE=Debug build

examples: configure
	$(CMAKE) --build $(BUILD_DIR) --target rflcpp_examples --parallel $(JOBS)

tests: configure
	$(CMAKE) --build $(BUILD_DIR) --target rflcpp_tests --parallel $(JOBS)
	cd $(BUILD_DIR) && $(CTEST) --output-on-failure -j $(JOBS)

test: tests

install: build
	$(CMAKE) --install $(BUILD_DIR) --prefix $(PREFIX)

uninstall:
	@if [ -f $(BUILD_DIR)/install_manifest.txt ]; then \
	    xargs rm -vf < $(BUILD_DIR)/install_manifest.txt; \
	else \
	    echo "No install_manifest.txt found in $(BUILD_DIR); nothing to uninstall."; \
	fi

clean:
	@if [ -d $(BUILD_DIR) ]; then $(CMAKE) --build $(BUILD_DIR) --target clean || true; fi

distclean:
	rm -rf $(BUILD_DIR)

format:
	@if command -v clang-format >/dev/null 2>&1; then \
	    find include src examples tests -type f \( -name '*.hpp' -o -name '*.cpp' \) \
	        -exec clang-format -i {} +; \
	    echo "Formatted sources with clang-format."; \
	else \
	    echo "clang-format not found; skipping."; \
	fi

docs: configure
	@$(CMAKE) --build $(BUILD_DIR) --target rflcpp_docs || \
	    echo "Docs target unavailable (set RFLCPP_BUILD_DOCS=ON and install Doxygen)."

help:
	@echo "rflcpp build targets:"
	@echo "  make            Build the library (Release)"
	@echo "  make debug      Build the library (Debug)"
	@echo "  make examples   Build all examples"
	@echo "  make tests      Build and run tests"
	@echo "  make install    Install to PREFIX (default: $(PREFIX))"
	@echo "  make uninstall  Remove installed files"
	@echo "  make clean      Remove build artifacts"
	@echo "  make distclean  Remove the entire build directory"
	@echo "  make format     Run clang-format over the sources"
	@echo "  make docs       Build documentation (requires RFLCPP_BUILD_DOCS=ON)"
	@echo ""
	@echo "Override variables on the command line, e.g.:"
	@echo "  make BUILD_TYPE=Debug PREFIX=\$$HOME/.local"
	@echo "  make CXX=clang++-20 GENERATOR=Ninja"
