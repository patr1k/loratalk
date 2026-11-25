# LoRa Talk design notes

## Threads

```
 UART RX DMA IRQ ──bytes──▶ stream buffer ──▶ LoRa worker thread ──LoRaEvent──▶ event queue ──▶ GUI thread
                                                ▲                                 (custom event
 GUI thread ──jobs (connect / apply / send)─────┘                                 LoRaTalkEventLoRa)
```

- **The worker thread owns the UART.** It assembles lines and sorts each one:
  - `+RCV=` becomes a Received event, even while a command is waiting for its reply.
  - Other lines are the reply to the pending command.
  - `+READY` and `+RESET` are ignored unless the worker is waiting for a reset to finish.
- **The GUI thread never blocks on the radio.** Jobs return their results as events.
- **All app state changes happen on the GUI thread**, in `loratalk_app_handle_lora_event`, before the current scene sees the event.

## Module protocol (see `RYLR998_AT_Commands.pdf`)

- Every command ends in CR LF. Wait for `+OK`, `+ERR=n` or `+KEY=value` before sending the next one.
- The `+RCV=<addr>,<len>,<data>,<rssi>,<snr>` payload can contain commas. The parser uses `<len>` to find where it ends, and falls back to the last two fields if that doesn't line up.
- `AT+NETWORKID?` replies with `+NETWORK=` (no "ID").
- A preamble other than 12 is only allowed with NETWORKID 18. The two settings are written in whichever order keeps the combination valid at every step.
- Highest SF per bandwidth: 9 at 125 kHz, 10 at 250 kHz, 11 at 500 kHz.
- A password can only be removed with `AT+RESET`. After a reset the settings are read back and re-applied.

## Where settings live

The module's flash memory is the source of truth for address, network ID, baud rate and (with `,M`) the frequency. The Flipper's `settings.conf` keeps a copy of the last known settings. On connect, it re-applies the ones that might not survive a power cycle:
- spreading factor, bandwidth, coding rate and preamble, and TX power (the AT guide doesn't say whether these persist),
- the password, which the module forgets on reset.

The first run adopts whatever the module has. Only changed values are written, to limit flash wear.

## Baud rate

- **Detection:** the last-used rate is tried first, then all 9 rates. A probe is `AT`. Getting `+ERR` back also proves the rate is right (the module's input buffer just had junk in it), so the probe retries once.
- **Changing it:** send `AT+IPR=x` and get the reply at the old rate, then switch the Flipper and probe. If that fails, reset at the old rate and listen at the new one; as a last resort, run full detection. The AT guide doesn't say when a new rate takes effect, so all three cases are handled.

## Unverified assumptions (check against real hardware)

| Assumption | Where it matters |
|---|---|
| Query replies (`AT+ADDRESS?`) come without a trailing `+OK` | A stray `+OK` would be taken as the next command's reply |
| `AT+SEND`'s `+OK` comes back within 2 s + airtime | Send timeout; `+ERR=17` (busy) is retried once |
| CR codes 1–4 mean 4/5–4/8 | Labels in the settings screen |
| `+RCV` has no spaces after commas | The parser tolerates spaces before numbers anyway |
