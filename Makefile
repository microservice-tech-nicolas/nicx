# nicx — Makefile wrapper around CMake
# Provides a familiar make interface: make, make install, make clean, etc.

PREFIX    ?= /usr/local
BUILD_DIR := build
GENERATOR := $(shell command -v ninja >/dev/null 2>&1 && echo "Ninja" || echo "Unix Makefiles")
BUILD_TOOL:= $(shell command -v ninja >/dev/null 2>&1 && echo "ninja" || echo "make")
NPROC     := $(shell nproc 2>/dev/null || echo 4)

# Terminal colors
BOLD  := \033[1m
CYAN  := \033[36m
GREEN := \033[32m
RESET := \033[0m

.PHONY: all build install uninstall clean distclean help test

##@ General

help: ## Show this help
	@printf "\n$(BOLD)$(CYAN)nicx$(RESET) — Linux system toolkit\n\n"
	@printf "$(BOLD)Usage:$(RESET)\n"
	@printf "  make $(CYAN)[target]$(RESET) $(GREEN)[PREFIX=/usr/local]$(RESET)\n\n"
	@printf "$(BOLD)Targets:$(RESET)\n"
	@awk 'BEGIN {FS = ":.*##"} /^[a-zA-Z_-]+:.*##/ { printf "  $(CYAN)%-14s$(RESET) %s\n", $$1, $$2 }' $(MAKEFILE_LIST)
	@printf "\n$(BOLD)Examples:$(RESET)\n"
	@printf "  make                     Build in release mode\n"
	@printf "  make install             Install to /usr/local (requires sudo)\n"
	@printf "  make install PREFIX=~/.local   Install to ~/.local (no sudo)\n"
	@printf "  make BUILD_TYPE=Debug    Build with debug symbols\n"
	@printf "  make uninstall           Remove installed files\n"
	@printf "\n$(BOLD)After install:$(RESET)\n"
	@printf "  nicx system info\n"
	@printf "  man nicx\n\n"

##@ Build

all: build ## Build the project (default)

build: ## Build in release mode
	@printf "$(CYAN)→$(RESET) Configuring with $(GENERATOR)...\n"
	@cmake -S . -B $(BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=$(or $(BUILD_TYPE),Release) \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX) \
		-G "$(GENERATOR)" \
		-Wno-dev --log-level=WARNING
	@printf "$(CYAN)→$(RESET) Building...\n"
	@cmake --build $(BUILD_DIR) --parallel $(NPROC)
	@printf "$(GREEN)✓$(RESET) Built: $(BUILD_DIR)/nicx\n"

debug: ## Build with debug symbols and sanitizers
	@$(MAKE) build BUILD_TYPE=Debug

##@ Install

install: build ## Install binary + man page (default PREFIX=/usr/local)
	@printf "$(CYAN)→$(RESET) Installing to $(PREFIX)...\n"
	@cmake --install $(BUILD_DIR) --prefix $(PREFIX)
	@mkdir -p $(PREFIX)/share/man/man1
	@install -m 644 docs/nicx.1 $(PREFIX)/share/man/man1/nicx.1
	@printf "$(GREEN)✓$(RESET) Installed:\n"
	@printf "    $(PREFIX)/bin/nicx\n"
	@printf "    $(PREFIX)/share/man/man1/nicx.1\n"
	@printf "\n$(BOLD)Run:$(RESET) nicx system info\n"
	@printf "$(BOLD)Man:$(RESET) man nicx\n\n"

install-user: ## Install to ~/.local without sudo
	@$(MAKE) install PREFIX=$(HOME)/.local
	@printf "\n$(BOLD)Ensure ~/.local/bin is in your PATH:$(RESET)\n"
	@printf "  export PATH=\"$$HOME/.local/bin:$$PATH\"\n\n"

uninstall: ## Remove installed binary and man page
	@printf "$(CYAN)→$(RESET) Uninstalling from $(PREFIX)...\n"
	@rm -f $(PREFIX)/bin/nicx
	@rm -f $(PREFIX)/share/man/man1/nicx.1
	@printf "$(GREEN)✓$(RESET) Uninstalled\n"

##@ Test & Check

test: build ## Run the binary against the current system
	@printf "\n$(BOLD)$(CYAN)── nicx --version ──────────────────$(RESET)\n"
	@$(BUILD_DIR)/nicx --version
	@printf "\n$(BOLD)$(CYAN)── nicx system info ────────────────$(RESET)\n"
	@$(BUILD_DIR)/nicx system info
	@printf "\n$(BOLD)$(CYAN)── nicx --help ─────────────────────$(RESET)\n"
	@$(BUILD_DIR)/nicx --help
	@printf "$(GREEN)✓$(RESET) All checks passed\n\n"

##@ Cleanup

clean: ## Remove build artifacts
	@rm -rf $(BUILD_DIR)
	@printf "$(GREEN)✓$(RESET) Cleaned build directory\n"

distclean: clean ## Remove build artifacts and generated files
	@rm -rf compile_commands.json
	@printf "$(GREEN)✓$(RESET) Full clean done\n"
