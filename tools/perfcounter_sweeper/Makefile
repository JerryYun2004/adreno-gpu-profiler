# Build from macOS/Linux host with Android NDK installed.
# Example:
#   make NDK=$HOME/android-ndk-r27d API=35 HOST_TAG=darwin-x86_64

NDK ?= $(HOME)/android-ndk-r27d
API ?= 35
HOST_TAG ?= darwin-x86_64
CC := $(NDK)/toolchains/llvm/prebuilt/$(HOST_TAG)/bin/aarch64-linux-android$(API)-clang

CFLAGS ?= -O2 -Wall -Wextra -std=c11
TARGET := streamer_sweeper
SRC := streamer_sweeper.c
TABLE := a8xx_perf_table.inc
XML ?= a8xx_perfcntrs.xml

.PHONY: all clean table push

all: $(TARGET)

table: $(TABLE)

$(TABLE): generate_a8xx_perf_table.py $(XML)
	python3 generate_a8xx_perf_table.py $(XML) > $(TABLE)

$(TARGET): $(SRC) $(TABLE)
	$(CC) $(CFLAGS) -o $@ $(SRC)

push: $(TARGET)
	adb shell 'su -c "mkdir -p /data/local/tmp/jerry_work/perfcounter_sweeps"'
	adb push $(TARGET) /data/local/tmp/$(TARGET)
	adb shell chmod 755 /data/local/tmp/$(TARGET)

clean:
	rm -f $(TARGET)
