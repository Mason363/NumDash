ARM_CC ?= arm-none-eabi-gcc
CC = cc
PYTHON ?= python3
NWLINK = node node_modules/nwlink/bin/nwlink
WARN = -Wall -Wextra -Werror -Wno-misleading-indentation
CORE = src/game.c src/level.c src/inflate.c src/leveldata.c src/objdefs.c src/assets.c src/gfx.c src/scene.c src/fx.c src/save.c
COMMON = $(CORE) src/ui.c src/app.c src/menu.c src/play.c src/editor.c
DEVICE_SOURCES = $(COMMON) src/platform_eadk.c src/main.c
DEVICE_OBJECTS = $(patsubst src/%.c,build/arm/%.o,$(DEVICE_SOURCES))
HEADERS = $(wildcard src/*.h)
ARM_FLAGS = -std=c11 -O2 -fno-math-errno $(WARN) -ffunction-sections -fdata-sections -flto -fno-fat-lto-objects -fwhole-program -fvisibility=internal $(shell $(NWLINK) eadk-cflags-device 2>/dev/null)
ARM_LINK = -Wl,--relocatable -nostartfiles --specs=nano.specs -Wl,--gc-sections -Wl,-e,main -Wl,-u,eadk_app_name -Wl,-u,eadk_app_icon -Wl,-u,eadk_api_level -flinker-output=nolto-rel
.PHONY: all build simulator test check clean levels assets run epsilon-app
all: build
build: build/numdash.nwa
node_modules/nwlink/bin/nwlink: package.json package-lock.json
	npm ci
	touch $@
# Regenerates the level data and artwork (needs Python 3 with numpy and Pillow).
levels:
	$(PYTHON) tools/levels.py
assets:
	$(PYTHON) tools/assets.py
build/.stamp:
	mkdir -p build
	touch $@
build/icon.o: assets/icon.png node_modules/nwlink/bin/nwlink | build/.stamp
	$(NWLINK) png-icon-o $< $@
build/arm/%.o: src/%.c $(HEADERS) node_modules/nwlink/bin/nwlink | build/.stamp
	mkdir -p build/arm
	$(ARM_CC) $(ARM_FLAGS) -c $< -o $@
build/numdash.nwa: $(DEVICE_OBJECTS) build/icon.o | build/.stamp
	$(ARM_CC) $(ARM_FLAGS) $(ARM_LINK) $(DEVICE_OBJECTS) build/icon.o -lm -o $@
check: build/numdash.nwa
	$(NWLINK) nwa-bin $< build/numdash.bin
simulator: build/numdash-sim
build/numdash-sim: $(COMMON) src/platform_sdl.c src/main.c $(HEADERS) | build/.stamp
	$(CC) -std=c11 -O2 $(WARN) -Isrc $$(sdl2-config --cflags) $(COMMON) src/platform_sdl.c src/main.c $$(sdl2-config --libs) -lm -o $@
build/tests: $(COMMON) tests/test.c $(HEADERS) | build/.stamp
	$(CC) -std=c11 -O1 -g $(WARN) -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc $(COMMON) tests/test.c -lm -o $@
test: build/tests
	./build/tests
	@for i in 1 2 3 4 5 6 7; do ./build/tests --replay $$((i-1)) tests/replays/level$$i.txt || exit 1; done
run: simulator
	./build/numdash-sim
# Native module for the official Epsilon simulator (nwlink >= 0.0.19).
epsilon-app: | build/.stamp
	$(CC) -std=c11 -O2 -Isrc $(shell $(NWLINK) eadk-cflags-simulator) $(COMMON) src/platform_eadk.c src/main.c $(shell $(NWLINK) eadk-ldflags-simulator) -Wl,-undefined,dynamic_lookup -o build/numdash.nwb
clean:
	rm -rf build
