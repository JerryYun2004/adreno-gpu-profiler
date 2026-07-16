# Width Sequence Patch Notes

## Purpose

Adds an internal width-sequence mode to `streamer_sweeper` so a single counter-recording window can contain multiple fused-softmax widths separated by user-selected CPU-side delays.

## New options

- `--widths <csv-list>`: comma-separated list of softmax widths to run inside each counter chunk, for example `128,256,512,1024,2048`.
- `--width-sleep <seconds>`: CPU-side sleep inserted between width steps.

## Recommended command

```bash
adb shell "su -c \"/data/local/tmp/streamer_sweeper \
  --time 4 \
  --widths 128,256,512,1024,2048 \
  --width-sleep 0.1 \
  --bench-args '--rows 128 --repeats 16 --csv'\""
```

This records one CSV per counter chunk with this internal workload sequence:

```text
width=128
sleep
width=256
sleep
width=512
sleep
width=1024
sleep
width=2048
```

## Implementation notes

- Width-sequence mode is separate from repeated identical benchmark burst mode.
- If `--widths` is provided, the width list defines the sequence and `--bursts` is ignored.
- In normal fused-softmax mode, `--bench-args` should omit `--width`; the sweeper appends `--width <value>` for each width step.
- For custom benchmark commands, `{width}` can be used as a placeholder. If `{width}` is absent, the sweeper appends `--width <value>` to the command.
- `run_config.txt`, chunk metadata, and benchmark logs now record width-sequence configuration.

## Validation performed here

- Ran local host-side C syntax check with `gcc -fsyntax-only -I/mnt/data streamer_sweeper.c -Wall -Wextra`.
- Android NDK build and phone-side execution were not run in this sandbox; build/push should be done on your Mac using `build_and_push.sh`.
