# Western Compressor

A one-knob stereo parallel FET limiter for macOS/Windows
(VST3 / AU / Standalone), built by Sabatino Audio. Internal project
name: OSC-72 (code identifiers unchanged); project directory:
western-compressor.

**Name:** renamed **Western Compressor** by Nathan on 2026-10-14 —
the official product name. It shipped as **Hotdog** (v1.0.0) earlier
the same day; Nathan had renamed it Hotdog on 2026-10-13, and it
began under the working title OSC-72. The Hotdog name nodded to the
prototype's studio nickname — the band's joke about the unit was
the hot-dog one: big thing in one end, little sausage out the other.

## GUI v18 — hardware grown ~x1.75, gloss black with scratched edges, sharp corners (352x253)

Nathan's direction (2026-10-14, superseding v15): the
**double perimeter stays deleted** (v15) — the window is a
wide rack unit whose face is just **two plates in one
plane**: the **Bitey compressor module** at left **butts
flush against the utility strip** at right, sharing one
seam and one outer perimeter. v16 changes five things.
(1) The **module's face is clean** — every scratch, scuff
and ageing mark removed; it draws no weathering at all.
(2) **One material:** the module's black is now the
strip's own brushed black (same gradient, same vertical
brush grain) — one brushed black metal on both plates.
(3) **The lettering changes again:** Michroma "still
looks like real estate font, especially that B" — every
string on module and strip is now set in **Saira Stencil
One** (SIL OFL 1.1, The Saira Stencil Project Authors), a
true stencil face: the lettering of equipment plates,
shipping crates and military stencil kits — "way more
industrial and less serious, more utilitarian", stamped
rather than drawn, and its B is a stamped B. Chosen from a
7-candidate sheet rendered with the actual panel strings
(Allerta Stencil too thin at small sizes; Stardos Stencil
is a serif stencil — vintage-print register; Barlow Semi
Condensed SemiBold the runner-up, an honest industrial
grotesque, but drawn rather than stamped). And **all
lettering is pure white** — #FFFFFF at full opacity,
flange numerals and the fixed index triangle included
(Nathan: the brightest possible white). (4) The window
shrinks again, to **640x460** — regular-plugin size.
(5) **The strip aligns to the module:** the 'B' drops so
its centre sits on the magic eye's centre line, and the
POWER jewel drops so it sits on the GAIN REDUCTION text's
line. The centre screws remain **Allen hex-socket** heads;
the knob's is **black** (the eye's hub screw is unchanged).
(The Oswald, Limelight, Titan One and Michroma TTFs stay
in `juce/Assets/` but are no longer embedded.)

**v17 (2026-10-14): size only.** Nathan: the unit still
read as a giant full-screen object — "it's a plug-in, it
should be smaller, make it half the size; I can't even
really tell yet till it's half the size". The window is
now exactly half of v16: **320x230**. Nothing else
changes — same design, colours, Saira Stencil lettering,
alignments ('B' on the eye's centre line, POWER jewel on
the GAIN REDUCTION line), gold toggles, section jewels,
black knob screw, clean single-material plates, eye at
rest in the stills, knob at 7. Implementation: the panel
paint stays authored in the v16 640x460 space and is
drawn under a 0.5 scale transform; component bounds and
repaint rects are re-based at half scale; the raster
constants that were absolute at the old size (knob rim
inset/shadow, O/P knob stroke widths) are now
size-proportional. The one deviation: the smallest
labels sit a touch above strict 0.5 scale for legibility
(GAIN REDUCTION 13 virtual pt, function words and flange
numerals 10.5, IN/OUT and L/R 10 — full list in
`docs/build-report-v17.md`). All dimensions quoted in
the bullets below are the v16 (virtual) ones; on screen
everything is exactly half. The review stills render at
2x (640x460 PNGs), and `docs/gui-v17-context.png` shows
the A render downscaled to true native 320x230 centred
on a 1440x900 desktop canvas, so the real on-screen size
can be judged.

**v18 (2026-10-14): sizing, finish, edges, lettering.**
Nathan: "the size of the overall plugin could be about
10% larger, but the size of the knobs, the magic eye,
the output level pots, the switches and the lights and
all their markings — all of that can be about one and a
half times the size they are now, if not two times". The
window grows ~10% to **352x253** (the paint transform
becomes 0.55 over the same 640x460 authoring space) while
the hardware grows ~x1.5–2 in a full re-layout: eye
70→110 px (virtual), flange R 60→95, toggles back to
their full artwork size, all jewels at full POWER scale,
every string up ~x1.6–1.75 (caption 20 pt, function
words 16.5, IN/OUT 16, numerals 17, the 'B' 52 — virtual
points). The hardware now dominates; the empty panel
field shrinks. The alignment lines survive: 'B' on the
eye's centre line, POWER jewel on the GAIN REDUCTION
line. **Edges:** every panel/frame corner is a **sharp
90° angle** — outer rim, plate edges, toggle escutcheons;
only hardware stays round. **Finish:** the brushed metal
is killed ("still looking a little too nice/fancy,
high-end") — both plates are smooth **gloss black** with
one soft sheen raking across the unit, and the wear
moves to the **edges and corners only** (worn rim runs,
edge-hugging scratches, corner chips; the faces stay
clean gloss). **Lettering:** Saira Stencil One is "better
but still a little bit too nice, especially the B" —
replaced by **Special Elite** (Apache License 2.0, Brian
J. Bonislawsky DBA Astigmatic), genuinely eroded,
ink-starved utilitarian glyphs; the B is the test letter
and its B is rough, not designed (Stardos Stencil Bold
was the runner-up: army-stencil name, but its Didone
serifs read as refined vintage print). Pure white
stays. A third still, `docs/gui-v18-c.png`, renders the
**black-pot variant** (snapshot `black` token): state A
with the main knob + flange in the panels' gloss black,
for Nathan's side-by-side. (The Oswald, Limelight, Titan
One, Michroma and Saira Stencil One TTFs stay in
`juce/Assets/` but are no longer embedded.)

