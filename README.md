# LoRa Talk

**Text chat for the Flipper Zero over a REYAX RYLR998 LoRa module**

LoRa Talk turns a Flipper Zero and a RYLR998 (or another RYLRxx8 module with the same AT commands) into a long-range text messenger. You can chat with another Flipper running LoRa Talk, or with any other device using a REYAX module.

- Send to one address or broadcast to everyone (address 0)
- Received messages show the sender and signal strength, with vibration, LED and backlight alerts
- Chat history is saved to the SD card (`/ext/apps_data/loratalk/history.txt`)
- Edits every module setting: address, network ID, frequency, spreading factor, bandwidth, coding rate, preamble, TX power, password and UART baud rate
- Finds the module at any supported baud rate, so you can't lock yourself out

Built for the official firmware (tested against SDK 1.5.1). The default band is US 902–928 MHz.

## Wiring

| Flipper pin | RYLR998 pin |
|---|---|
| 9 (3V3) | 1 VDD |
| 13 (TX) | 3 RXD |
| 14 (RX) | 4 TXD |
| 18 (GND) | 5 GND |

You can leave NRST (pin 2) unconnected because it has an internal pull-up. While LoRa Talk runs, it takes over the GPIO UART, so the expansion-module service is paused until you exit.

## Using it

1. **Connecting:** on launch the app looks for the module and reads its settings. On first run, the module's own settings are adopted as-is.
2. **Chat:**
   - **OK** writes a message (up to 240 characters).
   - **Hold OK** sets the recipient (0 = everyone).
   - **Up/Down** scrolls; **Left/Right** pages.
   - Your messages are right-aligned. ` ...` means sending and ` (!)` means it failed; the reason shows in the header.
3. **Module Settings:** change values, then choose **Apply Changes**. Only the values you changed are written. Leaving with unapplied changes asks whether to apply or discard them.
4. **Module Info:** shows firmware version, UID, current settings, and the estimated airtime for a short and a full-length message.

To talk to each other, two modules need the same **network ID, frequency, spreading factor, bandwidth, coding rate, preamble and password**. Their addresses must differ.

"Sent" means the module transmitted the message. LoRa has no delivery receipts.

## Building

```sh
pipx install ufbt
ufbt update --channel=release   # official firmware SDK
ufbt                            # build dist/loratalk.fap
ufbt launch                     # build, install and run on a USB-connected Flipper
```

The protocol and config logic has unit tests that run on your PC:

```sh
make -C tests
```

To watch the module traffic (`>> AT...` / `<< +OK`), run `ufbt cli`, then `log debug` with the app running.

## Testing without a second Flipper

[`tools/arduino_peer/arduino_peer.ino`](tools/arduino_peer/arduino_peer.ino) runs a second RYLR998 on an Arduino as an **echo bot**: it sends every message it receives back to the sender as `echo: <text>`. It also lets you chat from the Serial Monitor (`/send <addr> <text>`) and pass raw AT commands through. An Arduino Mega 2560 is the easiest board because it has a spare hardware UART. Wiring, including the level shifting a 5V board needs, is in the sketch's header comment.

## Project layout

| Path | What |
|---|---|
| `lora/lora_proto.*` | AT reply parsing (`+RCV`, `+ERR`, query values). Pure C, unit tested |
| `lora/lora_config.*` | Settings model, validation rules, airtime estimate. Pure C, unit tested |
| `lora/lora_module.*` | UART driver and worker thread: commands, baud detection, connect/apply/send |
| `loratalk_history.*` | Chat history in RAM plus the SD card log |
| `loratalk_settings.*` | Settings file (`/ext/apps_data/loratalk/settings.conf`) |
| `views/chat_view.*` | The chat screen |
| `scenes/` | One file per screen |
| `docs/` | Module datasheet, AT command guide, [design notes](docs/DESIGN.md) |
