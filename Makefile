Q := @
MAKEFLAGS += --no-print-directory
.DELETE_ON_ERROR:

PROFILE ?= debug
BUILD_DIR := build/$(PROFILE)

CC := ccache gcc

COMMON_FLAGS := -std=c23 -Wall -Wextra -pedantic -pedantic-errors -Wcast-qual -Wconversion -Wdisabled-optimization -Wfloat-equal -Wformat=2 -Winit-self -Wmissing-include-dirs -Wpacked -Wparentheses -Wredundant-decls -Wshadow -Wsign-conversion -Wstrict-aliasing=2 -Wstrict-overflow=5 -Wswitch-default -Wundef -Wunreachable-code -Wunused -Wvariadic-macros -Wvla -fstrict-aliasing -fno-omit-frame-pointer -march=native -mtune=native -mavx2 $(shell pkg-config --cflags x11 | awk 'gsub("-I", "-isystem ")') -I$(CURDIR)/$(SRCDIR) -I$(CURDIR)/src/common -I$(CURDIR)/src/frontend/include -I$(CURDIR)/src/capture/include

COMMON_LD_FLAGS := $(shell pkg-config --libs x11)

ifeq ($(PROFILE),debug)
    CFLAGS := $(COMMON_FLAGS) -O0 -g3 -DDEBUG \
                -fsanitize=address,undefined,leak \
                -fno-optimize-sibling-calls
    LDFLAGS := $(COMMON_LD_FLAGS) -fsanitize=address,undefined,leak \
               -fuse-ld=gold -Wl,-z,relro,-z,now -Wl,-z,noexecstack -pie
else ifeq ($(PROFILE),release)
    CFLAGS := $(COMMON_FLAGS) -O3 -flto -DNDEBUG -march=native -mtune=native -ffast-math -mavx2

    LDFLAGS := $(COMMON_LD_FLAGS) -flto -fuse-ld=gold -Wl,-z,relro,-z,now -Wl,-z,noexecstack -pie
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

info:
	@echo "Profile: $(PROFILE)"
	@echo "Target: $(TARGET)"

compile_commands:
	@echo "  GEN     compile_commands.json"
	$(Q)bear -- make PROFILE=$(PROFILE) clean all

help:
	@echo "Available targets:"
	@echo "  all             - Build the project"
	@echo "  clean           - Remove build artifacts"
	@echo "  run             - Run the built executable"
	@echo "  info            - Show build info"
	@echo "  compile_commands - Generate compile_commands.json"
	@echo "  help            - Show this help"

.PHONY: all clean run info compile_commands help
