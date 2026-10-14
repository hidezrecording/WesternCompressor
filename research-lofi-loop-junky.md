# Research note — Z.Vex Lo-Fi Loop Junky warble (for the Hotdog "warble" switch)

Researched 2026-10-14 for Nathan's request: a switchable effect on the
Hotdog faceplate mimicking the original Lo-Fi Loop Junky's playback
warble — his words: "a garbled pitch delay like an old tape", wow and
flutter. All findings below are paraphrased from the sources listed;
no bulk text is reproduced.

## What the original pedal is

- The Lo-Fi Loop Junky (Z.Vex, early 2000s) is an analog phrase
  looper: it records up to **20 seconds** of audio into non-volatile
  analog storage cells and loops it. Z.Vex's own feature copy stresses
  "real clocked-analog recording with no analog-to-digital
  conversion", built-in compression on the record path, true bypass,
  and a loop that is retained even with the battery removed.
- Five knobs: Volume, Tone (rolls off hiss/artifacts on the loop),
  Record level (the storage cells can be overdriven), **Vibrato
  Depth** and **Vibrato Speed**.
- Published fidelity limit, repeated identically across Z.Vex's own
  copy and every period dealer listing: **"very limited frequency
  response — nothing above 2.6 kHz — brick-wall filtering."** Plus
  hiss, aliasing artifacts, distortion and "out-of-tune effects" as
  advertised features. The direct (dry) guitar path is a separate,
  clean preamp; the lo-fi character exists only on the loop playback.
  (This maps neatly onto Hotdog's brief: the warble is a switchable
  insert that leaves the limiter path alone when off.)

Sources:
- Z.Vex feature list as archived by Loopers' Delight:
  https://loopersdelight.com/tools/loopjunky/loopjunky.html
- Analog Man's product page (same manufacturer copy):
  https://www.buyanalogman.com/ZVex_Lo_Fi_Loop_Junky_p/zvex-lo-fi-loop-junky.htm
- Sound Shoppe listing incl. control descriptions:
  https://soundshoppe.nyc/products/lo-fi-loop-junky-hand-painted
- Tape Op review (Larry Crane): confirms "won't play any sound over
  2.6 kHz", "crude vibrato", warped-78 feel:
  https://tapeop.com/reviews/gear/35/lo-fi-loop-junky-stompbox

## The chip underneath — and why 2.6 kHz / 20 s is a fingerprint

The pedal's two hard numbers — 20 s maximum loop and a 2.6 kHz
passband — match the ISD1400-series "ChipCorder" analog
record/playback ICs exactly. The ISD1420 datasheet headline specs are:
**duration 20 s, sampling frequency 6.4 kHz, filter pass band
2.6 kHz**. These chips store audio as analog charge levels in EEPROM
cells (hence "no A/D conversion" in the marketing) and are clocked by
an internal oscillator whose frequency is set by an external
resistor (ROSC) — sample rate scales directly with that clock
(illustrated by the sibling ISD1820's ROSC table: 100 kΩ → 6.4 kHz
sampling / 2.6 kHz bandwidth; 80 kΩ → 8.0 kHz / 3.4 kHz; 200 kΩ →
3.2 kHz / 1.3 kHz).

Sources:
- ISD1420P distributor spec summary (sampling 6.4 kHz, pass band
  2.6 kHz, 20 s): https://www.infinity-semiconductor.com (part page
  for ISD1420P, Nuvoton) — same figures echoed at micro-processor.com
  and micro-semiconductor.com listings.
- ISD1820 module guide with the ROSC → sample-rate/bandwidth table:
  https://www.phippselectronics.com/support/isd1820-voice-recorder-module-user-guide/

Caveat: Z.Vex has never published the chip part number; the ISD1420
identification is an inference from the exact spec match (20 s /
6.4 kHz-class clock / 2.6 kHz passband), widely repeated in the DIY
community. Treat it as "matches the ISD1420 spec sheet on every
published number", not as a factory-confirmed BOM line.

## What makes the warble

- The vibrato acts on **playback only**: the storage chip's playback
  clock is wobbled by an LFO (the Vibrato Speed / Depth controls).
  Because pitch in this architecture *is* clock rate, modulating the
  clock modulates pitch directly and proportionally — there is no
  separate pitch shifter. It also wobbles the chip's effective
  bandwidth and aliasing behaviour along with the pitch, which is
  part of why the result sounds "garbled" rather than like a tidy
  chorus/vibrato.
