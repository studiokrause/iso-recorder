# ISO Recorder — Remote control (OSC, MIDI, Stream Deck)

Version 3.5. Control the recording from a lighting or show-control desk, a tablet (TouchOSC and similar), a MIDI
controller or a Stream Deck (through Bitfocus Companion). All three ways use the same commands, so they behave the same.

- [Quick start](#quick-start)
- [OSC](#osc) — commands, feedback, examples
- [MIDI](#midi) — note map, button lights
- [Stream Deck and Bitfocus Companion](#stream-deck-and-bitfocus-companion) — module, actions, feedbacks, variables, presets
- [Notes and limits](#notes-and-limits)

Channels are numbered from **1**, as in the program (1–16). Only the number matters, not the channel name.

## Quick start

1. Open **Settings → Remote control**.
2. Tick **Listen for OSC commands (UDP)** and note the port (default **8000**). For MIDI tick **Use MIDI** and choose the input
   (the controller's buttons) and, for button lights, the output.
3. Press **OK**. The event log shows `Remote control: OSC :8000, MIDI in`.

Nothing is switched on by default; the program does not open a network port until you tick the box.

## OSC

OSC 1.0 over **UDP**, messages and `#bundle`. Integer, float and string arguments are understood.
Default port **8000** (Settings). The program only listens on the port; send commands to `<IP of the computer>:8000`.

### Commands

| Path | Argument | Effect |
| --- | --- | --- |
| `/isorec/ch/N/rec` | – | Start recording channel N |
| `/isorec/ch/N/stop` | – | Stop channel N |
| `/isorec/ch/N/pause` | – | Pause channel N; the same command resumes a paused channel |
| `/isorec/ch/N/split` | – | Close the current file and start the next one |
| `/isorec/ch/N/select` | – | Toggle the selection of channel N |
| `/isorec/ch/N/select/on`, `/select/off` | – | Select / deselect channel N |
| `/isorec/selected/rec` `stop` `pause` `split` | – | The same, for **all selected channels** at the same moment |
| `/isorec/select/all` | – | Select all channels |
| `/isorec/select/none` | – | Clear the selection |
| `/isorec/select/invert` | – | Invert the selection |
| `/isorec/view/2x2` `3x3` `4x4` `list` | – | Switch the channel view |
| `/isorec/view` | `"2x2"`, `"3x3"`, `"4x4"`, `"list"` or `2`, `3`, `4`, `0` | Same, with an argument |
| `/isorec/status` | – | Send the full state now (also keeps the sender registered, see below) |

Rules:

- A command with the argument **0** is ignored, because many surfaces (TouchOSC, Lemur) send 1 when a button is pressed and 0 when it is released.
  A command with no argument, or with a non-zero one, runs.
- An unknown path, a channel that does not exist or damaged data is ignored; nothing else happens.
- `rec` on a channel that is already recording, `stop` on one that is not recording, etc. do nothing.
- `selected/…` acts on the channels selected at that moment, like the buttons of the program.

### Feedback (status)

The program sends the status **to the address and port the last command came from** (so the reply reaches you even through
NAT or when the controller uses a random port). The first message from a new sender is answered with the full state; after that only
the **changes** are sent (the time of a recording changes once a second).
A sender is remembered for 20 seconds after its last message: send `/isorec/status` every 5 seconds or so as a keepalive
(Bitfocus Companion does it for you).
Optionally Settings has **Also send status to** (address and port) — a fixed target that always gets the status (for example a tablet).
It can be combined with the reply, or the reply can be switched off.

| Path | Type | Value |
| --- | --- | --- |
| `/isorec/version` | string | Program version, e.g. `3.5` |
| `/isorec/session` | string | Session name (empty = unnamed) |
| `/isorec/view` | string | `2x2`, `3x3`, `4x4` or `list` |
| `/isorec/channels` | int | Number of channels |
| `/isorec/rec/count` | int | Number of recording channels |
| `/isorec/selected/count` | int | Number of selected channels |
| `/isorec/ch/N/name` | string | Channel name (empty when the channel does not exist) |
| `/isorec/ch/N/state` | string | `REC`, `PAUSED`, `PREVIEW`, `WAITING` (scheduler), `RECONNECTING`, `ERROR`, `SAVING`, `OFF` (input None), `NONE` (no such channel) |
| `/isorec/ch/N/rec` | int | 1 while recording (not while paused) |
| `/isorec/ch/N/paused` | int | 1 while paused |
| `/isorec/ch/N/selected` | int | 1 when selected |
| `/isorec/ch/N/time` | string | Recorded time `HH:MM:SS` (without pauses) |
| `/isorec/ch/N/source` | string | Short description of the source |

The state of all 16 channel numbers is always sent, so a surface can be built once for 16 channels.

### Examples

Test from a PowerShell prompt (any OSC tool works as well, for example `oscsend` or Protokol):

```powershell
function Send-Osc([string]$Path, [string]$Computer = '127.0.0.1', [int]$Port = 8000) {
    function Pad($text) { $b = [Text.Encoding]::ASCII.GetBytes($text) + [byte]0; while ($b.Length % 4) { $b += [byte]0 }; $b }
    $packet = [byte[]]((Pad $Path) + (Pad ','))      # address, then an empty type tag list
    $udp = New-Object Net.Sockets.UdpClient
    [void]$udp.Send($packet, $packet.Length, $Computer, $Port)
    $udp.Close()
}
Send-Osc '/isorec/ch/1/rec'
Send-Osc '/isorec/select/all'
Send-Osc '/isorec/selected/stop'
```

The repository contains a complete automatic test (`tests\osc_test.ps1`) that sends real UDP packets and reads the feedback.

## MIDI

Windows MIDI (winmm). Choose the **MIDI input** (commands) and, optionally, the **MIDI output** (lights of the buttons) in Settings.
**MIDI channel**: *Any* accepts every channel (and the lights use channel 1); 1–16 uses only that channel.

A press is a **Note On** (velocity above 0) or a **Control Change** (value above 0); the release (velocity / value 0) and Note Off are ignored.
Control changes are used **only when you assign them** (see MIDI learn below), so moving a fader of a controller whose buttons you did not map
cannot start a recording.

### MIDI learn

**Settings → Remote control → MIDI mapping** lists all 91 commands with the message assigned to each one.

1. Select a command in the list (or double-click it).
2. Press **Learn** and press a button (or turn a knob) on the controller. The list shows the message that was received, for example
   `Note 36 · channel 2` or `CC 41`. The program listens to the **MIDI input** chosen above, even before you press OK.
   Learning ends with the first message, after 15 seconds, or when you click the button again.
3. **Clear** removes the assignment of the selected command; **Reset all to default** restores the note map below.
4. Press **OK** to apply and save.

Rules: a message controls one command only (assigning it to a second command takes it away from the first); an assignment that stores a MIDI
channel works only on that channel, and wins over an assignment of the same number without a channel; the other assignments use the **MIDI channel**
setting (*Any* or 1–16). The lights (MIDI output) use the assigned message too: Note On for a note, Control Change for a CC, velocity / value 127 and 0.
That makes a controller that sends control changes from its buttons (Korg nanoKONTROL2 in its default mode) work without editing the controller.

### Default note map (before you learn anything)

| Notes | Command | Light (MIDI output) |
| --- | --- | --- |
| 0 – 15 | REC channel 1 – 16 | on while the channel is recording or paused |
| 16 – 31 | STOP channel 1 – 16 | on when the channel exists and is not recording |
| 32 – 47 | PAUSE channel 1 – 16 | on while the channel is paused |
| 48 – 63 | SPLIT channel 1 – 16 | always off |
| 64 – 79 | SELECT (toggle) channel 1 – 16 | on while the channel is selected |
| 80 | REC of the selected channels | on when a selected channel is recording |
| 81 | STOP of the selected channels | on when channels are selected and none of them records |
| 82 | PAUSE of the selected channels | on when a selected channel is paused |
| 83 | SPLIT of the selected channels | off |
| 84 | Select all | on when every channel is selected |
| 85 | Invert the selection | off |
| 86 | Select none | on when nothing is selected |
| 87 / 88 / 89 / 90 | View 2×2 / 3×3 / 4×4 / list | on for the current view |

The lights are sent as Note On, velocity 127 (on) or 0 (off), only when they change, and all of them once after the settings are applied.
Pad controllers (Akai APC/MPK, Novation Launchpad, Behringer X-Touch in note mode, …) fit this map as it is; with other controllers use MIDI learn.

## Stream Deck and Bitfocus Companion

The Stream Deck is driven by **Bitfocus Companion** (v5.x). ISO Recorder comes with a Companion **module** that sends the OSC commands above, listens
for the status and gives you actions, feedbacks, variables and ready-made button presets.
(Elgato's own software cannot talk OSC; use Companion, which drives the Stream Deck directly.)

### Install the module

The module is in the release as `isorecorder-3.5.0.tgz` (and in the program folder, `companion\`).

1. Companion → **Modules** → *Import module package* → choose `isorecorder-3.5.0.tgz`.
   (Alternatively unpack the folder `companion\isorecorder` into Companion's *developer modules* folder.)
2. **Connections** → *Add connection* → **Studio Krause: ISO Recorder**.
3. Set **ISO Recorder address** (`127.0.0.1` when Companion runs on the same computer) and the **OSC port** (as in Settings, 8000).
4. The connection turns green once the program answers. If it stays red: check that **Listen for OSC commands** is ticked in the
   program, the port is the same, and the firewall allows UDP (use the **Local UDP port** option to give the status a fixed port to allow).

The module needs Node.js 22 or 26, which Companion includes.

### Actions

| Action | Options |
| --- | --- |
| Channel: REC / STOP / PAUSE / SPLIT / SELECT | channel 1–16; command (REC, STOP, PAUSE, SPLIT, SELECT toggle, Select, Deselect) |
| Channel: REC if idle, STOP if recording | channel |
| Selected channels: REC / STOP / PAUSE / SPLIT | command |
| Selection: all / none / invert | selection |
| View: 2x2 / 3x3 / 4x4 / list | view |

### Feedbacks (button colours)

| Feedback | True when |
| --- | --- |
| Channel state | the channel is in the chosen state (REC, PAUSED, PREVIEW, WAITING, RECONNECTING, ERROR, SAVING, OFF, NONE) |
| Channel is selected | the channel is selected |
| At least one channel is recording | `rec_count` > 0 |
| Current view | the chosen view is shown |

### Variables

Use them in button text as `$(connection-label:variable)`, for example `$(isorec:ch1_time)`.

| Variable | Value |
| --- | --- |
| `online` | 1 when the program answers |
| `version`, `session`, `view`, `channels`, `rec_count`, `selected_count` | as in the OSC feedback |
| `ch1_name` … `ch16_name` | channel name |
| `ch1_state` … `ch16_state` | `REC`, `PAUSED`, `PREVIEW`, … |
| `ch1_time` … `ch16_time` | recording time `HH:MM:SS` |
| `ch1_rec`, `ch1_paused`, `ch1_selected` … | 1 / 0 |
| `ch1_source` … `ch16_source` | source description |

### Presets (ready-made buttons)

Drag them from the **Presets** tab of the connection onto the Stream Deck page:

- **Channels 1–4, 5–8, 9–12, 13–16** — for every channel: a *status tile* (name and time; red while recording, amber when paused,
  brown when reconnecting, dark red on error; a press starts or stops the channel), **REC**, **STOP**, **PAUSE**, **SPLIT**
  (REC and PAUSE light up) and **SELECT** (green when selected).
- **Selected channels** — REC / STOP / PAUSE / SPLIT of the selected channels; **SELECT ALL**, **INVERT SELECTION**, **SELECT NONE**, and a counter of the selected channels.
- **View** — 2×2, 3×3, 4×4, LIST (the current view is green).
- **Status** — number of recording channels (`REC 3 / 8`, red when something records) and the program version.

A typical 15-key Stream Deck page: row of tiles for channels 1–5, row of **SELECT** for them, and a row with
SELECT ALL / INVERT / REC / PAUSE / STOP of the selected channels.

## Notes and limits

- Commands act exactly like the buttons in the window: recording of several selected channels starts, pauses and stops at the same moment.
  A channel cannot be started if its recording folder is not writable — the program shows the usual message and the state stays `PREVIEW`.
- The status is refreshed 4 times a second; a recording time changes once a second.
- Only the first 16 channels exist (the program limit).
- **Security:** OSC has no password. Anyone who can reach the UDP port can start and stop recording. Use it on a trusted network
  or bind Companion and the program to the same computer (`127.0.0.1`); do not open the port to the internet.
- **Tested** with real UDP packets against the program (`tests\osc_test.ps1`, `companion\isorecorder\test`) and with the module's code
  against the program. **Not tested** in a real Companion installation, on a physical Stream Deck or with a physical MIDI controller
  (the MIDI map and the lights are checked by the program's self-test only). Please report anything that does not work as described.
