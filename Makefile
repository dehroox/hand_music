Q := @
MAKEFLAGS += --no-print-directory
.DELETE_ON_ERROR:

PROFILE ?= debug
BUILD_DIR := build/$(PROFILE)

CC := ccache gcc

COMMON_FLAGS := \
    -std=c23 \
    -Wall -Wextra \
    -pedantic -pedantic-errors \
    -Wstrict-prototypes -Wold-style-definition \
    -Wmissing-prototypes -Wmissing-declarations \
    -Wredundant-decls -Wshadow -Wcast-qual -Wconversion \
    -Wsign-conversion -Wstrict-aliasing=3 -Wstrict-overflow=5 \
    -Wswitch-default -Wundef -Winit-self -Wparentheses \
    -Wfloat-equal -Wformat=2 -Wunreachable-code -Wunused \
    -Wvariadic-macros -Wvla -Wpacked -Wstrict-overflow -Wduplicated-branches \
    -Wduplicated-cond -Wlogical-op -Wrestrict -Warray-bounds \
    -Wmissing-include-dirs -Winline -Wstack-protector -Woverlength-strings \
    -Wnull-dereference -Wdouble-promotion -Wunsafe-loop-optimizations \
    -Wrestrict -Wpointer-arith -Wsign-compare \
    -Wbad-function-cast -Wmissing-format-attribute \
    -Wformat-security -Wstrict-aliasing=3 -Werror \
    -Wformat-nonliteral \
    -Wformat-signedness \
    -Wformat-truncation \
    -Wformat-overflow \
    -Wtrampolines \
    -Wvector-operation-performance \
    -Wabsolute-value \
    -fstrict-aliasing -fno-omit-frame-pointer -mavx2 \
    -D_FORTIFY_SOURCE=2 \
    -D_POSIX_C_SOURCE=200809L \
    $(shell pkg-config --cflags x11 | awk 'gsub("-I", "-isystem ")') \
    -I$(CURDIR)/$(SRCDIR) \
    -I$(CURDIR)/src/common \
    -I$(CURDIR)/src/frontend/include \
    -I$(CURDIR)/src/image/include


COMMON_LD_FLAGS := $(shell pkg-config --libs x11)

ifeq ($(PROFILE),debug)
    CFLAGS := $(COMMON_FLAGS) -O0 -g3 -DDEBUG \
                -fsanitize=address,undefined,leak \
                -fno-optimize-sibling-calls
    LDFLAGS :=  -fuse-ld=mold -Wl,-z,relro,-z,now -Wl,-z,noexecstack -pie $(COMMON_LD_FLAGS) -fsanitize=address,undefined,leak 
else ifeq ($(PROFILE),release)
    CFLAGS := $(COMMON_FLAGS) -O3 -flto -DNDEBUG -march=native -mtune=native -ffast-math -mavx2

    LDFLAGS := -fuse-ld=mold -Wl,-z,relro,-z,now -Wl,-z,noexecstack -pie $(COMMON_LD_FLAGS) -flto
else
    $(error Unknown profile "$(PROFILE)". Use: debug, release)
endif

SRCDIR := src
OBJDIR := $(BUILD_DIR)/obj
BINDIR := $(BUILD_DIR)
TARGET := $(BINDIR)/hand_music

SOURCES := $(shell find $(SRCDIR) -name "*.c")
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

all: $(TARGET)

-include $(DEPS)

$(TARGET): $(OBJECTS) | $(BINDIR)
	@echo "  LINK    $@"
	$(Q)$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lpthread

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo "  CC      $<"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR) $(BINDIR):
	$(Q)mkdir -p $@

clean:
	@echo "  CLEAN   all"
	$(Q)rm -rf build compile_commands.json

run: $(TARGET)
	@echo "  RUN     $(TARGET)"
ifeq ($(PROFILE),debug)
	$(Q)ASAN_OPTIONS=detect_stack_use_after_return=true:check_initialization_order=true:strict_init_order=true:abort_on_error=1:detect_leaks=1 \
	   UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
	   $< 
else
	$(Q)$<

endif

HEADERS := $(shell find $(SRCDIR) -name "*.h")

info:
	@echo "Profile: $(PROFILE)"
	@echo "Target: $(TARGET)"

compile_commands:
	@echo "  GEN     compile_commands.json"
	$(Q)bear -- make PROFILE=$(PROFILE) clean all

lint: compile_commands
	@echo "  LINT    all"
	$(Q)clang-tidy -p compile_commands.json $(SOURCES) --extra-arg=-Wno-unknown-warning-option

format:
	@echo "  FORMAT  all"
	$(Q)clang-format -i $(SOURCES) $(HEADERS)

check-format:
	@echo "  CHECK-FORMAT all"
	$(Q)clang-format --dry-run --Werror $(SOURCES) $(HEADERS)

help:
	@echo "Available targets:"
	@echo "  all             - Build the project"
	@echo "  clean           - Remove build artifacts"
	@echo "  run             - Run the built executable"
	@echo "  info            - Show build info"
	@echo "  compile_commands - Generate compile_commands.json"
	@echo "  lint            - Run clang-tidy static analysis"
	@echo "  format          - Format code with clang-format"
	@echo "  check-format    - Check code formatting with clang-format"
	@echo "  help            - Show this help"

.PHONY: all clean run info compile_commands help lint format check-format