**v19 (2026-10-14): the detail round + a real power
switch.** Nathan likes v18 "a lot more"; his fixes:
(1) the flange **numerals shrink onto the flange** (ring
R 88→87, face 17→15.5 pt) — every numeral now sits fully
on the annulus; (2) the **LIMIT/MOD clusters move up and
outward** off the bottom edge (centres y 428→410, LIMIT
x 96→86, MOD x 344→354); (3) **one label size panel-wide**
— 16.5 pt, the LIMIT/MOD size: GAIN REDUCTION comes down
from 20 to join POWER/OUTPUT; (4) **lettering changes
again** — Special Elite was "in the right direction" but
the typewriter face isn't loved — to **Rubik Dirt**
(Rubik Filtered Project, SIL OFL 1.1): the same
eaten-away erosion in a heavy stamped slab, industrial
not typed (Black Ops One was the runner-up — squared
military but vector-clean, the "too nice" failure again;
Special Elite TTF stays in Assets/, unembedded);
(5) the **knob ships gloss black** (the v18 C-variant
becomes the product; gold/yellow stay in the variant
mechanism) and the **levers go silver** (steel/chrome
shading, dark escutcheons unchanged); (6) the **big 'B'
is gone** — small **BITEY wordmarks** at label size sit
top-left on each plate; (7) the **GAIN REDUCTION caption
moves above the eye**, and the main pot gains an
**INPUT** label above its index triangle; the strip
caption over the output knobs is just **OUTPUT** now;
(8) the strip gains a **full POWER switch cluster**
matching LIMIT/MOD part for part (word above, silver
lever, ON/OFF labels, amber jewel on the cluster's line)
bound to a new **`power` parameter** (default ON): off =
audio fully bypassed in the processor (dry passthrough
through a ~25 ms click-free fade, latency reported 0
even if MOD is in, O/P trims bypassed too) and **every
light out** — POWER jewel, section jewels and a dead,
dark magic eye; LIMIT/MOD still flip but engage nothing
until power returns; (9) the strip's screws become
**four**, one per corner, like the module's. A third
still, `docs/gui-v19-off.png`, shows the power-off
state. Full notes in `docs/build-report-v19.md`.

**v20 (2026-10-14): three refinements.** Nathan likes
v19 overall. (1) **INPUT drops down** to sit almost
right on top of the index triangle (baseline 180→188 —
a few px of air above the arrow, not touching).
(2) **Lettering: same worn style, sharper rendering.**
Rubik Dirt's direction is right but it reads "a little
too blurry" — the face returns to **Saira Stencil One**
(v16's pick), now carried with a **light procedural
worn treatment** in the editor's text rendering: each
string (and each flange numeral) is rendered once into
a cached offscreen image and a sparse deterministic
speckle of pinholes is knocked out of the glyphs — the
worn-stencil read with hard edges, thinner lines than
Rubik Dirt's slab, and the pure #FFFFFF left bright
because no grey erosion smears the strokes. Chosen from
a sheet at true render size (Allerta Stencil the
runner-up: thinner and sharp, but its rounded forms
lean "designed"; Big Shoulders Stencil stays
hairline-condensed at any usable weight; Rubik Dirt
TTF stays in Assets/, unembedded). (3) **The magic
eye's ring becomes hardware:** the clean silk-white
ring "looks almost like a graphic" next to the real
hardware — it is replaced by an **aged gold retaining
bezel** matching the jewel bezels' gold tone: a brass
gradient raked from the upper left, its highlight
broken into uneven arc runs, fine scratches worried
into the band and small chips on the rim, all
deterministic. The glass, green, hub and dead-tube
off state are unchanged. Full notes in
`docs/build-report-v20.md`.

**v21 (2026-10-14): the Western round.** Nathan supplies
a "WESTERN GRANDE" specimen — a vintage Western poster
face — and asks to try that font "with a little bit of
that spray paint effect, very subtle". His v20
complaints drive the rest: "all the fonts have gotten
too big and the letters are too spaced apart and they're
all overlapping each other and the other labels and
overlapping some of the screws", and the flange numerals
"have gotten too dim — they should go back to being
white, and even a little bit smaller". So: (1) the face
is **Smokum** (Brian J. Bonislawsky DBA Astigmatic
(AOETI), Apache License 2.0) — tall bracketed slab
lettering, the specimen's own family, sturdy and plain
at heart and narrow enough to cure the crowding; chosen
from a sheet with the actual panel strings at the new
sizes (Sancreek chunkier and more ornate; Rye a
decorated Tuscan; Ewert an outline circus face, mush
small; Frijole a rough painted brush face; Ribeye a soft
storybook serif). (2) The worn-pinhole treatment becomes
a **very subtle spray-paint pass**: glyphs stay crisp
pure #FFFFFF, a faint overspray halo feathers their
edges and a sparse mist speckle settles around them —
nothing knocked out of the glyphs any more. (3) **Every
string shrinks by about a third** (labels 16.5 → 11 pt,
markings 16 → 10.5, flange numerals 15.5 → 10.5) and
**tracking tightens to near-natural** (0.6 labels /
0.3 markings). (4) A full **overlap audit** — every
string against every screw, escutcheon, jewel, the eye
bezel, the knob/flange and every other string — passes
with clear air everywhere; the v20 BITEY-vs-GAIN
REDUCTION crowding is gone (the caption now spans
roughly x 181–259, the wordmark ends around x 60).
Everything else is v20: window 352x253, gloss black +
edge wear, gold eye bezel, silver levers, black knob,
POWER cluster and behaviour, eye at rest in the stills,
knob at 7. Full notes in `docs/build-report-v21.md`.

**v22 (2026-10-14): the spray comes off.** Nathan's
verdict on v21: "completely get rid of the spray paint
effect... make it just very clean bright white
lettering", shrink the font "and make the font more
stretched... stretch it out a tiny bit", the flange
numerals clear and visible with no boxes around them,
the BITEY wordmarks clear of the screws, and the power
strip's screws shrunk to the compressor module's size.
So: (1) the **whole spray apparatus is deleted** — the
cached string images, the overspray halo + speckle, and
the numeral underlay images whose grey boxes ringed the
flange numerals and spilled past the knob's edge are
gone from the code entirely; **every string and numeral
is direct vector text in pure #FFFFFF** again. (2) The
face stays **Smokum**, set a step **smaller** (labels
11 → 10 pt, markings 10.5 → 9.5) and **stretched** —
`capsFont` now applies a 1.2x horizontal scale, so the
lettering reads wider at the smaller size. (3) The
flange numerals go the other way: **10.5 → 16 pt**,
measured ink r 82.2–91.3 on the 80–95 annulus — fully
on the flange, clearly visible, peak brightness 255.
(4) Both **BITEY wordmarks move out** (anchor x 34 →
40): measured ink starts ~7.6 device px clear of the
corner-screw heads. (5) The **strip's four corner
screws shrink from r 10 to r 9** — the module screws'
exact size. (6) The **zero-overlap audit is re-run**
on the render: no pair of string/screw/plate/jewel/
string under 2 device px anywhere (panel minimum:
IN on the MOD side → the flange rim, 3.7 device px;
the wordmarks sit ~7.6 device px clear of the screw
heads). Everything
else is v21. Full notes in `docs/build-report-v22.md`.

**v23 (2026-10-14): bolder type.** Nathan's verdict on
v22: the flange numerals "are a better size height-wise
but they should be stretched", and "all the rest of the
font should be about 30% bigger now... wider stretchier
... and bolder lines so I can see the middle of the
letters — they're not readable as they are". The root
cause is Smokum itself: a thin-stroked face whose
hairlines anti-alias into grey mush at panel sizes. So:
(1) the face becomes **Sancreek** (The Sancreek Project
Authors, SIL OFL 1.1) — the chunkier Western slab from
the v21 candidate sheet, roughly double Smokum's
stroke weight; at the new larger size its bracket
spurs read as sturdy plate lettering. The alternative,
Smokum with offset-pass synthetic emboldening, was
built and measured: thicker on paper but uneven and
double-struck — rejected. (2) **Sizes up ~30%**:
labels 10 → 13 pt, markings 9.5 → 12.5, and the
**stretch widens 1.2x → 1.5x**. (3) The flange
numerals **keep v22's 16 pt height** (measured ink
height 8.8 virtual px against v22's 8.7) in the bolder,
wider face — the '0' carries 65 ink px against v22's
35 — ink r 82.5..91.3 on the annulus. (4) A
**legibility gate** is run on the true-size render and
2x crops: every panel string reads with solid white
letter centres (measured solid-core fraction of label
ink 56–66%, against plain Smokum's 43–52% at the same
sizes). (5) The **zero-overlap audit is re-run** at
the new sizes and passes (tightest facing pair:
OUTPUT → the O/P dot rings, 3.2 device px). Clean
direct vector text only — the v22 deletions stay
deleted. Everything else is v22. Full notes in
`docs/build-report-v23.md`.

**v24 (2026-10-14): Nathan's font pick, plus wordmark
and placement changes.** Sancreek is rejected ("i don't
like that font"). A specimen sheet set the actual panel
strings in four bold faces (Ultra, Alfa Slab One,
Bevan, Smokum emboldened); Nathan picked **Smokum
Emboldened** — the v21/v22 Western face with its
outlines expanded by a FreeType stroker at 3.5% of em,
baked offline into a derived font file
(`juce/Assets/SmokumEmboldened-Regular.ttf`; Apache
2.0, see `NOTE-SmokumEmboldened.txt`) — at v23's exact
metrics (labels 13 pt, markings 12.5 pt, stretch 1.5x,
tracking 0.6/0.4; numerals 16 pt on the same annulus).
Also: (1) the module's top-left wordmark becomes
**WESTERN COMPRESSOR** (the strip keeps **BITEY**);
(2) both wordmarks **rise onto their top-left screw's
centreline** (ink centre on y 18); (3) the magic eye
**nudges up 5 virtual px**; (4) the **GAIN REDUCTION
caption moves back under the eye**, reversing v19 —
the stack reads eye / caption / INPUT / arrow / knob.
Legibility gate and zero-overlap audit re-run and
passing. Everything else is v23. Full notes in
`docs/build-report-v24.md`.

**v25 (2026-10-14): fine-tuning — "not bad… it's
close."** (1) The lettering goes **a little bigger**
(labels 13 → 14.5 pt, markings 12.5 → 14 pt) and **a
little thinner**: the panel face is now **Smokum
Emboldened Thin**, the same bake at a reduced stroker
radius (48 units = 2.34% of em;
`juce/Assets/SmokumEmboldenedThin-Regular.ttf`, see
`NOTE-SmokumEmboldenedThin.txt`). The flange numerals
keep the v24 bake. (2) **Spacing choreography**: the
magic eye rises for real (top 43 → 22 virtual) while
the POWER cluster drops a hair (+4) — the GAIN
REDUCTION caption's bottom meets POWER's line in the
middle (measured delta 4.5 virtual px) — and the
input stack drops a little (knob centre 300 → 310),
opening clear space between the caption and INPUT.
(3) The **flange widens** (R 95 → 100) and the
numerals grow a hair taller (16 → 17.5 pt), stretch a
bit more (1.65x) and ride the wider annulus (ring
R 90). (4) The **O/P pots spread** (centres
505/575 → 496/584) and **grow** (23 → 27 device px —
drawing and hit area together), their dot rings grow
with them (R 27 → 30), the OUTPUT caption rises
(348 → 342) and L/R tuck up under their knobs
(436 → 424). Legibility gate and zero-overlap audit
re-run and passing (worst gap 2.79 device px).
Everything else is v24. Full notes in
`docs/build-report-v25.md`.

**v26 (2026-10-14): small moves back, thinner
lettering.** (1) The **eye stack settles down
partway** — eye centre (220, 77) → (220, 86), GAIN
REDUCTION baseline 150 → 159 (about 40% of the
v24→v25 rise returned; the caption→INPUT gap
narrows by the descent to 12.5 device px and stays
the widest string gap on the module) — and the **POWER
cluster descends a few px further** with it (word
146 → 152, cluster line 180 → 186), keeping the
caption/POWER near-alignment in the same spirit
(measured ink-bottom delta 6.4 virtual px).
(2) **All fonts thin out**: the face is now **Smokum
Emboldened XThin**, the bake at a stroker radius of
29 units = **1.42% of em** — deliberately below the
cautious 1.6–1.9% band, per Nathan ("it can still
thin out a little bit and be okay") — serving panel
text **and** the flange numerals (the v25 heavier
numeral bake is retired) at v25's unchanged sizes
(labels 14.5, markings 14, numerals 17.5 pt).
Legibility gate re-measured on the finished panel:
solid-core 0.64–0.74 across strings, digits peaking
at full 255 and reading clearly — no fallback to a
thicker bake needed. (3) The strip's **BITEY
wordmark mirrors to the top-RIGHT screw** — same
screw centreline (ink centre on y 18), its right
edge keeping the same measured air to the right
screw head (6.05 device px) that its left edge kept
to the left one. Zero-overlap audit re-run and
passing (worst gap 3.30 device px). Everything else
is v25. Full notes in `docs/build-report-v26.md`.

**v27 (2026-10-14): the wordmark settles, two
centrelines align.** Nathan approved v26 ("everything
looks good, the font looks good") with one last
change, plus two alignment moves. (1) **BITEY moves
to the compressor module**: the strip loses its
wordmark entirely (its top is plain panel, just the
four corner screws) and BITEY now sits at the
module's top-RIGHT screw, mirrored against WESTERN
COMPRESSOR — same baseline (22.4, ink riding the y-18
screw centreline), right-anchored at moduleRight − 39;
measured ink gaps to the neighbouring screw heads:
15.3 still px (BITEY, right) vs 14.3 (WESTERN
COMPRESSOR, left) on the same render — the same
1-still-px sidebearing asymmetry the v26 mirror
carried. The module below the top band is
pixel-identical to v26. (2) **POWER aligns with the
eye**: the whole cluster rises −100 virtual px so the
toggle's centre sits on the eye-hub line (cluster
line 186 → 86; measured jewel-ink centroid within
0.3 virtual px of the hub line). (3) **OUTPUT aligns
with the input knob**: the section rises −73 virtual
px so the pots' centres sit on the knob's centre line
(383 → 310; measured disc-ink centroids within
0.1 device px of the knob centre screw's line).
Overlap audit re-run across the strip: the risen
POWER cluster keeps 8.6 device px to the top screws,
~80 device px separate it from the OUTPUT caption,
and the vacated strip bottom is open panel. Full
notes in `docs/build-report-v27.md`.

**v28 (2026-10-14): the wordmarks swap sides.**
Nathan: "Switch western compressor and bitey —
reverse them: put bitey over toward the left screw
and Western compressor over toward the right screw."
The module's top band now reads **BITEY … WESTERN
COMPRESSOR**: BITEY takes the top-left anchor (x 40,
baseline 22.4) and WESTERN COMPRESSOR the top-right
anchor (moduleRight − 39), anchors and baseline
unchanged from v27 — only the strings trade places.
Measured on the A render: both ink spans sit at
y 14..23 (the y-18 screw centreline), and the
ink-to-screw-head gaps come out at **14.3 still px
on both sides** (v27 measured 14.3 left / 15.3
right; the 1-still-px change on the right is the
end glyph's sidebearing — 'R' now, 'Y' before).
Top-band audit re-run: WESTERN COMPRESSOR's
leftward extent (ink x 275..440) keeps 190 still px
from BITEY's ink and a minimum 10.8 device px of air
over the eye bezel's top arc. Pixel-diff proof of
scope: v28-A differs from v27-A only inside the
wordmark band (x 44..440, y 13..24) — everything
else, strip included, is pixel-identical. The strip
stays wordmark-free. Full notes in
`docs/build-report-v28.md`.

**v29 (2026-10-14): the wordmarks move to the
bottom — and everything else rises.** Nathan:
"Change it up: put the title WESTERN COMPRESSOR all
the way down at the bottom of the compressor unit
panel, centred between the two screws, and BITEY all
the way at the bottom of the power module, centred
between the two screws." Both wordmarks leave the
top band (both plates' tops are now bare panel and
screws): **WESTERN COMPRESSOR** is centred between
the module's bottom corner screws (anchor = the
module's centre, mcx) and **BITEY** returns on the
strip, centred between its bottom screws (scx) —
both at baseline 446.4, the mirror of the old top
treatment, so their ink (480..489 still px) rides
the **bottom screw centreline** (y 442). Measured:
ink centres within 1.0 still px of their plate
centres horizontally and 1.7 still px of the screw
line vertically (the same sidebearing class as
every round since v24); air to the neighbouring
screw heads 128/130 still px (module) and 58/60
(strip). ADDITION (same round): **every control
assembly rises 10 virtual px as a whole** — eye +
caption, input stack, LIMIT/MOD clusters, POWER
cluster, OUTPUT section — so the wordmark bands
get room and the switches lift off the bottom; the
wordmarks stay anchored to the screws (centred
both ways, unmoved: block translation dy = 0, ink
spans identical). Block translation measures every
assembly's rise at exactly −11 still px (−10.0
virtual). The two centreline alignments hold:
POWER jewel ↔ eye-hub delta unchanged (0.00 still
px), OUTPUT ring ↔ knob-screw delta changes
+0.05 still px. New bottom clearances: flange rim
→ wordmark ink 20.0 device px; OUT ink → wordmark
ink 12.5 device px; toggle plate bottoms →
wordmark ink 5.0 device px; switch bottoms now
18 device px off the panel's bottom edge (was 13).
Top air after the rise: phosphor top 16.0 device
px from the panel edge; bezel → nearest top screw
head 161 still px circle-exact; both top bands
verified bare. Full notes in
`docs/build-report-v29.md`.

**v30 (2026-10-14): EXPERIMENTAL — the switch
clusters flank the magic eye.** Nathan: "Let's try
one where we move both the limit and the mod
switches up so they're directly aligned with the
power switch so that they're flanking the magic eye
up closer to the top — with all their associated
labels and lights." The LIMIT and MOD clusters
relocate UP as whole assemblies (word above, toggle
on its plate, IN/OUT beside the throws, amber jewel
on the outer flank — v29 internal geometry
untouched): toggle centres land at **(108, 76)**
and **(332, 76)**, mirrored about the module
centre, **on the eye-hub line** — the same line the
POWER toggle's centre rides — so all three toggles
and the eye hub share one horizontal line, and all
three jewels sit on y 76. Measured: component
centre rows identical (Δy = 0.00 device px); jewel
warm-ink centroids within 0.6 still px of the eye
centre's line, jewel-to-jewel spread 0.3 still px;
flanking symmetry 165.0 vs 164.7 still px. The span
(d = 112) is set by the OUT ink's circle-exact air
to the bezel: **3.4 device px** (IN: 8.8); word ink
keeps 34.6/38.4 device px from the top screw heads;
the plate bottoms keep 20.8 device px above the
GAIN REDUCTION caption and 54.3 above the flange
rim. The module's bottom corners the clusters
vacated stay empty panel; the knob, the caption,
the POWER and OUTPUT sections and both bottom
wordmarks do not move (pixel-diff vs v29 confined
to the top cluster band and the vacated bottom
band). Full notes in `docs/build-report-v30.md`.

**v31 (2026-10-14): the flanking pair nudges
outward.** Nathan: the toggles should sit centred
between the eye and the module's outer edges —
"limit move left a little bit, mod move right a
little bit." Each cluster moves OUTWARD **15
virtual px** as a whole assembly at the same
height: toggle centres **(93, 76)** and **(347,
76)** (span d 112 → 127, still mirrored about the
module centre, still on the eye-hub line — the
component centre row is unchanged, Δy = 0.00
device px across all three toggles); words and
IN/OUT anchors ride along, section jewels out to
**(55, 76)** / **(385, 76)** (still centre ∓/± 38).
Measured on the renders: jewel warm-ink centroids
(60.1, 83.3) / (423.1, 83.3) still px against the
eye centre (241.5, 83.5) — flanking symmetry 181.4
vs 181.6 still px. Overlap audit re-run and
passing: the tightest v30 pair, OUT ink ↔ bezel,
grows 3.4 → **11.9 (L) / 11.4 (R) device px**
circle-exact (IN: 8.8 → 17.6/17.3); word ink ↔
bezel 33.9/35.8; word ink keeps ~26–29 device px
from the top screw heads; jewel bezels sit 23.0
device px from the panel side edge / seam (both
sides). Pixel-diff vs v30-A is confined to the two
top cluster bands — the knob column, mid panel,
strip and wordmark bands are pixel-identical.
Full notes in `docs/build-report-v31.md`.

**v32 (2026-10-14): the switches come back down —
a second build, same audio.** Nathan, on the
shipped v31: "let's immediately start another one
with the same audio build where we put the limit
and mod switches back down where they were near
the bottom of the unit… just a little higher than
where they had been previously" — then his
placement correction, which supersedes the
"a little higher": **the toggle CENTRES line up
with the BOTTOM EDGE of the input pot** — the
horizontal line through the flange's lowest point
(knob centre 300 + flange R 100 = **y 400**) — at
the v29 bottom flank positions, centres **(86,
400)** and **(354, 400)**, clusters as whole
assemblies with their internal geometry unchanged
(word = centre −37, IN/OUT −3/+15, jewel centre
∓/± 38 on the outer flank). The corrected line
lands exactly on the v29 bottom geometry (the
v29 rise had already set the centres at y 400),
so the v32 stills are **pixel-identical to
v29's** (max diff 0 on A, B and OFF) — the
flanking trial leaves no trace on this build.
Measured on the render: toggle-centre line at
still y 440; flange silhouette fit centre
(241.5, 330), R 109 → bottom 439 (drawn circle:
440.5); the lit LIMIT jewel's warm centroid at
(52.6, 439.7) — the toggle line and the flange's
bottom edge agree to **≤ ~1 still px**. Overlap
audit re-run on the v32 render, passing: word
ink → flange rim **15.5 (L) / 17.7 (R)** device
px circle-exact, IN ink 15.8/15.4, OUT ink
19.0/18.5; OUT ink → WESTERN COMPRESSOR ink
**12.5 device px**; toggle plate bottoms →
wordmark ink **5.0 device px**; switch component
bottoms **18 device px** off the panel's bottom
edge; jewels ~11 device px above the bottom
screw heads. The strip does not move — POWER
stays on the eye-hub line, OUTPUT on the
knob-centre line — and the eye, caption, knob
stack and both bottom wordmarks are exactly as
v31. DSP untouched; suite still 49/49. With
this build the plugin is renamed **WESTERN
COMPRESSOR** (Nathan, official — the faceplate
has read it since v24); the code/repo/installer
rename is the separate release step, not part of
this GUI pass. Full notes in
`docs/build-report-v32.md`.

- The **strip** is a panel in **gloss black** (v18: the
  brushed metal is gone — a smooth near-black gloss with
  one soft sheen raking across the whole unit), its wear
  concentrated at its **edges and corners** (scratched
  edges: worn rim runs, edge-hugging scratches, corner
  chips; the face stays clean), meeting the module at a
  single seam line, with **big Phillips-head screws** in
  its four corners (v19; v12–v18 had two, top and
  bottom; v22: shrunk to the module screws' size, r 9). Its finish is the material for the whole unit:
  the module wears the same gloss black. Everything on
  the faceplate is drawn in code; the embedded assets
  are the Smokum Emboldened typefaces (v24; v25: the
  Thin bake at 2.34% of em for panel text, the v24
  bake retained for the flange numerals; v26: the
  XThin bake at 1.42% of em for everything, numerals
  included; Sancreek in v23, Smokum in v21–v22,
  Saira Stencil One in v20),
  so the panel renders identically on every
  machine.
- Strip top (v19): a small **BITEY wordmark** at label
  size, top-left — the big floating 'B' is removed —
  (v24: raised so its ink centre rides on the corner
  screw's centreline; the module's wordmark becomes
  WESTERN COMPRESSOR; v26: mirrored to the strip's
  top-RIGHT screw, same centreline and the same
  measured air to its screw; v27: REMOVED from the
  strip entirely — BITEY moves onto the compressor
  module's top-right screw, and the strip's top is
  plain panel) and
  below it the **POWER switch cluster** (v27: risen so
  the toggle's centre sits on the magic eye's hub
  line): the word POWER
  over a silver lever toggle (the third BatToggle,
  bound to `power`), **ON/OFF** labels beside its
  throws, and its **amber jewel** on the cluster's own
  line. The jewel is the power light (lit = power on).
  The photo's P48 toggles are **omitted**, per Nathan.
- The **module** is a plate in the strip's own **gloss
  black** — clean-faced (no scratches across the face,
  per Nathan) with the same scratched-edge wear at its
  perimeter (v18; v16–v17 it wore brushed black, clean)
  — drawn flush as the unit's left face, its four
  Phillips screws inset at its own corners.
- Module upper-centre: a small 1629-style **magic eye tube as
  the live gain reduction display** (110 px virtual since
  v18, ~61 px on screen), drawn like the
  real thing: at 0 dB GR the whole disc glows phosphor green
  around a small dark round centre hub (Allen hex-socket
  screw at its heart); as GR builds, **black eats into the
  green** — the lit sector shrinks back as if the green were
  being pulled out of the eye — until at ~24 dB only a thin
  green sliver and the dark hub remain. No VU meter, no
  numeric readout. (The v15 review stills show the eye at
  rest — 0 dB, entirely green — so the colour composition
  can be judged; the live eye still displays real GR.
  v19: with POWER off the eye is a dead tube — no green
  at all — and its caption, GAIN REDUCTION, now sits
  directly ABOVE the eye. v20: the eye's ring is an
  **aged gold retaining bezel** — brass gradient raked
  from the upper left, highlight broken into uneven arc
  runs, fine scratches worried into the band and small
  chips on the rim — replacing the clean white ring,
  which read as a graphic next to the hardware. v24:
  the eye nudges up a hair (5 virtual px), and the GAIN
  REDUCTION caption moves back UNDER the eye.)
- Module centre: **one** large bakelite knob (the ALL THE WAY
  parameter), carried over from v9/v10 — a stepped profile: a
  smooth domed **top disc**, a fine **ribbed grip ring** under
  it, and a **black Allen centre cap screw** that turns
  with the knob — inside a wide **skirt flange** printed with
  pure-white **numerals 0-10**. There is **no pointer** on the
  knob (the centre screw is its only mark) — **the numbers
  are the pointer**. The flange's numerals **rotate with the
  knob** on the control's 270° sweep, and the value is read
  against a small **fixed index triangle** printed on the
  module at 12 o'clock, just above the flange: the numeral
  under the mark is the setting (numerals ascend
  counterclockwise around the flange, as on the real box, so
  turning clockwise brings the next value up to the mark).
  The word **INPUT** (v19, at the unified label size) sits
  directly above the triangle, naming the pot (v20: it
  drops down to sit almost right on top of the arrow).
  Colour:
  **gloss black** — the shipped default since v19 (the v18
  black-pot variant became the product); the v6 red and the
  v7 yellow/green presentations remain snapshot-selectable
  variants. (v19 also shrinks the numerals onto the flange
  annulus — ring R 87, 15.5 pt — after they floated past
  its edge in v18.)
- Module bottom corners: two **lever switches**, vertical
  throw (in = up, out = down) — **LIMIT** (left: the
  compressor in/out) and **MOD** (right: the warble insert
  in/out), each silk-screened with its function word and
  'in'/'out'. The levers were black (v9–v14) then gold
  (v15–v18); since v19 they are **silver** — steel/chrome-
  family shading, same heavy teardrop form — on small
  **black** escutcheon plates with corner screws. (v19
  also moves both clusters up and outward off the bottom
  edge; v30, experimental, moves them again — up to
  flank the magic eye on its hub line; v31 nudges
  the flanking pair 15 virtual px outward each;
  v32 returns both clusters to the bottom corners,
  toggle centres on the flange's bottom-edge line
  (y 400).)
  Each lever has a small **amber indicator jewel**
  on its outer flank (left of LIMIT, right of MOD —
  locations confirmed by Nathan), drawn with the POWER
  jewel's own rendering, lit while its section is in —
  and, since v19, only while POWER is on.
- Strip bottom (after the photo): two small black fluted
  **output knobs** (captioned just **OUTPUT** since v19)
  with white pointer lines and silk dot markers around
  their travel — **L** and **R** output trims,
  real parameters (`outLevelL`/`outLevelR`, -12...+12 dB,
  0 dB = unity at centre, double-click resets), applied as
  the final per-channel gains in the processor, after the
  chain. The v8-v11 **BITEY** maker line was retired in
  v12 for the badge; since v19 the brand is carried by
  small wordmarks at label size, top-left on each
  plate; since v24 the module's reads **WESTERN
  COMPRESSOR** (the strip keeps **BITEY**), and both
  ride on their top-left screw's centreline.

## What it is

OSC-72 reconstructs a single documented studio trick from November
1971 at Record Plant, New York: an unnamed prototype limiter — built
by a British electronics engineer and left at the studio — had proved
too violent for every track it was tried on, until engineer Shelly
Yakus ran a finished stereo mix through it **in parallel** overnight:
the mix on tracks 1–2 was split to tracks 3–4, those tracks went
through the limiter, and the crushed pair was raised underneath the
originals **until the crushed tracks equalled the originals**. The
result was a Top 5 hit whose meter, per the artist, pinned on the
first chord and never moved — and whose low end surged into the gaps
in the arrangement.

The full research dossier this plugin is built from:
`~/workspace/research_notes/roger-mayer-prototype-limiter-raspberrie-20261014-0546/report.md`
(primary source: Eric Carmen interviewed by Johnny Black, 2 May 2006,
Music Dayz fact 117068).

**Public-copy rule (same as DN1000):** the faceplate and public copy
do **not** name the engineer, his production models, the band, or the
hot-dog brand behind the unit's studio nickname. It is framed on its
own terms: a one-knob parallel limiter.

## The one knob

`ALL THE WAY`, 0–10. It is the only sound-shaping control —
attack, release, threshold, makeup and blend are all wired inside,
exactly as the session trick had them: fixed by the engineer, not
the user. The other faceplate controls are the two corner
in/out levers — **LIMIT** (the compressor itself; out = the
limiter path is fully out of the signal) and **MOD** (the warble
insert) — and, since v12, the two small **O/P LEVEL** trims
(plain output gains, ±12 dB per channel; they shape nothing).
With both levers out and the trims at 0 dB, the plugin is
bit-exact dry at zero latency.

| Knob | Behaviour |
|---|---|
| 0 | Transparent. Threshold sits above full scale, wet weight is 0 — bit-exact unity. |
| 0 → 7 | Limiter threshold falls progressively; gain reduction turns violent; makeup gain tracks the average reduction so the crushed path stays hot; the crushed path blends up underneath the dry signal. |
| **7** | **The documented endpoint: crushed path = dry path (50/50).** |
| 7 → 10 | Crushed path dominates (75% at 10); the dry signal is still present underneath. |

## DSP design — verified vs reconstruction

Framework-free C++17 core in `dsp/` (no JUCE dependency), stereo,
zero reported latency unless the mod insert is in (then its
12 ms base delay is reported honestly), ~1% of one core for the
core at 48 kHz.

**Verified from the research (design-family facts):**
- **Feed-forward detector** — control derived from the input, before
  the gain cell (not tapped from the output).
- **Positive and negative peaks controlled separately** — four
  half-wave envelopes (pos/neg × L/R).
- **Linked stereo gain reduction** — one gain computer, max of all
  envelopes, so the stereo centre holds under heavy GR.
- **No ratio control and no fixed ratio** — dynamic ratio: the
  fraction of the over-threshold excursion removed grows continuously
  toward ~100% as the excursion grows (the manufacturer's own
  description is exponential growth above threshold).
- **Parallel routing + blend-to-equality endpoint** — the session
  account itself.
- FET gain reduction is the documented gain mechanism of the family.

**Reconstruction / inference (never published — labelled as such in
the code):**
- Every exact time constant: GR attack 0.2 ms, GR release 120 ms
  (fast-musical, so low end surges into gaps — the witness's described
  effect), detector catch 50 µs / hang 50 ms, makeup smoothing 300 ms.
  The manufacturer publishes switch positions, never times.
- The knob law: threshold +6 dBFS → −36 dBFS (power 1.2), blend
  0 → 0.5 at knob 0.7 → 0.75 at 1.0, makeup = 0.8 × average GR,
  capped +30 dB.
- A gentle asymmetric Class-A-style soft saturation on the crushed
  path only (small-signal unity, 2nd-harmonic dominant, no hard
  flat-top) — there for density/weight, per Nathan's standing rule.

**Deliberately NOT modelled:** 456HD tape simulation (a modern
reissue addition — anachronistic), transformers and 48 V supply as
features (the vintage unit's I/O arrangement is itself uncertain).

**Unknowns that no amount of research resolved:** the prototype's
model name/number (it may never have had one), whether it was a
direct ancestor of any production model, its fate, and any first-person
account from the engineer who ran the session.

## Tests

```
g++ -std=c++17 -O2 -o build/test_osc72 tests/test_osc72_dsp.cpp dsp/Osc72Dsp.cpp
./build/test_osc72
```

13 checks: knob-0 unity, monotonic GR (measured 0.4 / 9.9 / 23.0 /
33.6 dB mean GR at knob 3 / 5.5 / 8 / 10 on a 0.8-amp 200 Hz sine),
peak control at full knob (0.99 in → 0.57 peak out, still hot),
finite outputs + silence-tail decay, stereo-linked gain match
(1.0% L/R difference on a 40 dB-asymmetric input), blend law
(0 / 0.50 / 0.75), CPU sanity (10 s of stereo in ~99 ms).
Plus 12 warble checks (v7): off = bit-exact, pitch modulation
5.2% peak-to-peak with mean pitch preserved, −33 dB @ 5 kHz
bandwidth limit, delay bounds, click-free toggling, GR untouched.
Plus 7 chain checks (v8): limit-out bit-exactness with mod out,
eye GR = 0 while the bypassed limiter computes GR, zero latency
with both out, limit toggle click-free, and the mod's width
behaviour — 32 total at v8. The suite has since grown through
the audio v9 rework (the knob gangs limiter + transient designer
+ the wet = knob mix law) and the MOD revision (mid/side: centre
dry, hard L/R warble on the side) to **49 checks, all passing**
(run 2026-10-14 alongside GUI v12). Since v8 the suite drives
`osc72::Chain` — the same routing class the processor ships —
rather than a test-side replica.

## Build

```
cd juce && cmake -B ../build -DCMAKE_BUILD_TYPE=Release
cmake --build ../build --config Release
```

Headless GUI snapshot (screenshot-gate verification; knob parked at
7; since v15 the magic eye is shown **at rest** — 0 dB GR via the
display-only override — so the colour composition can be judged;
v1–v14 stills used a ~21 dB override; since v18 the window
is 352x253 and the snapshot renders at 2.0x (704x506 stills).
Tokens: `yellow`/`green` knob variants (the knob ships
black since v19 — no token needed), `black` = gloss-black
pot + beaten case, `warble` = mod in, `limitout` = limit
out, `poweroff` = the v19 power-off state):

```
xvfb-run ../build/OscSnapshot ../docs/gui-v32-a.png
xvfb-run ../build/OscSnapshot ../docs/gui-v32-b.png warble
xvfb-run ../build/OscSnapshot ../docs/gui-v32-off.png poweroff
```

## Status

v1.4.1 — HOST RE-PREPARE FIX (diagnosis
~/workspace/western-compressor-release-1.4.0/
DIAGNOSIS-adt-power-toggle.md). No sound change:
with no re-prepare event the renders are bit-identical
to 1.4.0 (the K7 programme hashes pass unmoved). The
fix is for hosts that re-call prepareToPlay on a live
instance — Luna's top-bar power cycle does, through
the JUCE AU wrapper: the re-call used to rebuild the
chain under a live MOD engagement and park both
inserts forever (their reset parked the engagement
state off while the target survived, and the
no-change setEnabled re-apply could never re-arm
them — the MOD channel went sample-identical to the
raw feed; what read as a "much shorter delay" was
the doubling gone). Two changes: (1) a same-rate
re-prepare now preserves all live chain state — the
re-call is an inaudible non-event; a first prepare or
a genuine rate change still rebuilds from scratch;
(2) any reset whose engagement target is on re-arms
the insert into its 30 ms fade-in, ADT and Phase 90
alike. Regression pins: DSP section N and processor
P12 — against the 1.4.0 DSP the new pins fail with
exactly the diagnosed signature (fade 0.00, output
sample-identical to the feed). Suites: DSP 92/92,
processor 40/40; no pre-existing pin moved.
v1.4.0 — THE MIXER-ARCHITECTURE RELEASE (FULL2
candidate, approved by Nathan's ear 2026-10-19; staging
report ~/workspace/western-compressor-full2-candidate/
REPORT.md). The strip is now a two-channel mixer on an
output bus, PA-200-style: the compressor module outputs
WITHOUT internal makeup; the MOD send is tapped
post-compressor at the module output; COMP MIX and MOD
MIX are parallel channel gains into the bus; the auto
makeup (0.92·avgGR + 7k², peak-aware 1.600 ceiling) is
applied at the bus sum. The module's GR traces are
bit-identical to the interim FULL candidate across all
30 programme configurations. Acceptance, measured: at
COMP MIX 0 the insert feed now travels +18.39 dB RMS
across the INPUT sweep (the previous candidate's feed
was sample-identical at INPUT 5 vs 10 there — the flaw
Nathan's ear caught). Ballistics are the classics-centre
seats: LIMIT attack FAST 3.000 ms / SLOW 14.958 ms,
release SINGLE 90% 0.9986 s / DUAL 90% 3.144 s (T4
memory contrast 1.41×); COMP attacks read 4.438 /
21.021 ms through the shared constants (intrinsic law
ratio, flagged). ADT: right deck carries a minimal
+1.0 ms static stagger (kAdtDeck1StaggerMs, this build
only) so the two decks never sustain the same delay;
mono-in acceptance — centre = SRC dry at full strength,
sides = decks only, delay/pitch trajectory correlation
≈ −0.06; flavour P stays a pure mono-pedal model. Face
gui-v61: one centred MIX word under the two mix pots,
OUTPUT on its own line below the L/R pots. The
sidechain HPF and the old whole-chain MIX parameter are
deleted (sessions' orphaned values are ignored on
restore). Suites: DSP 84/84, processor 38/38.
v1.3.1 — THE 2X BALLISTICS ARE THE ONLY BALLISTICS
(report docs/build-report-v64.md). One change over
1.3.0, on Nathan's ear verdict: the 2X timings "just
sound so much better"; 1X "is just crushing and
wrecking all the transients" — so the 1X speeds are
done away with entirely. The 1X/2X selector is
REMOVED at every level: the mirrored MiniBatToggle
beside RATIO and its 2X/1X silk (gui-v59 — the RATIO
word's line is plain silk again; the face is
byte-identical to gui-v57), and the APVTS 'timesTwo'
parameter and its processor control path. In the
DSP the v62 slewed scale is replaced by the
compile-time constant kBa6aTimeScale = 2.0f at the
same use sites — including the FAST chain's x3
calibration seat — and the frozen FAST constants
are untouched, as ever. THE AUDIO IS BIT-IDENTICAL
TO 1.3.0 WITH 2X ENGAGED: programme renders across
30 seat/mode/MOD configurations hash identically
to reference renders captured from the 1.3.0 tree
with 2X engaged and settled (suite K7a-e), the
absolute attack/release timings land exactly on
the 1.3.0-at-2X values (K1-K6; end-to-end P11b/c:
SLOW rise 16.0 ms, DUAL 50% recovery 0.103 s from
22.5 dB), and the settled scale is now the state
from the FIRST SAMPLE — the selector's ~46 ms
load glide does not exist. Sessions saved under
1.3.0 load fine: the orphaned 'timesTwo' state
value is ignored by APVTS restore (pinned, P11a);
a 2X session is bit-identical, a 1X session now
plays at the baked ballistics — the directive's
intent. Everything else is 1.3.0 exactly: A/P mini
toggle + plain silk, Phase 90 / ADT, meter, ratio
laws, statics. Suites: DSP 72/72, processor 45/45;
snapshots byte-identical to docs/gui-v59-a.png
(default opening state: INPUT 5, LIMIT, lever at
A) and docs/gui-v59-b.png (P selected).
v1.3.0 — A/P SELECTOR BACK TO THE MINI TOGGLE +
1X/2X TIMING DOUBLER (reports
docs/build-report-v61.md, docs/build-report-v62.md,
docs/build-report-v63.md). Three changes over
1.2.0. (1) THE A/P SELECTOR IS A TOGGLE AGAIN
(gui-v57): Nathan on the shipped 1.2.0 — "Pretty
cool. Revert the mod button back to a small toggle
that toggles left right", then "Don't make the A
and the P light up." The round push button and its
lamp letters are gone: the selector is the gui-v54
MiniBatToggle (black escutcheon, silver teardrop
lever, horizontal throw), letters plain white silk
in both states, flavour shown by the lever alone.
The v56 alignment is kept — the assembly stands so
the A ink's left edge lines up with the I and the O
of the IN/OUT labels below (measured 378.64 vs the
column's 378.18 vp). (2) 1X/2X TIMING SELECTOR: a
second mini selector, Nathan's mirror of the A/P —
"a toggle that goes between 1x and 2x ... double
the selected attack and release times". New APVTS
bool `timesTwo` (default false = 1X). One slewed
scale factor multiplies every ballistics tau at its
use site: detector charge and decay, control attack
pole, SINGLE and DUAL release taus, and the T4
memory charge/decay. NOT scaled: B+ sag (a
power-supply behaviour), make-up/slew taus, ratio
laws, statics, colour, ADT and phaser. The frozen
FAST constants are never edited — the FAST chain
takes a calibrated scale (x3.0 at 2X) because the
bench step's t90 carries a fixed resampler-edge
floor; measured after calibration, every timing
lands inside x2.0 ±15% in both modes (attacks
1.93-2.15x, releases 1.97-2.06x; the Dual 90% tail
reads 1.73-1.78x by mechanism — a doubled memory
charge tau means a fixed hold charges less T4
stress, the hardware's own "recovery depends on
previous reduction"). Toggling is click-free (the
scale slews on the 5 ms law coefficient and snaps
exact in ~46 ms). AT 1X THE PLUGIN IS BIT-IDENTICAL
TO 1.2.0: settled at 1X every scaled tau is
multiplied by exactly 1.0f (K7: 0 differing samples
of 192,000; K8: identity restored exactly after a
2X excursion). No latency change in either
position. (3) THE 1X/2X PANEL TOGGLE (gui-v58): a
second MiniBatToggle on the RATIO word's line,
reading 2X · toggle · 1X · RATIO — 1X nearest the
word (the default, mirroring A nearest MOD), the
lever throwing toward the word at 1X and outward
at 2X; the 1X stands on the COMP/LIMIT ink column
exactly as the A stands on IN/OUT's. Plain silk
legend, no lamps. The face now carries the matched
pair: MOD · A · toggle · P at right, 2X · toggle ·
1X · RATIO at left. Suites: DSP 70/70, processor
45/45; snapshots byte-identical to
docs/gui-v58-a.png (default opening state: INPUT 5,
LIMIT, 1X, lever at A) and docs/gui-v58-b.png (2X
selected).

v1.2.0 — 20 dB METER SCALE + MOD A/P PHASE 90
SELECTOR (reports docs/build-report-v55.md,
docs/build-report-v56.md, docs/build-report-v57.md,
docs/build-report-v58.md; design
docs/mod-phaser-design.md). Three changes over
1.1.9. (1) METER RESCALED TO 20 dB + GR SCALE: the
display's full scale drops 28 -> 20 dB (lit law keeps
its 0.85 shape, range constant only; the driver
clamps at 20 so the two dark fronts meet at the
centre at -20; display law only — the audio path and
real GR are untouched, deeper reduction simply tops
the meter out). Under the display, a mirrored scale
in the FAST/SLOW silk: 0 at both outer ends, then
-5 / -10 / -15 stepping inward, and the centre tick
lettered with an infinity glyph instead of -20
(Nathan: "that will really tie it together") — the
ticks stand on the same lit-law formula as the masks,
so each front comes to rest on its own value (the
thermometer reading Nathan approved). The glyph is a
stroked vector lemniscate in the same white silk: no
Smokum bake contains U+221E, so a text glyph would
have fallen back to a machine-dependent system font.
(2) MOD A/P FLAVOUR SELECTOR: the MOD section gains
a second flavour beside the Reel ADT — P is an MXR
Phase 90 (Nathan's "cosmic country" target: George
Harrison, Waylon Jennings, Link Wray). Script-era
voicing: four one-pole allpass stages per channel
sharing ONE LFO coefficient (the pedal is mono; the
stereo image cannot wander), NO feedback loop (the
regeneration resistor is a block-logo addition),
50/50 dry+phased mix (two notches, comb peaks exactly
at unity). Rate fixed at 0.30 Hz; break sweeps one
decade 600 Hz-6 kHz, so the first notch travels
~250 Hz-2.5 kHz. Measured: notch frequencies within
+0.3% of the stage-law prediction at both LFO
extremes, notch depth -138 dB, LFO 0.300 Hz. New
APVTS bool `modPhaser` (false = A, the default; the
A bench output is bit-identical to 1.1.9). Latency
tracks the flavour truthfully: MOD in reads 992
samples (A) vs 32 (P) at 48 kHz, and the dry-tap
blend crossfade follows A<->P edges exactly as it
follows MOD edges (P10 pins). CPU unchanged within
run-to-run noise. (3) THE A/P SELECTOR: a small round
metal push button (MiniPushButton) sits on the MOD
word's own line, reading MOD · A · [button] · P
typeset like text. History: v53 first placed a
miniature bat toggle right of the IN/OUT silk on the
y-400 line — Nathan rejected that ("The toggle
should be just to the right of the label MOD"); v54
moved the assembly onto the word line (the A one word
space — the GAIN REDUCTION space, 5.45 vp — after the
MOD ink); gui-v56 (the current face) replaces the bat
with the push button ("let's just make it a push
button"), shifts the whole assembly +6.36 vp right so
the A ink's left edge stands in the IN/OUT labels'
ink column (measured 378.64 vs the column's 378.18),
and lights the ACTIVE flavour's letter steady in the
jewels' amber while MOD is in and POWER is on (the
section jewels' logic; no pulsing). Suites: DSP
56/56, processor 41/41; snapshots byte-identical to
docs/gui-v56-a.png (INPUT 5, LIMIT, MOD in, A lit)
and docs/gui-v56-b.png (P lit).

v1.1.9 — OVERSAMPLED CORE + INPUT DEFAULT 5 +
RATIO MODE SWITCH (diagnosis
docs/harshness-diagnosis.md; report
docs/build-report-v53.md). Nathan's ear verdict on
1.1.8: the low end is better, but pushing INPUT hard
gives a hard-clipping "breaking glass" at the limits.
Diagnosis: the 1.600 ceiling is exonerated; the harsh
energy is the vari-mu + 6V6 cascade compounding — at
HF it is genuine aliasing (5 kHz / knob 1.0: 8.96%
folded inharmonic vs 0.15% truth at 384 kHz); at
997 Hz and on two-tone material it is ~96% true
cascade harmonics/IMD at extreme depth (the model's
limit character, not a defect). Fix (Nathan
pre-approved the best recommendation): the whole
limiter core runs at 2x the host rate behind a
polyphase Blackman-Harris FIR pair — 5 kHz inharmonic
8.96% -> 1.76%; colour-point THD identical (0.156%);
statics/GR/RMS identical; the shipped ceiling law is
re-applied once at the output rate to absorb
decimation overshoot (slam peaks still bound at
exactly 1.600). Costs stated plainly: CPU roughly
doubles (suite G1 pin re-based 8% -> 12% with this
cause) and reported latency rises exactly 32 host
samples (32 / 992 with MOD), with the MIX blend
re-verified sample-exact at the new latency. INPUT
default: the plugin now opens at 5 (the 1.1.5
staging's "barely kissing" point; opening GR on
-14 dBFS measured 1.59 dB LIMIT / 1.58 dB COMP). The
LIMIT IN/OUT switch is now a RATIO mode switch
(header word RATIO; positions COMP / LIMIT, words
only, no figures; the existing `limit` bool is
repurposed — true = COMP, false = LIMIT, default
LIMIT — so old sessions load without error, only the
stored value's meaning moved: saved OUT now loads
LIMIT-active, saved IN loads COMP). Both modes are
the same vari-mu law family re-voiced gain-computer
side only; measured ratios (perturbation, knob 0.75):
COMP 1.40 / 1.75 / 2.24 / 2.89 / 3.04 and LIMIT 2.62 /
2.93 / 3.45 / 4.30 / 5.01 at 1 / 3 / 6 / 12 / 18 dB GR
— on Nathan's targets (COMP ~1.5:1 digging to ~3:1;
LIMIT ~3:1 already shallow rising to ~4-5:1) within
his ±10-15% voicing tolerance except LIMIT @ 1 dB
(2.62, -13%, at the edge). Density: at knob 7.5 LIMIT
delivers +2.7 dB more GR at identical loudness;
flagged — at knob 10 LIMIT's programme RMS falls
-2.3 dB vs shipped (the firmer law at the very top);
COMP tracks the shipped density everywhere. Bloom:
LIMIT +8.56 dB, COMP +7.36 dB. Ballistics: FAST
attack constants bit-identical to 1.1.5-1.1.8
(Nathan froze the seat — its suite step reading
moves to 0.58-0.71 ms only because the test step is
band-limited by the new resampler; base-rate bench of
the same law reads 0.375 ms); SLOW attack constants
x1.25 toward the LA-2A seat (full step 10.54 ms in
COMP); FAST release and SLOW release verified against
their references, untouched. GUI: v49 face + the
RATIO cluster only (snapshot byte-identical to
docs/gui-v50-a.png, the default opening state).
Installer unchanged from 1.1.6 (relocation off;
preinstall byte-compare passes). Suites: DSP 45/45,
processor 31/31; moved pins ledgered with causes in
the report (A1 deleted/replaced by design — the
LIMIT bypass no longer exists).

v1.1.8 — LF FLUX RELIEF (diagnosis
docs/lf-blowout-diagnosis.md; report
docs/build-report-v52.md). Nathan's ear verdict on
1.1.7: at high INPUT the low end "hits a wall" and
blows out too quickly. Stage bisection put the LF
excess on the irons (92% of distortion power at knob
0.5, 47% at 0.85, ~0% at 1 kHz) — the waveshape gate
stood fully open at storm flux while the regulated
swing ran through it; the detector, the make-up
ceiling and the imbalance term were exonerated by
measurement. Fix (the ONLY change in this release):
the iron gate is multiplied by a flux-relief factor
1/(1+(|flux|/0.35)^2) — real-core saturation behaviour
(incremental permeability collapses, so the colour
stops growing instead of squaring): ~0.99 at nominal
programme flux, ~0.91 at the hot colour point, ~0.52
at a 40 Hz storm peak; a measured no-op at 1 kHz
(flux ~0.02). 40 Hz THD at -10 dBFS: 5.73 -> 4.62% at
knob 0.75, 11.37 -> 9.38% at 0.85; the 5%-THD crossing
moves 0.72 -> 0.76 of knob. Density/weight held:
programme mean GR at max 31.1 dB, LF share unchanged,
bloom +7.87 dB; zero pins moved. GUI unchanged
(snapshot byte-identical to docs/gui-v49-a.png);
installer unchanged from 1.1.6 (relocation off;
preinstall byte-compare passes). Suites: DSP 42/42,
processor 31/31.

v1.1.7 — METER REWORK (GUI v45–v49; reports
docs/build-report-v47.md … v51.md) + ADT TRIM.
Nathan's ear/eye rounds on 1.1.6. The gain-reduction
display first shrank a quarter in height, then came
back up 15%: frame height 70 -> 52.5 -> 60.4 virtual
(width unchanged, still centred on the POWER line;
nothing else on the panel moved). The palette climbed
the brightening ladder: field brights x1.20 (peak stop
(166,255,180)), the dark end stepped in kind, and the
mask became a gradient — leading edge (28,155,65)
falling to floor (25,138,58) at the edges, the falloff
itself then softened by a third at Nathan's word so
the dark triangles sit closer to the main green while
the gradient still reads. Finally the vertical rib
shading deepened +18% at the field centre (cos^2
envelope, exactly zero extra depth at the far edges)
for a deeper, more 3D louver. The artwork floor still
equals the mask floor equals the fully-closed shade,
and the centre green never moves with GR. Audio: one
change — the ADT deck trim goes 15% less again
(kAdtDeckGain 0.4920 -> 0.4182, exactly 51% of the
1.1.0 return level; F2/F3 pins moved with the trim,
still requiring real widening). Installer unchanged
from 1.1.6 (bundle relocation stays off; the shipped
pkg's PackageInfo <relocate/> is empty — verified from
the artifact). Suites: DSP 42/42, processor 31/31;
snapshot byte-identical to docs/gui-v49-a.png.

v1.1.6 — INSTALLER: BUNDLE RELOCATION OFF (the Luna
root cause). Diagnosis closed
(docs/luna-missing-diagnosis.md): the shipped
PackageInfo carried a <relocate> entry for the bundles'
SHARED id (com.Sabatino Audio.OSC72) — pkgbuild's
default analysis marks bundles relocatable
(BundleIsRelocatable), so the Installer 'upgraded' any
pre-existing copy of that id it found on disk (any
mounted volume) instead of writing to /Library, while
our preinstall wiped the /Library copies first:
successful installs left no component in /Library at
all (auval: "didn't find the component"). The artifacts
themselves audited clean: the payload holds all 35
entries (component binary 17,966,336 B, modes correct),
the Bom lists the exact declared paths, Scripts =
preinstall only and PackageInfo declares no other
script phase. Fix: the installer build now analyzes a
component plist, forces BundleIsRelocatable=false on
every bundle and builds with --component-plist (the new
pkg's PackageInfo <relocate/> is empty — verified from
the shipped artifact). The preinstall additionally
clears the user Preferences component cache
(com.apple.audio.AudioComponentCache.plist), which also
carried the dangling OS72 record. Audio code unchanged
from 1.1.5. Suites: DSP 42/42, processor 31/31.

v1.1.5 — STAGING RE-CENTRE + ATTACKS DOUBLED
(v46; report docs/build-report-v46.md). Nathan's ear
verdict on 1.1.4: the box "is still digging in way too
hard" — at INPUT 5 a regular signal should be "just
barely kissing the compressor"; that was happening at ~0,
and by 5 the signal was "absolutely squashed". The drive
law is re-centred as a logistic taper (-5 dB at knob 0 to
the SAME +51 dB at knob 10 — the 1.1.1 depth brief
stands; it was the centre that was wrong): knob 5 tones
-20/-14 dBFS now draw 0.1/1.8 dB GR (was 5.8/9.3),
programme mean GR at max 31.2 dB (was 31.4). Both attack
times doubled by the same verdict (constants scaled per
seat, FAST x2.8 / SLOW x2.65, calibrated so the MEASURED
t90 doubles at the new staging): FAST t90 0.188 ->
0.375 ms, SLOW 3.90 -> 7.83 ms; releases untouched.
Suites: DSP 42/42, processor 31/31; moved pins B2a
(staging pin), C1/C2 bands, C5/H stimulus knobs follow
the depth — all listed in the report.

v1.1.4 — BALLISTICS RE-VOICE
(v45; research docs/ballistics-refs.md, report
docs/build-report-v45.md). The four toggle ballistics
are re-timed after Nathan's reference units.
PROVENANCE, stated plainly: the RCA BA-6A has no
attack/release controls — these are NOT factory BA-6A
settings. ATTACK SLOW and RELEASE SLOW are timed after
the opto pair (seated between the LA-3A and the LA-2A,
with the T4 family's shaped attack and two-stage
memory release); ATTACK FAST is timed after the 1176
attack knob at ~11 o'clock, RELEASE FAST after the
1176 release knob at ~4:30–5 o'clock. Measured
(suite definitions): FAST attack t90 0.67 -> 0.19 ms;
SLOW attack t90 31.8 -> 3.9 ms (demand-shaped);
FAST (Single) release 90% recovery 0.32 -> 0.061 s;
SLOW (Dual) release 50% recovery 0.154 -> 0.069 s,
90% 2.43 -> 2.53 s, now with memory (90% after an
8 s hold 3.03 s vs 2.08 s after a 0.3 s hold).
Suites: DSP 42/42, processor 31/31; moved pins listed
in the report (C1/C2/C3/C4 recalibrated to the new
seats, E3b ceiling 3.0 -> 3.5 and I2 tolerance
0.5 -> 0.75 dB with the mechanism documented, P6c
DUAL clause 0.35 -> 0.28). Rides on top of the
1.1.4 work below. Shipped as release 1.1.4.

v1.1.4 — MIX-alignment
diagnosis + dry-tap transition fix + GUI v44 palette
(full write-up: docs/mix-alignment-diagnosis.md).
Nathan reported a double attack and asked whether the
MIX blend was ever verified phase/sample-accurate.
Honest answer: not directly — P7 pinned the blend
arithmetic against the NOMINAL latency, circularly.
An impulse battery through the full processor now
measures the paths themselves: the blend is
sample-exact in steady state at 44.1/48/96 kHz (wet
IR = dry IR shifted by exactly the latency; DSP pin
F5: max diff 5.8e-11, silence before arrival), and
reported latency is truthful in every switch state.
One real defect found and fixed: the dry tap's read
offset STEPPED at MOD/POWER latency changes while
the chain crossfades over 30 ms, so a transient
inside the engagement window was weighted almost
entirely at the new offset (mid-fade impulse: +0 peak
0.128 / +960 peak 0.378 pre-fix; 0.336 / 0.336
post-fix — pin P9e, which fails on the pre-fix code).
The tap now crossfades its offset over the same
30 ms. New pins P9a–P9e (processor) + F5 (DSP);
suites DSP 38/38, processor 31/31, NO existing pin
moved. The ADT decks' near-unison approaches to the
SRC delay are characterised in the diagnosis §7 and
left as-is per Nathan's ruling ("if it's on the ADT,
it's fine"). GUI v44: the meter's dark end lifted a
fourth step (floor (12, 68, 28) -> (14, 82, 34),
masks = floor, dead glass in step); pixel proofs
re-run (centre strip at 25 dB identical to the 0 dB
artwork, 14,256 mask px exactly the floor); headroom
note in the diagnosis §8.

v1.1.3 / GUI v43 (the Luna-stereo fix + SIDECHAIN
release; reports docs/build-report-v42.md (feature +
diagnosis), docs/build-report-v43.md (v42 tweaks),
docs/build-report-v44.md (v43 palette + this release)):
(1) INSTALLER — the preinstall root-$HOME defect is
fixed: the script runs as root, so its $HOME-relative
AU-cache clear and user-domain wipes targeted /var/root
and never touched the user's account — the root cause
of 1.1.2 not appearing on stereo tracks in Luna (the AU
itself advertises {1,1} {2,2}, probe-verified; full
diagnosis in docs/luna-stereo-diagnosis.md). The
preinstall now resolves the console user (stat
/dev/console → dscl home) for all user-domain steps.
(2) SIDECHAIN — a detector-only high-pass (new APVTS
float 'scCutoff', 10–200 Hz log, default 10 Hz =
filter out of the sense path, every shipped pin
bit-identical at the bottom stop): 40 Hz GR 25.21 →
16.78 dB at sc=200, 1 kHz unmoved. The strip's four
small pots become a 2×2 grid (GUI v41: SIDECHAIN top-
left, MIX moved to the right column, OUTPUT L/R
unmoved below; all four pots 54→50 device px, silk
scaled; the v42 pass deleted the low-cut glyph —
the word SIDECHAIN alone centres on its pot).
(3) METER COLOUR — the field's darks lifted from
near-black into the green family in three steps
(v41 (2,10,4)→(6,32,14); v42 floor →(10,54,22);
v43 floor →(12,68,28), 0.30 →(18,104,41), 0.55
→(22,132,48)): Nathan's direction — the darks read as
shading of the green, closer to the main green each
pass; masks = the ramp floor exactly throughout;
dead glass in step. Pixel proofs hold at v43:
centre strip at 25 dB identical to the 0 dB artwork
(max diff 0); 14,256 mask px read exactly (12,68,28).
Suites: DSP 37/37, processor 22/22. Shipped as
release 1.1.3.

v1.1.2 / GUI v40 (Nathan's ear-verdict refinement pass
on 1.1.1, all measured in docs/build-report-v41.md;
circuit study in docs/engine-v5-circuit-study.md):
(1) SLOW ATTACK GENTLER — the mod's charge path
lengthens (detector τ 5→8 ms, control pole 12→24 ms);
GR t90 ~15→31.8 ms (suite); FAST stock, untouched.
(2) SOFTER UP-FRONT RATIO — the vari-mu law gains the
6H6 square-law toe + remote-cutoff sectional rise
(GR = 27·(shape(env) − shape(verge)), shape = e²/(e +
0.38)): incremental ratio at ~1 dB GR 1.47→1.35:1,
at ~18 dB 3.6→4.14:1 (firms with depth); working
depth kept (GR at −10 dBFS in 11.6 dB; programme GR
at max 31.7 dB).
(3) ADT DOWN ANOTHER ~10% — deck returns ×0.90
(0.5467 → 0.4920 = 60% of the 1.1.0 level); mono
side/out 0.238 → 0.214 at processor level.
(4) GLOW — B+ sag at the output stage (programme-
driven, ~25/240 ms; H3 at working depth 1.55→1.74%);
the two-element remix holds (bloom +9.11 dB at knob
0.5). Two candidate mechanisms were measured and
DROPPED (driver bias-shift — a wash; DUAL slow-share
rebalance — bloom fell), recorded plainly in the
report. Suites: DSP 34/34, processor 18/18; GUI
snapshot byte-identical to v40. Shipped as
release 1.1.2.

v1.1.1 / GUI v40 (Nathan's ear-verdict fixes on 1.1.0,
all measured in docs/build-report-v40.md):
(1) INPUT DIGS IN — the drive law steepens from
−3 + 3k + 12k² (top +12 dB) to −3 + 9k + 45k²
(top +51 dB); knob 0 unchanged. Programme mean GR
at max 9.9 → 31.5 dB; tone GR at centre 5.7 →
12.3 dB; programme RMS still rises at every step;
slam peaks still bounded at exactly 1.600.
(2) ADT DOWN A THIRD — deck returns ×2/3 (0.82 →
0.5467) against the untouched SRC path; mono
side/out 0.446 → 0.238 at processor level.
(3) METER MOTION — the field artwork is built
ONCE, static, and GR moves only two black masks
closing over it (the black moves; the green never
does); the 25 dB centre strip is byte-identical
to the 0 dB artwork's centre. Suites: DSP 32/32,
processor 18/18 (pins moved deliberately, listed
in the report). Shipped as release 1.1.1.

GUI v39 (two changes on the v4 engine tree, both
Nathan's: (1) THE BOW TIE GOES RULER-STRAIGHT — the
v38 span law (t^1.2, sail inside the span) read as
two half-circles that stopped short of the centre;
now the span grows LINEARLY (h = t beyond the
centre bar) and is BLUNT (no sail), so the dark
region's top and bottom boundaries are two straight
lines per side, running all the way in to ONE
SINGLE SOLID GREEN BAR at the centre — the louver
phase shifts half a rib so exactly one rib is
centred on u = ½ and h is zero across precisely
its width, the triangles meeting on either side of
it, the sparkle on the bar. Measured
(build-report-v39.md): silhouette boundary fits a
straight line to max 0.08 virtual px (RMS 0.03;
≤ 0.54 from the design line); zero-dark slices at
0 dB: exactly ONE (the central rib), per-slice
fractions monotone .000 → full at the corners; GR
erosion still edges-first (lit 1.000/.688/.151 vs
law 1.000/.687/.150; extent inside the fronts
1.00 → .68 → .11, centre 0.00 at every GR). The
strength law, the sag, the coverage and the
converging law are v38's, untouched. (2) THE MIX
WORD JOINS THE WORD LINE — the under-pot MIX word
is deleted and the word is redrawn ABOVE its pot
with the identical call as ATTACK/RELEASE
(baseline 149, centred on the pot's x): ink rows
139.5..148.2 for all three words, delta +0.0. The
pot and its markings drop a hair to clear it —
centre (540, 186) → (540, 190), measured
(540.0, 189.8); word-ink-to-top-dot gap 10.0
virtual px. Pixel diff vs v38 confined to the
display field and the MIX zone — 0 changed pixels
anywhere else; DSP suite 31/31, processor suite
18/18, full build green) — **stopped at Nathan's
screenshot gate.** Details: docs/build-report-v39.md.

GUI v38 (four changes on the v4 engine tree, all
Nathan's: (1) the DISPLAY FIELD'S DARK PATTERN NOW
FOLLOWS NATHAN'S OWN LAW, overriding our photo
readings — NO dark at the centre (the shape's size
is ZERO at the centre column, no floor), each
slice's dark shape CENTRED on the slice's vertical
middle and growing gradually and monotonically
across the WHOLE half-width (span h = t^1.2 of the
field height, t = |u − ½|·2), until the dark FILLS
the outermost slices — all dark at the corners (a
fullness ramp straightens the v37 sail to its blunt
span and the shadow strength rises 0.26 → full as
h → 1; the sail's angled-top character and the
bright slices' measured sag are v37's, untouched).
The aggregate at rest is a BOW TIE: two triangles,
full height at the edges, points meeting at the
centre sparkle. Measured (build-report-v38.md):
per-slice dark fraction 0.000 at the innermost
pair rising monotonically to .94 (luminance domain;
.88 on the shipped render's green channel, whose
ramp floor caps the measurable deficit), silhouette
extent 0 across the inner five slice-pairs climbing
to .95 at the edges, profile still one smooth curve
(interior max row-step .034; the centre dip deepens
by construction). The converging GR law rides on
top unchanged (lit .981/.694/.152 vs 1.000/.687/
.150) and the bow tie erodes EDGES-FIRST exactly as
specified — dark extent just inside the fronts
.73 → .37 → .00 across gr 0/10/25, centre 0.00 at
every GR, no closing tweak needed. (2) the GAIN
REDUCTION caption drops "just the slightest
amount" toward the frame: baseline 30 → 33 (ink
+2.7), air to the frame top 11.0 → 8.3 virtual px.
(3) the OUTPUT word LEAVES the v37 L · OUTPUT · R
line and rides ABOVE THE LINK TOGGLE like LIMIT/MOD
above theirs — baseline 363, the LIMIT/MOD words'
own line (ink tops identical at 353.6; air 12.7 to
the L/R ink bottoms, 30.6 to the lowest dots); L
and R stand alone under their pots again, untouched.
(4) NEW: a MIX POT on the strip — the O/P pots' own
small knob centred under the POWER toggle at
virtual (540, 186), ON the ATTACK/RELEASE centre
line, with the O/P silk convention (five dots at
R 30), the word MIX under the pot at the L/R offset,
and the sweep ends marked NUMERICALLY — '0' at the
full-left end of the arc, '100' at the full-right,
just outside the end dots; NO wet/dry words anywhere.
New APVTS float 'mix' 0..1, DEFAULT 1.0 (fully wet —
the pre-feature sound; value strings read "0".."100").
The blend lives in processBlock around the chain:
the dry tap is a delay line read at the chain's
CURRENT latency (MOD in 960, out 0) so mix positions
never comb; the O/P trims stay the wet side's final
stage; POWER off remains bit-exact bypass whatever
the mix. Processor suite grows 8 checks (P7a–P7g):
mix=1 bit-for-bit the default instance (MOD out and
in), mix=0 the input exactly (MOD in: delayed by
exactly 960), mix=0.5 the mean of aligned dry and
wet (max error 5.96e-08), power-off bypass at
mix=0.3 — 18/18; DSP suite 31/31; full JUCE build
green. Pixel diff vs v37 confined to the display,
the caption band, the MIX zone and the OUTPUT zone —
0 changed pixels anywhere else) — **stopped at
Nathan's screenshot gate.** Details:
docs/build-report-v38.md.

GUI v37 (two changes on the v4 engine tree, both
Nathan's: (1) the GR DISPLAY FIELD IS REBUILT AGAIN —
v36's woven band still read as stacked HORIZONTAL
BANDS (light strip / dark strip / diamond row / dark
strip / light strip) because it was built from
horizontal terms; Nathan re-sent his 202W photo and
the field is now built to the photo's actual
structure (measured, docs/build-report-v37.md):
uniform full-height vertical louver slices under ONE
smooth vertical base curve — the width-averaged
profile is a single continuous sag (0.99 at the top,
0.70 at the mid-height minimum, 0.97 at the bottom;
max interior row-step 0.032 at 92-row sampling vs
the photo's 0.019 and v36's 0.079) — plus ONE dark
sail fin per slice: angled top edge, lean mirrored
per field-half (blunt edge toward the centre, point
outward), the fins shrinking toward the centre
slices into small diamonds (~13% of field height at
the innermost pair, photo's pair ~18%) with a tiny
bright sparkle at the meeting point. The band
centreline, the bright channel, the pinch/petal
terms and the width centre-weighting are all DELETED
— nothing horizontal remains to stack into layers.
Frame, caption, converging law and ballistics
unchanged (lit fractions track (1-gr/28)^0.85);
(2) the strip's OUTPUT/LINK header is RELOCATED:
the v36 raised header was too crowded — the OUTPUT
caption drops to the pots' bottom line, centred
between the L and R labels (reads L · OUTPUT · R,
ink gaps 16.4 / 16.2 virtual px, OUTPUT ink centre
539.3 vs strip centre 540), and the LINK cluster
(labels left of toggle, jewel right, lit = linked —
v36 arrangement kept) drops BELOW the pots to the
bottom row: toggle centre virtual (540, 400), the
LIMIT/MOD toggle centres' line — straight across
(jewel centroid measured (582.8, 399.8)); plate
bottom ~425.3 vs BITEY ink top 436.8 = 11.5 virtual
px air, the same clearance the module's bottom row
keeps to WESTERN COMPRESSOR; the band the header
vacated under POWER stays empty panel. The v36
toggle-bottom/top-dot alignment is retired. Block
diff vs v36 confined to the display field, the
vacated header band, the caption line and the LINK
cluster's new home; pots, dots, POWER, LIMIT/MOD
and ATTACK/RELEASE zones: 0 changed pixels. DSP
suite 31/31, processor suite 10/10, full JUCE build
green) — **stopped at Nathan's screenshot gate.**
Details: docs/build-report-v37.md.

GUI v36 (five changes on the v4 engine tree, all
Nathan's: (1) the GR DISPLAY FIELD IS REBUILT PER-PIXEL
— the v35 teeth (which "read as a bunch of jagged saw
teeth"), their feather and the shading overlay are
deleted; the field is now a cached per-pixel image
(buildGrFieldImage) in which every column carries its
own vertical shading (rib louver × phosphor falloff ×
centre-weighting), and the 202W's woven band emerges
from the columns exactly as the original's does: a
bright lens-shaped channel whose half-height swells to
~0.155 of the field at each of 13 diamond centres (one
per two ribs) and pinches to ~0.015 between them, so
the darker regions read as smooth curved triangles
pointing into bright diamonds, with a sparkle point at
each pinch; NO horizontal lines exist anywhere in the
field (self-check: side-by-side crop vs the reference
+ a row-step scan — max single-row step 14.7/255 in
broad gradient shoulders, vs v35's 54 spike at the
feather seam); the converging lit law is unchanged
(bright fractions 0.995 / 0.68 / 0.145 at 0 / 10 / 25
dB, vs law values 1.0 / 0.687 / 0.150); (2) the GAIN
REDUCTION caption moves ABOVE the display (baseline
149 -> 30, centred on the display, ink 20.9..30.0,
10.9 px air to the frame top); ATTACK / RELEASE do
NOT move — baseline 149 is their own line now (their
zone diff vs v35 is exactly the departed caption's
ink, nothing else); (3) the bottom clusters swap
jewel <-> IN/OUT: jewels to the INNER flanks
(125, 400) / (315, 400), labels to the OUTER (anchors
62 right / 378 left); LIMIT now reads IN/OUT, toggle,
jewel and MOD jewel, toggle, IN/OUT; jewel-to-flange
clearance 23.4 virtual px circle-exact; (4)+(5) the
strip gains a LINK CLUSTER in a stacked OUTPUT
header: the OUTPUT caption rises (259 -> 211) and
under it sits a POWER-convention cluster with
Nathan's reversed left/right order — LINK/UNLINK
labels LEFT of the toggle (right-anchored at 512,
baselines 242/260), jewel RIGHT of it at (583,
245.5), lit = linked; toggle centre virtual
(540, 245.5), plate bottom at virtual ~270.7 vs the
pots' top-dot centre line 269.8 — in line, inside
the dot's own span; new APVTS bool 'outLink'
(default linked) with EDITOR-SIDE absolute linkage:
dragging either O/P pot sets both to the dragged
value; engaging link alone changes nothing (a resting
offset persists until the first drag); audio path
untouched; audits re-run and passing, block diff vs
v35 confined to the display, the departed/arrived
caption bands, the bottom clusters and the strip
header band; DSP suite 31/31, processor suite 10/10,
full JUCE build green) — **stopped at Nathan's
screenshot gate.** Details: docs/build-report-v36.md.

GUI v35 (two changes on the v4 engine tree: (1) the
ATTACK / RELEASE clusters DROP DOWN as whole assemblies
(+110 virtual px) — their words land ON the GAIN
REDUCTION caption line (baseline 149, the caption's own
draw baseline; measured ink-centroid deltas +0.05 /
+0.65 still px — one common line reading ATTACK · GAIN
REDUCTION · RELEASE) and the toggles follow directly
under their words at centres (93, 186) / (347, 186),
FAST/SLOW still on the outer flanks, no jewels; the
display and POWER do not move; audit re-run and
passing (plates ≈35.8 virtual px circle-exact off the
flange, word ink 28.5 px clear of the frame's bottom);
(2) the GR display's rib field gains the 202W
original's woven centre band of INTERLOCKING CURVED
TRIANGLES — dark teeth on the bright field, mirror-
symmetric, tapering to a point-to-point pair at the
centre, closing with the same converging-darkness law
(lit widths 168.2 / 115.9 / 25.5 virtual px at
0 / 10 / 25 dB, fronts symmetric to 1 still px);
block diff vs v34 confined to the display + cluster
bands, everything below still y 484 pixel-identical;
DSP suite 31/31, processor suite 10/10, full JUCE
build green) — **stopped at Nathan's screenshot
gate.** Details: docs/build-report-v35.md.

ENGINE v4 / GUI v34 (documented in
docs/engine-v4-design.md and docs/engine-v4-
research.md rather than this log: the BA-6A limiter
+ widest Reel ADT engine replaces the v1.0.3 audio
under the kept face; the round magic eye becomes
the Pioneer 202W-style ribbed GR display in the
eye's aged-gold frame reshaped rectangular, and two
new toggles flank it — ATTACK fast/slow at left,
RELEASE fast/slow (Single/Dual) at right, no
jewels, FAST/SLOW on the outer flanks) — built and
locally verified, awaiting Nathan's verdict
alongside v35.

GUI v32 (the LIMIT/MOD clusters return to the
module's bottom corners as whole assemblies —
toggle centres at virtual (86, 400) and (354,
400), ON THE FLANGE'S BOTTOM-EDGE LINE (knob
centre 300 + flange R 100 = y 400; measured on
the render: toggle-centre line still y 440,
flange silhouette bottom 439, drawn circle
440.5 — delta ≤ ~1 still px; the lit LIMIT
jewel's warm centroid sits at (52.6, 439.7));
section jewels back at (48, 400) / (392, 400)
on the toggles' outer flanks; overlap audit
re-run on the v32 render and passing — word /
IN / OUT ink 15.4–19.0 device px circle-exact
off the flange rim, OUT ink 12.5 device px
above the WESTERN COMPRESSOR ink, toggle plate
bottoms 5.0 device px above the wordmark ink,
switch bottoms 18 device px off the panel edge;
the strip does not move (POWER on the eye-hub
line, OUTPUT on the knob-centre line); the
stills are pixel-identical to v29's, whose
bottom geometry this restores; everything else
as v31; DSP suite still 49/49) — **stopped at
Nathan's screenshot gate.** The Mac variant
build follows separately, after approval. (This
build is the first under the plugin's new
official name, WESTERN COMPRESSOR; the code and
repo rename rides with that release step.)

GUI v31 (the v30 flanking pair nudges OUTWARD a
little — LIMIT left, MOD right, 15 virtual px each
way as whole assemblies: toggle centres at virtual
(93, 76) and (347, 76), span d 112 -> 127, still
mirrored about the module centre and still on the
eye-hub line with POWER (component centre row Δy =
0.00 device px); section jewels out to (55, 76) /
(385, 76); overlap audit re-run for the new
positions and passing — the bezel clearance the
nudge bought shows in the tightest pair, OUT ink
now 11.9/11.4 device px circle-exact (was 3.4),
jewels 23.0 device px from the panel edges;
everything else as v30; DSP suite still 49/49) —
**stopped at Nathan's screenshot gate.** No GitHub
repo, no installer, no download build until the
GUI screenshot is approved.

GUI v30 (EXPERIMENTAL layout trial: the LIMIT and
MOD switch clusters relocate up to flank the magic
eye — whole assemblies, toggle centres at virtual
(108, 76) and (332, 76) on the eye-hub line with
the POWER toggle, section jewels on their outer
flanks on the same line; toggle-centre alignment
Δy = 0.00 device px by construction, jewel-ink
centroids within 0.6 still px of the eye centre's
line on the renders; overlap audit re-run for the
new positions and passing, tightest pair the OUT
ink at 3.4 device px circle-exact from the bezel;
the vacated module bottom corners stay empty
panel; everything else as v29; DSP suite still
49/49) — **stopped at Nathan's screenshot gate.**
No GitHub repo, no installer, no download build
until the GUI screenshot is approved.

GUI v29 (the wordmarks move to the very bottom of
their plates — WESTERN COMPRESSOR centred between
the module's bottom screws, BITEY back on the strip
centred between its bottom screws, both ink centres
on the bottom screw centreline; both top bands
bare — and every control assembly rises 10 virtual
px as a whole for wordmark-band room and switch
lift, the wordmarks themselves unmoved and
screw-anchored; both centreline alignments
verified preserved (deltas change 0.00 / +0.05
still px); bottom-band and top-air audits re-run
and passing; DSP suite still 49/49) — **stopped at
Nathan's screenshot gate.** No GitHub repo, no
installer, no download build until the GUI
screenshot is approved.

GUI v28 (the two module wordmarks swap sides:
BITEY takes the top-left screw anchor, WESTERN
COMPRESSOR the top-right — same anchors, same
baseline 22.4, ink still riding the y-18 screw
centreline; measured ink-to-screw gaps 14.3 still
px on both sides; top-band audit re-run and
passing, WESTERN COMPRESSOR keeping 10.8 device px
over the bezel's top arc; pixel-diff vs v27
confined to the wordmark band, everything else
pixel-identical; strip still wordmark-free; DSP
suite still 49/49) — **stopped at Nathan's
screenshot gate.** No GitHub repo, no installer, no
download build until the GUI screenshot is approved.

GUI v27 (Nathan approved v26 — "everything looks
good, the font looks good" — with one last change
plus two alignments: BITEY leaves the strip (top
now plain panel) for the compressor module's
top-right screw, mirrored against WESTERN
COMPRESSOR with the same measured air; the POWER
cluster rises so the toggle's centre sits on the
eye-hub line (y 86; jewel-ink centroid within 0.3
virtual px); the OUTPUT section rises so the pots'
centres sit on the input knob's centre line
(y 310; disc-ink centroids within 0.1 device px);
strip overlap audit re-run and passing; module
below the top band pixel-identical to v26; DSP
suite still 49/49) — stopped at Nathan's screenshot
gate.

GUI v26 (small moves back: the eye stack settles
down partway — eye centre 86, caption baseline 159 —
and the POWER cluster descends +6 further to line
186, keeping the caption/POWER near-alignment; ALL
lettering thins to SMOKUM EMBOLDENED XTHIN, the
bake at 1.42% of em, numerals included, at v25's
sizes; the strip's BITEY wordmark mirrors to the
top-right screw with the same measured air;
legibility gate passed at true size — solid-core
0.64–0.74, digits peak 255 and read clearly;
zero-overlap audit re-run and passing, worst gap
3.30 device px; everything else as v25; DSP suite
still 49/49) — stopped at Nathan's screenshot gate.

GUI v25 ("not bad… it's close" — fine-tuning:
lettering a little bigger, labels 14.5 / markings
14, and a little thinner via SMOKUM EMBOLDENED THIN,
the bake at 2.34% of em, numerals keeping the v24
bake at 17.5 pt on the widened flange, R 100 / ring
R 90; the eye rises to y 22 and the POWER cluster
drops a hair so the caption/POWER alignment meets in
the middle; the input stack drops to centre 310,
opening caption-to-INPUT space; the O/P pots spread
to 496/584, grow to 27 device px, L/R tucked up;
legibility gate passed at true size; zero-overlap
audit re-run and passing; everything else as v24;
DSP suite still 49/49) — **stopped at Nathan's
screenshot gate.** No GitHub repo, no installer, no
download build until the GUI screenshot is approved.

GUI v24 (Sancreek rejected; the lettering becomes
SMOKUM EMBOLDENED — Nathan's pick from the four-face
specimen sheet, the Western face with a FreeType-stroker
outline expansion of 3.5% of em baked into a derived
font — at v23's metrics: labels 13 pt, markings
12.5 pt, stretch 1.5x; flange numerals 16 pt on the
same annulus; module wordmark becomes WESTERN
COMPRESSOR, the strip keeps BITEY, both wordmarks
riding on their top-left screw's centreline; the eye
nudges up 5 virtual px; GAIN REDUCTION moves back
under the eye; legibility gate passed at true size;
zero-overlap audit re-run and passing; clean direct
vector text only; everything else as v23; DSP suite
still 49/49) — **stopped at Nathan's screenshot
gate.** No GitHub repo, no installer, no download
build until the GUI screenshot is approved.

GUI v23 (the lettering becomes SANCREEK — a genuinely
bolder Western slab, because Smokum's hairline strokes
anti-alias into grey mush at panel sizes and read
"impossible to read" however clean the rendering;
all panel type up ~30% (labels 13 pt, markings
12.5 pt) and the stretch widened 1.2x → 1.5x; flange
numerals keep v22's 16 pt height in the bolder, wider
face, fully on the annulus; legibility gate passed at
true size; zero-overlap audit re-run and passing;
clean direct vector text only; everything else as
v22; DSP suite still 49/49) — **stopped at Nathan's
screenshot gate.** No GitHub repo, no installer, no
download build until the GUI screenshot is approved.

GUI v22 (the spray-paint treatment and the whole
pre-rendered text-image path DELETED — every string
and flange numeral is direct vector text in clean pure
#FFFFFF, no halo, no speckle, no underlay boxes; the
Smokum face shrinks a step and STRETCHES 1.2x
horizontally; flange numerals grow 10.5 → 16 pt for
visibility, fully on the annulus; both BITEY wordmarks
moved clear of the corner screws; the strip's corner
screws shrunk to the module screws' size; zero-overlap
audit re-run and passing; everything else as v21; DSP
suite still 49/49) — **stopped at Nathan's screenshot
gate.** No GitHub repo, no installer, no download build
until the GUI screenshot is approved.

GUI v21 (lettering becomes SMOKUM — a vintage Western
poster face after Nathan's "WESTERN GRANDE" specimen —
with a very subtle spray-paint treatment replacing the
worn pinholes: faint overspray halo + edge speckle, glyph
cores crisp pure white; EVERY panel string shrinks by
about a third and the tracking tightens to near-natural
after the v20 lettering read too big, too spaced out and
overlapping — screws and labels included — and the top
band is re-audited so nothing touches anything; flange
numerals bright pure white again, a touch smaller than
the general shrink; everything else as v20; DSP suite
still 49/49) — stopped at Nathan's screenshot gate
(superseded by v22).

GUI v20 (INPUT dropped to sit almost on top of the
index triangle; lettering back to Saira Stencil One
with a light procedural worn treatment — Rubik Dirt's
style kept, its blur gone: sharper, thinner, brighter;
the magic eye's white ring replaced by an aged gold
retaining bezel with fine scratches and uneven wear;
everything else as v19; DSP suite still 49/49) —
stopped at Nathan's screenshot gate (superseded by v21).

GUI v19 (numerals on the flange; switch clusters up and
outward; one label size panel-wide; Rubik Dirt lettering;
gloss-black knob as the shipped default; silver levers;
BITEY wordmarks replacing the big 'B'; GAIN REDUCTION
above the eye; INPUT over the index triangle; OUTPUT
caption; strip POWER switch cluster with a real `power`
parameter — off = bypassed and all lights out — and the
gui-v19-off.png still; four strip corner screws; DSP
suite still 49/49) — stopped at Nathan's screenshot
gate (superseded by v20).

GUI v18 (window +10% to 352x253 while every control and
marking grows ~x1.5–2; sharp 90° panel/frame corners;
gloss black finish with scratched edges and clean faces;
Special Elite lettering; plus the gloss-black-pot variant
still gui-v18-c.png; audio and parameters unchanged) —
stopped at Nathan's screenshot gate (superseded by v19).

GUI v7 (2026-10-14, also at the gate — `docs/gui-v7-off.png` /
`docs/gui-v7-on.png`, full report in `docs/build-report-v7.md`):
adds the **warble switch** — a residential wall-toggle between the
eye caption and the knob, silk-screened only "off"/"on", engaging
a Lo-Fi Loop Junky-style garbled-pitch insert (post-limiter,
default off, bit-transparent when off; research in
`research-lofi-loop-junky.md`) — and a **per-pixel knob renderer**
replacing the flat-wedge flutes and line-stroke pointer that read
as digital striping with a hard patch around the pointer. Palette
colours unchanged. DSP suite now 25 checks, all passing.

GUI v8 (2026-10-14, at the gate — `docs/gui-v8-a.png` /
`docs/gui-v8-b.png`, full report in `docs/build-report-v8.md`):
the panel becomes a **module plate** screwed into the box (four
plate screws); the centre wall switch is replaced by two corner
**bat toggles** — **limit** (compressor in/out, new DSP bypass
with 30 ms crossfade; out + mod out = bit-exact dry) and **mod**
(the warble insert, which gains a subtle **degraded width**:
independent per-channel drift walks on the read position plus a
gentle width narrowing); the knob's fluting goes low-relief under
a near-round silhouette and its pointer bar shortens. DSP suite
now 32 checks, all passing.

GUI v9 (2026-10-14, at the gate — `docs/gui-v9-a.png` /
`docs/gui-v9-b.png`, full report in `docs/build-report-v9.md`):
from Nathan's reference photo of a vintage decade box — the knob
gains the photo's **stepped profile** (smooth domed top disc,
fine ribbed grip ring, slotted steel **centre cap screw** turning
with the knob) in the same yellowish-gold, and turns against a
**fixed skirt flange** printed with silk-white **numerals 0–10**
(superseding the dashes-only rule); the corner toggles become
the photo's **heavy black levers** on small black escutcheon
plates. GUI-only change; DSP suite still 32 checks, all passing.

GUI v10 (2026-10-14, at the gate — `docs/gui-v10-a.png` /
`docs/gui-v10-b.png`, full report in `docs/build-report-v10.md`):
Nathan's pointer correction — the knob's rotating **pointer is
removed** (the centre screw is the only mark left on the disc);
the flange **numerals rotate with the knob**, and a small fixed
**index triangle** on the plate at 12 o'clock reads the value,
like the panel arrow on the real decade box. With the knob at 7,
the '7' sits at the top under the mark. GUI-only change; DSP
suite still 32 checks, all passing.

GUI v12 (2026-10-14, at the gate — `docs/gui-v12-a.png` /
`docs/gui-v12-b.png`, full report in `docs/build-report-v12.md`):
the face becomes a **rack panel** after Nathan's H2 Audio 002
photo — brushed charcoal, big Phillips corner screws, light
wear; a round **'B' badge** and a glowing amber **POWER jewel**
(lit = LIMIT in) up top; the photo's P48 toggles omitted. The
**v11 lettering** lands with it (all panel text in embedded
**Oswald SemiBold**, a 1930s Alternate-Gothic-line grotesque)
and both centre screws are **Allen hex-socket** heads. New real
controls: **L/R O/P LEVEL** trim knobs (±12 dB, unity at
centre). The BITEY maker line is retired (the badge carries
it). DSP untouched by this pass; suite at 49 checks, all
passing; the new DSP (audio v9 + mid/side MOD) is now linked
into the JUCE build.

GUI v13 (2026-10-14, at the gate — `docs/gui-v13-a.png` /
`docs/gui-v13-b.png`, full report in `docs/build-report-v13.md`):
Nathan read his H2 photo literally: "the power label and
light, the B logo and the output labels and controls go on the
right — that's a separate unit — and then the Bitey compressor
module; the whole thing bolts into that empty gaping space."
The window becomes a **wide rack chassis** (1180x800): the
compressor **module** (weathered plate: eye, decade knob,
LIMIT/MOD levers only) is bolted into a deep, shadowed **bay**
at left on four Phillips screws; the **utility strip** at
right (brushed charcoal, big Phillips screws at its top and
bottom) carries the **'B' badge**, **POWER** + amber jewel and
the **O/P LEVEL** knobs. Lettering changes too — Oswald was
"too high-end and clean, almost like a corporate real estate
font" — to **Limelight**, a wide geometric Art Deco face
("more geometric, less vertical and tight"), picked from 12
candidates rendered with the actual panel strings; the Oswald
asset is retired from the build. Layout + lettering only: DSP,
parameters and control behaviour untouched; suite still 49
checks, all passing.

GUI v14 (2026-10-14, at the gate — `docs/gui-v14-a.png` /
`docs/gui-v14-b.png`, full report in `docs/build-report-v14.md`):
Nathan's three corrections to v13. (1) "Completely ditch the
Art Deco font — make it a seventies font that's easy to
read": all lettering is now **Titan One**, a heavy rounded
70s display face picked from 8 OFL candidates rendered with
the panel strings at true size (Limelight retired to
`Assets/`, unembedded, like Oswald). (2) The strip's **'B'
loses its badge circle** — the bare letter floats on the
strip. (3) The rack **shrinks and unifies**: the bay
collapses to a **mounting lip** — a folded-metal flange with
a lit edge outline just behind the module — so the module
nearly fills its opening and its corner screws land on the
lip, bolted into the perimeter instead of floating mid-bay;
the chassis shell and strip take the **module's own black**;
the window shrinks 1180x800 → 1064x760 and the strip slims
398 → 330 px. Layout + lettering only: DSP, parameters and
control behaviour untouched; suite still 49 checks, all
passing.

GUI v15 (2026-10-14, at the gate — `docs/gui-v15-a.png` /
`docs/gui-v15-b.png`, full report in `docs/build-report-v15.md`):
Nathan's corrections to v14, in two rounds. (1) **Flush
faces**: the double perimeter is deleted — no bay recess,
no lip, no shadow moat; the module's clean face butts
directly against the strip's face, two plates in one plane
with one seam and one shared black. (2) **Size**: the
window shrinks to ~3/4 — 1064x760 → **798x570**, "more
like the size of a regular plugin"; all layout scaled with
it, small labels bumped slightly for legibility. (3)
**Font**: Titan One rejected — "way too cartoonish... too
bubbly, especially that B" — replaced by **Michroma**, the
serious squared Eurostile/Microgramma register of 70s
tech lettering, picked from 10 OFL candidates (Titan One
retired to `Assets/`, unembedded). (4) The knob's **Allen
centre screw is black** (eye hub screw unchanged). (5)
**Section jewels**: a small amber jewel on each toggle's
outer flank (left of LIMIT, right of MOD — locations
confirmed by Nathan), the POWER jewel's own rendering,
lit while its section is in. (6) The LIMIT/MOD **levers
turn gold** — the knob's aged gold; black read cartoonish;
escutcheons stay dark. (7) The v15 **stills show the magic
eye at rest** (0 dB, entirely green) so the colour
composition can be judged — a snapshot-state change only;
the live eye still displays real GR. GUI only: DSP,
parameters and control behaviour untouched; suite still
49 checks, all passing.

GUI v16 (2026-10-14, at the gate — `docs/gui-v16-a.png` /
`docs/gui-v16-b.png`, full report in `docs/build-report-v16.md`):
Nathan's corrections to v15. (1) **Clean module face**: all
scratches/scuffs/ageing removed from the compressor module —
it draws no weathering at all. (2) **One material**: the
module's black becomes the strip's brushed black (same
gradient + vertical brush grain) on both plates. (3)
**Font**: Michroma rejected — "still looks like real estate
font, especially that B" — replaced by **Saira Stencil One**,
stamped-equipment / crate-and-military-stencil lettering:
way more industrial, less serious, more utilitarian (Michroma
retired to `Assets/`, unembedded; runner-up was Barlow Semi
Condensed SemiBold). (4) **Pure white**: all lettering —
every string, the flange numerals and the index triangle —
is now #FFFFFF at full opacity (was the warm silk off-white
0xfff0f2ea). (5) **Size**: 798x570 → **640x460**. (6) **Strip
alignment**: the 'B' drops so its centre sits on the magic
eye's centre line, and the POWER jewel drops so it sits on
the GAIN REDUCTION text's line (POWER label above it; O/P
LEVEL stays at the strip bottom). GUI only: DSP, parameters
and control behaviour untouched; suite still 49 checks, all
passing.

GUI v17 (2026-10-14, at the gate — `docs/gui-v17-a.png` /
`docs/gui-v17-b.png` plus `docs/gui-v17-context.png`, full
report in `docs/build-report-v17.md`): Nathan's size
correction — the unit "still read as a giant full-screen
object; it's a plug-in, it should be smaller — make it
half the size". The window halves exactly, 640x460 →
**320x230**. NOTHING else changes: the panel paint stays
authored in the v16 640x460 space under a 0.5 scale
transform; component bounds, toggle/knob sizes and the
timer's repaint rects are re-based at half scale; the
raster constants that were absolute at the old size
(knob rim inset and drop-shadow offset, centre-screw
radius, the O/P knobs' stroke widths) are now
size-proportional so the artwork scales cleanly. Single
deviation, for legibility: the smallest labels sit a
touch above strict 0.5 scale (virtual sizes: GAIN
REDUCTION 12 → 13 pt, LIMIT/MOD/POWER/O/P LEVEL and the
flange numerals 9.5 → 10.5 pt, IN/OUT and L/R 8.5 → 10 pt;
the 'B' is unchanged). Stills render at 2x (640x460), and
the context shot composites the A render downscaled to
true native 320x230 at the centre of a blank 1440x900
dark-grey desktop canvas — the plugin's real size against
a screen. GUI only: DSP, parameters and control behaviour
untouched; suite still 49 checks, all passing.

GUI v18 (2026-10-14, at the gate — `docs/gui-v18-a.png` /
`docs/gui-v18-b.png` / `docs/gui-v18-c.png` plus
`docs/gui-v18-context.png`, full report in
`docs/build-report-v18.md`): Nathan's sizing corrections —
"the size of the overall plugin could be about 10%
larger, but the size of the knobs, the magic eye, the
output level pots, the switches and the lights and all
their markings — all of that can be about one and a half
times the size they are now, if not two times; also the
edges of the whole thing should be sharp angles, not
curved" — plus two additions: the finish ("the whole
thing is still looking a little too nice/fancy, high-end:
get rid of the brushed aluminum, just GLOSS BLACK with
SCRATCHED EDGES") and the lettering (Saira Stencil
"better but still a little too nice, especially the B").
The window grows 320x230 → **352x253** (paint transform
0.55 over the same 640x460 authoring space) and the
layout is re-based around grown hardware: eye 70 → 110
(virtual), flange R 60 → 95 with numerals at 17 pt, the
knob component 54 → 94 device px, toggles back to their
full 40x54-virtual artwork (22x30 device), all three
jewels at full 1.0 scale, O/P knobs 13 → 23 device px
with their dot ring at R 27, every string ~x1.6–1.75 up
(caption 20 pt, function words 16.5, IN/OUT + L/R 16,
the 'B' 52). The two alignment lines are kept ('B' on
the eye's centre line — both now at virtual y 103;
POWER jewel on the GAIN REDUCTION line — jewel at 175,
caption baseline 182). One collision found in the first
render and fixed: the function words' baseline (412)
put their glyphs behind the grown toggle escutcheons
(components paint over the editor's silk) — the words
moved up to baseline 397, clear of the plates (403+)
and of the flange. **Sharp edges**: the outer rim's
rounded rectangle becomes a square drawRect, the toggle
escutcheons lose their corner radius; screws, jewels,
eye and knobs stay round (hardware). **Finish**:
drawBrushGrain and the old face-weathering are deleted;
both plates are painted by paintGlossBlack() (deep
gloss gradient + one soft diagonal sheen continuous
across the seam) and drawScratchedEdges() adds the wear
ONLY at the perimeter — broken bright rim runs,
edge-hugging scratches biased to the rim, corner chips,
deterministic seeds (the snapshot's beaten-up variant
wears harder). **Font**: SPECIAL ELITE (Apache 2.0,
Astigmatic) embedded, Saira Stencil One retired to
Assets/ unembedded — chosen from a rendered 3-candidate
sheet: Saira stays vector-crisp ("designed"); Stardos
Stencil Bold's Didone serifs read as refined vintage
print; Special Elite's glyphs are genuinely eroded and
ink-starved, and its B is the roughest of the three.
**Variant still**: gui-v18-c.png via the new `black`
snapshot token (KnobColour::black palette through the
existing variant hook) — state A with the main pot and
flange in gloss black, white numerals, black centre
screw, gold toggles unchanged. GUI only: DSP,
parameters and control behaviour untouched; suite
still 49 checks, all passing.
