# ISO Recorder

Controls **ISO Recorder 3.5 or newer** from Bitfocus Companion / Stream Deck over OSC (UDP).

## Setup

1. In ISO Recorder open **Settings → Remote control**, tick **Listen for OSC commands (UDP)** and press OK. Default port: 8000.
2. Add this connection, enter the address of the computer running ISO Recorder (`127.0.0.1` for the same computer) and the OSC port.
3. The connection turns green when the program answers. The module asks for the status every 5 seconds; ISO Recorder answers to the
   address and port the request came from. If your firewall blocks it, set a fixed **Local UDP port** and allow it.

## Actions

- **Channel** REC / STOP / PAUSE / SPLIT / SELECT (channels 1–16). PAUSE also resumes.
- **Channel: REC if idle, STOP if recording** — one button for both.
- **Selected channels** REC / STOP / PAUSE / SPLIT — all selected channels at the same moment.
- **Selection** — select all, none, invert.
- **View** — 2x2, 3x3, 4x4, list.

## Feedbacks

Channel state (REC, PAUSED, PREVIEW, WAITING, RECONNECTING, ERROR, SAVING, OFF, NONE), channel is selected, at least one channel is recording, current view.

## Variables

`online`, `version`, `session`, `view`, `channels`, `rec_count`, `selected_count`, and for every channel 1–16:
`chN_name`, `chN_state`, `chN_time`, `chN_rec`, `chN_paused`, `chN_selected`, `chN_source`.

## Presets

Ready-made buttons for every channel (status tile, REC, STOP, PAUSE, SPLIT, SELECT), for the selected channels, the selection, the view and the status.

The OSC paths, the MIDI note map and more: `docs/Remote-Control.md` in the ISO Recorder package.
