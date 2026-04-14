# stm32g0b1re printf app

Simple Zephyr application for `nucleo_g0b1re` that prints messages with `printf()`
through the default UART console.

This app includes a board overlay that keeps the UART console and disables
other board-enabled peripherals that are not needed for this example. It also
enables one spare UART for simple TX/RX communication.

## UARTs

- Console: `USART2`, ST-LINK virtual COM port, `PA2/PA3`
- Spare UART: `USART1`, 115200 8N1
  - TX: `PC4`, Arduino `D1`
  - RX: `PC5`, Arduino `D0`

The application sends a loop-count message on the spare UART once per second.
Any byte received on the spare UART is printed on the console and echoed back
out on the spare UART.

If the spare UART receives the exact text `Alternate`, the application changes
`PC4` and `PC5` to GPIO inputs for 1 second. After that delay it reapplies the
`USART1` pinctrl default state and resumes UART communication.

Pressing Enter on the console UART performs the same pin switch for 10 seconds,
then restores `PC4` and `PC5` to the `USART1` alternate function.

## Build

```powershell
cd C:\Users\kangc\zephyrproject
west build -d build\stm32g0b1re_printf -p always -b nucleo_g0b1re apps/stm32g0b1re_printf
```

## Flash

```powershell
west flash -d build\stm32g0b1re_printf
```

## Output files

Build output is generated under `build\stm32g0b1re_printf`.

Typical files:

- `build\stm32g0b1re_printf\zephyr\zephyr.elf`
- `build\stm32g0b1re_printf\zephyr\zephyr.bin`
- `build\stm32g0b1re_printf\zephyr\zephyr.hex`
- `build\stm32g0b1re_printf\zephyr\zephyr.map`

The default console on this board is the ST-LINK virtual COM port (`USART2` on
`PA2/PA3`).

## Printf
PA0 raw=458, VREFINT raw=1508, VDDA=3292 mV, PA0=0.368 V
PA0 raw=4093, VREFINT raw=1505, VDDA=3299 mV, PA0=3.297 V
PA0 raw=4087, VREFINT raw=1505, VDDA=3299 mV, PA0=3.293 V
PA0 raw=4091, VREFINT raw=1505, VDDA=3299 mV, PA0=3.296 V
PA0 raw=4095, VREFINT raw=1505, VDDA=3299 mV, PA0=3.299 V
PA0 raw=4095, VREFINT raw=1505, VDDA=3299 mV, PA0=3.299 V