- Character, from Z.Vex's own copy: speed runs from "very slow
  vibrato for warped-record effects" up to "very fast for jiggly
  playback"; depth is described by dealers as "the level of pitch
  twisting". Tape Op's reviewer used vibrato around 3 o'clock on both
  controls and describes a warped-record result. No Hz or cents
  figures were ever published by Z.Vex — the speed/depth ranges are
  known only qualitatively (JUDGMENT CALL territory, see below).
- Because the underlying clock is an RC oscillator and the storage is
  analog, the modulation in the real pedal is not a surgically clean
  LFO: there is inherent drift and irregularity on top of the periodic
  wobble. This is the "tired tape" quality Nathan asked for — a clean
  sine chorus LFO alone will not read as the same effect.

## The Instant Lo-Fi Junky (for contrast — NOT what Nathan asked for)

The Instant Lo-Fi Junky (2011, designed by Joel Korte) recreates the
Loop Junky's playback texture **in real time** on the live signal:
a THAT-chip compressor, a bucket-brigade (BBD) delay line as the
vibrato medium, and low-current op-amp filters. Its manual states the
traditional Loop Junky simulation setting is the **triangle**
modulation waveform, with Speed and Depth affecting only the
warbled path, and a Tone control that only darkens the warbled path.
Reviewers describe the result as like a stretched cassette tape.
The BBD approach matters for Hotdog: it proves the accepted realtime
equivalent of "wobble the ChipCorder clock" is "read a short delay
line with a modulated read position", which is exactly the
architecture implemented here.

Sources:
- Instant Lo-Fi Junky manual (Z.Vex, via zZounds):
  https://cf3.zzounds.com/media/Product_Manual-fbe44258600303e12eced712bc3fea04.pdf
- MusicRadar review of the Instant Lo-Fi Junky:
  https://www.musicradar.com/reviews/guitars/z-vex-instant-lo-fi-junkie-532288

## Numbers carried into the Hotdog implementation

| Choice | Value | Basis |
|---|---|---|
| Wet-path bandwidth | low-pass ≈ 2.6 kHz (steep), high-pass ≈ 100 Hz | Published pedal limit (2.6 kHz, "brick-wall"); low end: ChipCorder input coupling territory (judgment call) |
| Base delay | 12 ms | BBD realtime-equivalent architecture (Instant Junky); a pure transport lag like tape/BBD, not an echo |
| Wow | 0.6 Hz sine, ±1.8 ms | "Very slow… warped-record" end of the pedal's speed range; tape wow band |
| Main vibrato | 4.9 Hz triangle, ±0.55 ms | Triangle = the manual's stated traditional waveform for the emulation; mid-speed "jiggly" region |
| Flutter | 9.3 Hz sine, ±0.12 ms | Tape flutter band; small — the chip's warble is wow-dominant |
| Random drift | stepped random targets, smoothed (τ ≈ 0.35 s), ±1.1 ms | RC-clock / analog-storage irregularity; keeps the wobble from ever repeating like a clean LFO |
| Resulting pitch swing | ≈ ±2 % typical, ≈ ±3 % worst-case (≈ ±35–50 cents) | Sum of dD/dt across components; "out-of-tune effects" without seasick parody |

JUDGMENT CALLS (research was thin — flagged for Nathan):
1. Z.Vex never published vibrato depth in cents or speed in Hz for
   the Loop Junky. The component depths/rates above are chosen from
   the qualitative descriptions ("warped record" slow end, "jiggly"
   fast end, "pitch twisting" depth) and tape-machine conventions.
2. The low-frequency cutoff (~100 Hz high-pass) is inferred from the
   ChipCorder family's input-coupling practice, not from a published
   Loop Junky figure (only the 2.6 kHz top is published).
3. The ISD1420 chip identification is a spec-match inference, as
   noted above.
4. Aliasing/hiss: the original's grit partly comes from its 6.4 kHz
   sampling aliasing. Hotdog's warble path runs at full host rate and
   does NOT simulate aliasing or hiss — only the pitch garble and the
   bandwidth limit, per Nathan's description ("garbled pitch delay").
   Adding aliasing grit later would be a deliberate extra, not a fix.
