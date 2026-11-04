# Nhóm 14 - Data Logger STM32F103C8 + FreeRTOS
# Build trên Linux:  make            -> build/datalogger.elf/.hex/.bin
#                    make flash      -> nạp qua ST-Link (stlink-tools)
#                    make test       -> chạy unit test phần logic trên PC
#                    make size       -> xem dung lượng Flash/RAM

.RECIPEPREFIX := >

TARGET  := datalogger
BUILD   := build
PREFIX  ?= arm-none-eabi-
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
SIZE    := $(PREFIX)size

RTOS := middlewares/FreeRTOS
COMMA := ,

SRCS := main.c \
        $(wildcard bsp/src/*.c) bsp/system_stm32f1xx.c \
        $(wildcard hal/src/*.c) \
        $(wildcard app/src/*.c) \
        $(RTOS)/tasks.c $(RTOS)/queue.c $(RTOS)/list.c $(RTOS)/timers.c \
        $(RTOS)/event_groups.c $(RTOS)/stream_buffer.c \
        $(RTOS)/portable/GCC/ARM_CM3/port.c $(RTOS)/portable/MemMang/heap_4.c
ASRCS := bsp/startup_stm32f103xb.s
LDSCRIPT := bsp/STM32F103XB_FLASH.ld

INCS := -Ibsp/inc -Ihal/inc -Iapp/inc \
        -Imiddlewares/CMSIS/Core -Imiddlewares/CMSIS/Device \
        -I$(RTOS)/include -I$(RTOS)/portable/GCC/ARM_CM3

MCU    := -mcpu=cortex-m3 -mthumb
DEFS   := -DSTM32F103xB -DUSER_VECT_TAB_ADDRESS
CFLAGS := $(MCU) $(DEFS) $(INCS) -Os -g3 -std=gnu11 -Wall -Wextra \
          -ffunction-sections -fdata-sections -MMD -MP
LDFLAGS := $(MCU) -T$(LDSCRIPT) --specs=nano.specs \
           -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map -Wl,--print-memory-usage

OBJS := $(addprefix $(BUILD)/,$(SRCS:.c=.o) $(ASRCS:.s=.o))

all: $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin

$(BUILD)/$(TARGET).elf: $(OBJS)
>$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/%.hex: $(BUILD)/%.elf ; $(OBJCOPY) -O ihex $< $@
$(BUILD)/%.bin: $(BUILD)/%.elf ; $(OBJCOPY) -O binary $< $@

# Thư viện ngoài (FreeRTOS, CMSIS) không phải code nhóm -> tắt cảnh báo
$(BUILD)/middlewares/%.o: middlewares/%.c
>@mkdir -p $(dir $@)
>$(CC) $(CFLAGS) -w -c $< -o $@

$(BUILD)/bsp/system_stm32f1xx.o: bsp/system_stm32f1xx.c
>@mkdir -p $(dir $@)
>$(CC) $(CFLAGS) -w -c $< -o $@

$(BUILD)/%.o: %.c
>@mkdir -p $(dir $@)
>$(CC) $(CFLAGS) -Werror -c $< -o $@

$(BUILD)/%.o: %.s
>@mkdir -p $(dir $@)
>$(CC) $(MCU) -c $< -o $@

size: $(BUILD)/$(TARGET).elf
>$(SIZE) $<

flash: $(BUILD)/$(TARGET).bin
>st-flash --reset write $< 0x08000000

# Unit test chạy trên PC (gcc của máy), không cần kit
test:
>@mkdir -p $(BUILD)/host
>gcc -std=gnu11 -Wall -Wextra -Werror -Iapp/inc tests/test_app_logic.c app/src/app_logic.c -o $(BUILD)/host/test_app_logic
>./$(BUILD)/host/test_app_logic
>gcc -std=gnu11 -Wall -Wextra -Werror -Itests/fake -Iapp/inc -Ibsp/inc -Ihal/inc \
>    tests/test_flash_log.c tests/fake_w25q.c app/src/flash_log.c app/src/app_logic.c \
>    -o $(BUILD)/host/test_flash_log
>./$(BUILD)/host/test_flash_log
>python3 tools/test_datalogger.py

# Mô phỏng các màn hình OLED trên PC (dùng đúng code vẽ của firmware) -> build/sim/*.png
sim:
>@mkdir -p $(BUILD)/sim
>gcc -std=gnu11 -Wall -Wextra -Wno-unused-function -Itests/sim -Iapp/inc -Ibsp/inc -Ihal/inc \
>    tests/sim/oled_sim.c bsp/src/ssd1306.c bsp/src/font5x7.c app/src/flash_log.c \
>    app/src/app_logic.c tests/fake_w25q.c -lm -o $(BUILD)/sim/oled_sim
>./$(BUILD)/sim/oled_sim
>python3 tools/oled_render.py $(BUILD)/sim

# Firmware test phần cứng từng module (tests/hw/bN_*.c) -> build/hw/bN_*.bin
#   make hw                      : build tất cả
#   make hw-flash T=b3_i2c_scan  : build + nạp 1 bài
HW_TESTS := $(basename $(notdir $(wildcard tests/hw/b*.c)))
HW_LIB   := $(addprefix $(BUILD)/,$(filter-out main.o app/src/%,$(SRCS:.c=.o)) \
            $(patsubst %.c,%.o,$(wildcard app/src/app_logic.c)) tests/hw/hw_common.o $(ASRCS:.s=.o))

hw: $(foreach t,$(HW_TESTS),$(BUILD)/hw/$(t).bin)

$(BUILD)/hw/%.elf: $(HW_LIB) $(BUILD)/tests/hw/%.o
>@mkdir -p $(dir $@)
>$(CC) $^ $(filter-out -Wl$(COMMA)-Map=%,$(LDFLAGS)) -o $@

$(BUILD)/tests/hw/%.o: CFLAGS += -Itests/hw

hw-flash: $(BUILD)/hw/$(T).bin
>st-flash --reset write $< 0x08000000

clean:
>rm -rf $(BUILD)

.PHONY: all size flash test sim hw hw-flash clean
-include $(OBJS:.o=.d)

