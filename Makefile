CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -Iinclude
RELEASE = -O2 -DNDEBUG
DEBUG   = -g -fsanitize=address,undefined

SRC     = src/main.c src/pipe.c src/monitor.c src/safety.c \
          src/summary.c src/suggest.c src/history.c

OUT     = build/nova-shell

.PHONY: all release debug clean

all: release

release:
	@mkdir -p build
	$(CC) $(CFLAGS) $(RELEASE) $(SRC) -o $(OUT)
	@echo "✅  Built: $(OUT)  [release -O2]"

debug:
	@mkdir -p build
	$(CC) $(CFLAGS) $(DEBUG) $(SRC) -o $(OUT)-debug
	@echo "🐛  Built: $(OUT)-debug  [debug + ASan/UBSan]"

clean:
	rm -rf build/
	@echo "🧹  Cleaned build directory"
