CFLAGS += -std=c23 -O3 -flto -pedantic-errors \
	-Wall -Wextra -Werror \
	-Wshadow -Wdouble-promotion -Wformat=2 -Wundef \
	-Wsign-conversion \
	-Wstrict-prototypes -Wmissing-prototypes \
	-Wcast-qual -Wpointer-arith -Wwrite-strings \
	-Wmissing-declarations -Wredundant-decls -Wnested-externs \
	-Winline -Winvalid-pch -Wswitch-default -Wswitch-enum \
	-Wuninitialized -Winit-self \
	-Wfloat-equal -Wlogical-op -Waggregate-return -Wpacked \
	-Wformat-overflow=2 -Wformat-truncation=2 -Walloca -Wvla \
	-Wdisabled-optimization -Wold-style-definition \
	-Wduplicated-cond -Wduplicated-branches \
	-Wrestrict -Wnull-dereference -Wformat-signedness \
	-Wjump-misses-init -Wstringop-overflow=4 -Warray-bounds=2 \
	-Wimplicit-fallthrough=5 \
	-fanalyzer -fstrict-overflow -fstrict-aliasing \
	-fno-common -fno-plt -fipa-pta -fstrict-volatile-bitfields \
	-MMD -MP $(addprefix -I,$(shell find src -type d)) -mavx2

LDFLAGS += -Wl,-O1 -Wl,--as-needed -Wl,--no-undefined \
	-Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack \
	-Wl,--gc-sections -Wl,--icf=all \
	-fuse-ld=mold -lX11

SRC_DIR := src
SRC := $(shell find $(SRC_DIR) -name '*.c')
OBJ := $(patsubst $(SRC_DIR)/%.c,obj/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

OUTPUT ?= hm

$(OUTPUT): $(OBJ)
	cc $(CFLAGS) $^ -o $@ $(LDFLAGS)

obj/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	cc $(CFLAGS) -c $< -o $@

.PHONY: clean clangd

clean:
	rm -rf obj $(OUTPUT) compile_commands.json

clangd:
	bear -- make

-include $(DEP)
