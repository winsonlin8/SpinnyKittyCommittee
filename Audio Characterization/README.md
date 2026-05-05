# Audio Characterization

This directory contains the Python/Jupyter script used to analyse the target
song and generate the pre-computed animation data embedded in the MSPM0
firmware.

## Script

**`spinny_kitty_spinning_testing.py`** — exported from Google Colab
([original notebook](https://colab.research.google.com/drive/1R49ilYCfoFYy3VuJ1tbd7pQvCooijN18)).

### What it does

1. **Loads** the source MP3 via `librosa`.
2. **Analyses** three audio features at 20 ms frame resolution:
   - Beat tracking (tempo, beat times)
   - Onset detection (drum hits and transients)
   - RMS energy (overall loudness envelope)
3. **Generates motor PWM data** — combines beat boosts, onset boosts, and a
   smoothed RMS envelope into a per-frame duty-cycle value (0–255).
4. **Generates LED animation data** — maps RMS energy to comet speed, onset
   detections to colour changes, and beat strength to brightness.
5. **Exports** two C header files:
   - `MSPM0/i2c_target/music_data.h` — `pwm_data[]` array (6 735 values)
   - `MSPM0/i2c_target/led_beat_params.h` — `led_beat_params[][4]` array (5 800 frames)

### Output format

Each row of `led_beat_params` contains:

| Index | Field       | Range  | Description                           |
|:------|:------------|:-------|:--------------------------------------|
| 0     | `pos`       | 0–34   | Comet head position on 35-pixel ring  |
| 1     | `bright`    | 0–255  | Peak pixel brightness                 |
| 2     | `tail_len`  | —      | Tail length (passed through; MSPM0 uses cubic falloff over full ring) |
| 3     | `color_idx` | 0–7    | Index into the 8-colour palette       |

### Running

The script is designed for **Google Colab** and expects the audio file to be
on Google Drive. To run locally, remove the `drive.mount` call and set
`AUDIO_FILE` to a local path. Dependencies:

```bash
pip install librosa matplotlib numpy pandas
```

Then run the script cell-by-cell in Jupyter, or as a plain Python script:

```bash
python spinny_kitty_spinning_testing.py
```

After running, copy the generated `.h` files into `MSPM0/i2c_target/` and
rebuild the CCS project.
