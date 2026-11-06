# Nhóm 14 – Trạm đo môi trường và ghi log (Data Logger)

STM32F103C8T6 (Blue Pill) + FreeRTOS. Đo nhiệt độ, độ ẩm (SHT30), ánh sáng (LDR),
gắn giờ từ DS3231, ghi log vòng vào Flash W25Q64, hiển thị OLED SSD1306,
cảnh báo bằng còi và LED, nhận lệnh từ máy tính qua UART.

## Build và nạp (Linux)
    make            # build/datalogger.bin
    make flash      # nạp qua ST-Link
    make hw-flash T=b3_i2c_scan   # nạp một bài test phần cứng (tests/hw/)

## Lệnh UART (115200 8N1)
    HELP | GET | STAT | PERIOD <ms> | TEMP <lo> <hi> | HUM <lo> <hi>
    TIME YYYY-MM-DD HH:MM:SS | LAST <n> | DUMP | LOG <từ> <đến> | ERASE

## Cấu trúc
    bsp/          board, pin map, driver module (SHT30, DS3231, SSD1306, W25Q64)
    hal/          driver ngoại vi STM32 (GPIO, UART, I2C, SPI, ADC, Timer)
    middlewares/  FreeRTOS V11.1.0, CMSIS
    app/          3 task (sensor, comm, display), IPC, log Flash
    tests/hw/     bài test từng module trên kit
    main.c        khởi tạo HAL, IPC, tạo task, chạy scheduler
