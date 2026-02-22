# librat

librat 是一个轻量级、嵌入式友好的 C/C++ 库。

## OpenOCD RTT 模式

librat 仅使用 SEGGER RTT 控制块（符号名：`_SEGGER_RTT`），OpenOCD RTT 可直接读取 COBS 帧数据。

协议握手（v0.2）：

- `rat_init()` 会先发送 runtime schema 控制帧（`HELLO -> SCHEMA_CHUNK -> SCHEMA_COMMIT`，控制包 ID=0x00）
- Host 侧 `rttd` 在 schema ready 前不会解码业务包
- schema hash 由固件对 `rat_gen.h` 内嵌 schema bytes 做 FNV-1a 计算

通道映射（无锁 SPSC）：

- Up[0]：`RatMain`，仅主循环写入
- Up[1]：`RatISR`，仅 ISR 写入

缓冲区大小可通过宏配置：

- `RAT_RTT_UP_MAIN_SIZE`
- `RAT_RTT_UP_ISR_SIZE`
- `RAT_RTT_DOWN_BUFFER_SIZE`

典型流程：

1) 通过 ELF 获取控制块地址与大小：

```bash
arm-none-eabi-nm -S build/stm32f4_rtt/stm32f4_rtt.elf | rg _SEGGER_RTT
```

1) 启动 OpenOCD RTT（建议缩短 polling 间隔）：

```bash
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg \
  -c "transport select swd" -c "adapter speed 8000" \
  -c "init" -c "reset run" \
  -c "rtt setup <addr> <size> \"SEGGER RTT\"" -c "rtt start" \
  -c "rtt polling_interval 1" \
  -c "resume" -c "rtt server start 19021 0"
```

Windows 可用脚本（自动解析 `_SEGGER_RTT` 地址并设置 polling）：

```
powershell -ExecutionPolicy Bypass -File tools/openocd_rtt_server.ps1
```

如需保留 gdb/telnet/tcl 端口：

```
powershell -ExecutionPolicy Bypass -File tools/openocd_rtt_server.ps1 -DisableDebugPorts:$false
```

1) Host 侧读取（默认端口 19021）：

```
rttd --config firmware/example/stm32f4_rtt/rat.toml
```

## J-Link RTT 模式

与 OpenOCD RTT 相同，流程是：

1) 先启动 J-Link RTT server
2) 再由 `rttd` attach 到 RTT TCP 端口（默认 `127.0.0.1:19021`）

Linux/macOS:

```bash
./tools/jlink_rtt_server.sh --device STM32F407ZG --if SWD --speed 4000 --rtt-port 19021
```

Windows:

```powershell
powershell -ExecutionPolicy Bypass -File tools/jlink_rtt_server.ps1 -Device STM32F407ZG -Interface SWD -Speed 4000 -RttTelnetPort 19021
```

Host 侧连接：

```bash
rttd --config firmware/example/stm32f4_rtt/rat.toml
```

常见失败排查：

- 目标板是否上电（VTref）
- SWDIO/SWCLK/GND 连线
- device 名是否与芯片匹配（如 `STM32F407ZG`）
- 端口是否被占用
