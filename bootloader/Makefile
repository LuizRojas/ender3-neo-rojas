# Requer arm-none-eabi-gcc (GNU Arm Embedded Toolchain)
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size
CFLAGS  = -mcpu=cortex-m3 -mthumb -Os -Wall -Wextra -ffunction-sections -fdata-sections \
          -ffreestanding -std=c11
LDFLAGS = -nostartfiles -T linker.ld -Wl,--gc-sections -Wl,-Map=build/bootloader.map
SRC     = src/main.c src/sdio.c src/fat.c src/update.c

all: build/bootloader.bin

build/bootloader.elf: $(SRC) src/*.h linker.ld
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -lc -lgcc -o $@
	$(SIZE) $@

build/bootloader.bin: build/bootloader.elf
	$(OBJCOPY) -O binary $< $@

test:
	@mkdir -p build
	cc -std=c11 -Wall -Wextra -g -fsanitize=address,undefined -Isrc \
	   test/host_test.c src/fat.c src/update.c -o build/host_test
	sh test/run_tests.sh

clean:
	rm -rf build

.PHONY: all test clean
