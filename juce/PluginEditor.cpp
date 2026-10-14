#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>
#include <map>
#include <vector>

// ─────────────────────────────────────────────────────────────────────
// Hotdog panel — a conversion of a mid-century kit
// test-instrument front panel (internal design reference: Heathkit
// CT-1) to this compressor. v8 (Nathan, 2026-10-14): the panel is
// now a MODULE — a rounded-corner plate with four screws, mounted
// into the larger weathered near-black green box — carrying the
// small magic eye tube (a full glowing green disc with a dark
// centre hub at rest; as gain reduction builds, black eats into
// the green as if the phosphor were being pulled out of it), ONE
// fluted knob (low-relief fluting now, near-round silhouette), and
// two small metal bat toggles in the bottom corners: 'limit'
// (left) and 'mod' (right), vertical throw, in = up / out = down.
// The v7 centre wall switch is gone. Drawn entirely in code — no
// text wordmark on the faceplate, and the top strip is plain panel.
// v9 (Nathan, 2026-10-14, from his reference photo of a vintage
// decade box): the knob is reshaped to the photo's stepped profile
// — smooth domed top disc, fine ribbed grip ring, slotted centre
// cap screw — in the same yellowish-gold, turning against a FIXED
// wide skirt flange printed with silk-white numerals 0–10 (the
// panel tick dashes are gone), and the two corner toggles are
// restyled as the photo's heavy BLACK levers on small black
// escutcheon plates. Everything else is v8.
// v10 (Nathan, 2026-10-14): the knob's rotating pointer is REMOVED
// — on the real decade box the numbers are the pointer. The
// flange's numerals now ROTATE WITH THE KNOB, and a small fixed
// silk-white index triangle on the plate at 12 o'clock, just
// above the flange, reads the value: the numeral under the mark
// is the setting. The knob's top disc carries only its centre
// screw. Everything else is v9.
// v11 (Nathan, 2026-10-14): the panel LETTERING changes to a
// 1930s test-equipment / military hand — Oswald SemiBold, an
// Alternate-Gothic-style condensed grotesque of the kind the
// decade box's engravers lettered in, embedded so it renders
// identically everywhere (see panelTypeface()). EVERY drawn
// string uses it: flange numerals, GAIN REDUCTION, LIMIT/MOD,
// IN/OUT, BITEY — one lettering hand across the panel, as the
// period plates had. Also: both centre screws — the knob's cap
// screw and the magic eye's hub — become ALLEN (hex-socket)
// heads instead of slotted. Everything else is v10.
// v12 (Nathan, 2026-10-14, H2 Audio 002 rack-panel photo): the
// module-in-a-box framing is GONE — the whole face is now one
// tall brushed-charcoal RACK PANEL with big Phillips corner
// screws and only LIGHT weathering. Top centre: a round 'B'
// (Bitey) logo badge over a POWER label and a glowing amber
// jewel (lit while LIMIT is in; the photo's P48 toggles are
// omitted, per Nathan). The eye, the decade knob (rotating
// numerals, fixed index, Allen centres — all carried over) and
// the LIMIT/MOD black levers move onto the panel, and two NEW
// small black knobs at the bottom are real L/R O/P LEVEL trims
// (parameters outLevelL/outLevelR, ±12 dB, unity at centre).
// The BITEY maker line is retired — the badge carries the brand.
// v13 (Nathan, 2026-10-14, same H2 photo, read literally): v12
// flattened the photo into one panel — but in the photo the
// badge, POWER jewel and O/P LEVEL knobs live on a SEPARATE
// narrow panel at the RIGHT of a wide rack chassis, and the
// rest is a big empty BAY. So v13 is a wide landscape chassis:
// on the right, the narrow brushed-charcoal utility STRIP
// carries the 'B' badge, POWER + jewel and the O/P LEVEL knobs
// (big Phillips screws at its top and bottom, photo positions);
// on the left, the deep shadowed bay — and bolted into it, on
// four Phillips screws, sits the Bitey compressor MODULE: a
// weathered plate (the v8/v10 module look returns here, where
// it contrasts with the clean strip) carrying ONLY the eye, the
// decade knob and the LIMIT/MOD levers. ALSO v13: the LETTERING
// changes again — Oswald "still looks like it's trying to be
// too high-end and clean, almost like a corporate real estate
// font" (Nathan); he asked for something way weirder and old
// school, "very seventies if needed, or almost Art Deco-like —
// more geometric, less vertical and tight". The face is now
// LIMELIGHT (Sorkin Type, SIL OFL 1.1): wide geometric Art Deco
// display capitals with period inline shading and deco numerals,
// chosen from a 12-candidate sheet rendered with the actual
// panel strings (see docs/build-report-v13.md). DSP, parameters
// and every control's behaviour are untouched — layout and
// lettering only.
// v14 (Nathan, 2026-10-14): three corrections. (1) The Art
// Deco font is ditched COMPLETELY — the face is now TITAN ONE,
// a seventies font that's easy to read (see panelTypeface()).
// (2) The 'B' loses its badge circle: just the letter, floating
// on the utility strip. (3) The rack GEOMETRY tightens: in v13
// the module "looks like it's floating in the centre of that
// gaping space" — the external power rack is too big. So the
// bay shrinks to a mounting LIP whose lit edge outline sits
// just behind the module, the module nearly fills the opening,
// and its four corner screws land on that lip — bolted into
// the perimeter edges. The chassis shell and the strip become
// the SAME black as the module plate, and the whole window
// shrinks (1180x800 -> 1064x760). DSP, parameters and every
// control's behaviour are untouched — layout and lettering only.
// v15 (Nathan, 2026-10-14): five more corrections. (1) The
// double perimeter around the module is DELETED — no bay, no
// lip, no shadow moat: the module's clean face butts FLUSH
// against the power strip's face, two plates in one plane with
// one outer perimeter and one shared black. (2) The whole
// window shrinks again, to ~3/4 of v14 (1064x760 -> 798x570,
// "more like the size of a regular plugin"). (3) Titan One is
// REJECTED — "way too cartoonish... too bubbly, especially
// that B" — the face is now MICHROMA: serious seventies
// tech lettering, squared Eurostile/Microgramma register
// (see panelTypeface()). (4) The knob's Allen centre screw is
// BLACK (the eye's hub screw is unchanged), and the LIMIT/MOD
// levers turn GOLD — black read "a little too cartoonish" —
// each gaining a small amber indicator jewel on its outer
// flank, lit while its section is in (the POWER jewel's own
// rendering; POWER still follows LIMIT). DSP, parameters and
// every control's behaviour are untouched.
// v16 (Nathan, 2026-10-14): five more corrections. (1) The
// compressor module's face is CLEAN now — every scratch,
// scuff and ageing mark is removed from it; the module draws
// no weathering at all. (2) ONE MATERIAL: the module's black
// background becomes the strip's brushed black — the strip's
// gradient + vertical brush grain, verbatim — so both plates
// are the same brushed black metal (the strip keeps its light
// weathering; the module has none). (3) Michroma is REJECTED
// too — "still looks like real estate font, especially that
// B" — the face is now SAIRA STENCIL ONE: stamped-equipment /
// crate-and-military-stencil lettering, "way more industrial
// and less serious, more utilitarian" (see panelTypeface()).
// (4) The window shrinks again (798x570 -> 640x460, regular-
// plugin size). (5) STRIP ALIGNMENT: the 'B' drops so its
// centre sits on the magic eye's centre line, and the POWER
// jewel drops so it sits on the GAIN REDUCTION text's line.
// AND (Nathan, same round): ALL lettering goes to the
// BRIGHTEST POSSIBLE WHITE — pure #FFFFFF at full opacity for
// every text string, the flange numerals and the fixed index
// triangle (kSilk is pure white now). DSP, parameters and
// every control's behaviour are untouched.
// v17 (Nathan, 2026-10-14): SIZE ONLY. The unit still read as
// a giant full-screen object — "it's a plug-in, it should be
// smaller, make it half the size; I can't even really tell
// yet till it's half the size". The window halves exactly:
// 640x460 -> 320x230. The panel artwork is still authored in
// the v16 640x460 space and paint() draws it under a 0.5
// scale transform; the child components' bounds and the
// timer's repaint rects are re-based at the half scale, and
// the knob / small-knob raster constants that were absolute
// at the old size are now size-proportional so the artwork
// scales cleanly. The ONLY other deviation: the smallest
// labels sit a touch above strict 0.5 scale for legibility
// (noted in docs/build-report-v17.md). Design, colours,
// lettering, alignments, DSP, parameters and every control's
// behaviour are otherwise byte-for-byte v16.
// v18 (Nathan, 2026-10-14): a sizing and finish overhaul.
// (1) The window grows ~10% (320x230 -> 352x253; the paint
// transform becomes 0.55 over the same 640x460 authoring
// space) while every CONTROL and MARKING grows ~x1.5–2:
// the decade knob + flange + numerals, the magic eye, the
// O/P LEVEL pots, the LIMIT/MOD toggles, all three jewels
// and every text string — a re-layout in which the hardware
// dominates and the empty panel field shrinks. The two
// cross-panel alignment lines are kept: the 'B' on the eye's
// centre line, the POWER jewel on the GAIN REDUCTION line.
// (2) SHARP EDGES: every panel/frame corner is a square 90°
// angle now — outer rim, plate edges, toggle escutcheons;
// only the hardware (screws, jewels, eye, knobs) stays round.
// (3) FINISH: the brushed metal is KILLED — "still looking a
// little too nice/fancy, high-end" — both plates are smooth
// GLOSS BLACK with a soft sheen, and the wear moves to the
// EDGES and corners (scratched edges; faces stay clean — he
// rejected face scratches in v16). (4) LETTERING: Saira
// Stencil One is "better but still a little bit too nice,
// especially the B" — the face is now SPECIAL ELITE, whose
// glyphs are genuinely eroded and ink-starved (see
// panelTypeface()). (5) A snapshot-only 'black' knob
// colourway renders the gloss-black-pot variant still
// (docs/gui-v18-c.png). DSP, parameters and every control's
// behaviour are untouched.
// v19 (Nathan, 2026-10-14): he likes v18 "a lot more"; the
// fixes: (1) the flange numerals FLOAT OUT past the flange's
// edge — the numeral ring shrinks so every numeral sits
// fully ON the flange annulus. (2) The LIMIT/MOD switch
// clusters sit too close to the bottom edge — both move up,
// LIMIT a bit left, MOD a bit right. (3) ONE label size for
// the whole panel — the LIMIT/MOD size; GAIN REDUCTION,
// POWER and O/P LEVEL come DOWN to it. (4) LETTERING:
// Special Elite was "in the right direction" but the
// typewriter face isn't loved — the face is now RUBIK DIRT,
// the same eaten-away erosion in a heavy stamped slab (see
// panelTypeface()). (5) THE KNOB SHIPS BLACK — the v18
// black-pot variant becomes the product (gold stays in the
// variant mechanism). (6) The LIMIT/MOD levers go SILVER.
// (7) The big floating 'B' is REMOVED — small 'BITEY'
// wordmarks, at the label size, sit top-left on each plate.
// (8) The strip gains a real POWER switch: a full cluster
// matching LIMIT/MOD exactly (word above, silver lever on
// its black escutcheon, ON/OFF labels, amber jewel beside
// it on the GAIN REDUCTION line), bound to the new 'power'
// parameter — off kills the audio (processor bypass) and
// every light, the magic eye included. (9) The strip's
// screws become FOUR, one per corner, like the module's.
// v20 (Nathan, 2026-10-14): he likes v19 overall — three
// refinements. (1) 'INPUT' drops DOWN to sit almost right on
// top of the knob's index triangle (a few px of air, not
// touching). (2) LETTERING: Rubik Dirt's style/direction is
// right but it renders "a little too blurry" — the face goes
// back to SAIRA STENCIL ONE (v16's pick) carried with a LIGHT
// procedural worn treatment (see drawTrackedText): the same
// worn industrial register, but sharper glyphs, thinner
// lines, and the pure white reading brighter because no grey
// erosion smears the strokes. (3) The magic eye's ring stops
// being a graphic: the clean silk-white ring is replaced by
// an AGED GOLD retaining bezel — brass/gold hardware to
// match the jewel bezels' gold tone, fine scratches and
// uneven wear drawn in (see drawEyeBezel). DSP, parameters
// and every control's behaviour are untouched.
// v21 (Nathan, 2026-10-14): the lettering changes AGAIN —
// he supplies a "WESTERN GRANDE" specimen and asks to try
// that vintage Western font "with a little bit of that
// spray paint effect, very subtle". And his v20 complaints
// drive the rest: "all the fonts have gotten too big and
// the letters are too spaced apart and they're all
// overlapping each other and the other labels and
// overlapping some of the screws"; the flange numerals
// "have gotten too dim — they should go back to being
// white, and even a little bit smaller". So: the face is
// SMOKUM, the specimen's own register (tall bracketed
// slab, sturdy, plain); EVERY panel string shrinks by
// about a third and the tracking tightens to near-natural;
// the worn-pinhole treatment becomes a very subtle SPRAY-
// PAINT pass (faint overspray halo + edge speckle, pure
// white kept bright); the numerals go
// back to bright pure white, a touch smaller than the
// general shrink; and the top band is re-audited so no
// string touches a screw, a switch, a plate or another
// string (the v20 BITEY / GAIN REDUCTION crowding is the
// named case). DSP, parameters and every control's
// behaviour are untouched.
// v22 (Nathan, 2026-10-14): the spray paint is GONE —
// "completely get rid of the spray paint effect... make
// it just very clean bright white lettering". The whole
// pre-rendered-text-image path (cached string images, the
// overspray halo + speckle, the numeral underlays — the
// grey boxes he saw ringing the flange numerals) is
// DELETED: every string and numeral is direct vector text
// in pure #FFFFFF again. The Western face (Smokum) stays,
// but SHRINKS a little and STRETCHES — capsFont now sets
// a 1.2x horizontal scale ("make the font more
// stretched... stretch it out a tiny bit"), labels and
// markings down a step (10 / 9.5 pt). The flange numerals
// do the opposite of v21: BIGGER again (10.5 -> 16 pt) so
// they read clearly on the black flange, fully on the
// annulus, nothing around them. The BITEY wordmarks move
// clear of the screws they crowded, the strip's four
// corner screws shrink to the module screws' size, and
// the zero-overlap audit is re-run. DSP, parameters and
// every control's behaviour are untouched.
// v23 (Nathan, 2026-10-14): v22's type is "impossible to
// read" — Smokum's hairline strokes anti-alias to grey
// mush at panel sizes ("the center section of each letter
// is looking very very blurry"). The fix is genuinely
// BOLDER strokes: the face becomes SANCREEK, the chunkier
// Western slab that lost the v21 sheet on ornament —
// judged again at the new, larger size, its bracket spurs
// read as sturdy plate lettering, and its native stroke
// weight is roughly double Smokum's. (The alternative —
// Smokum with offset-pass synthetic emboldening — was
// built and measured: thicker on paper, but the letters
// went uneven and double-struck, the same blur in another
// costume; rejected.) Everything also grows: labels and
// markings up ~30% (10 -> 13, 9.5 -> 12.5), the stretch
// up 1.2x -> 1.5x; the flange numerals KEEP v22's 16 pt
// height (measured ink height matches: 8.8 vs 8.7 virtual
// px) but take the wider stretch and the bolder face.
// Clean direct vector text only — the v22 deletions stay
// deleted. Zero-overlap audit re-run at the new sizes.
// DSP, parameters and every control's behaviour are
// untouched.
// v24 (Nathan, 2026-10-14): Sancreek is REJECTED — "i
// don't like that font". From a specimen sheet of four
// bold faces set with the actual panel strings, Nathan
// picked #4: SMOKUM EMBOLDENED — the v21/v22 Western face
// with its outlines expanded by a FreeType stroker,
// radius 3.5% of em, baked into a derived font file so
// the weight is real outline, drawn once as clean vector
// glyphs (see panelTypeface() and
// Assets/NOTE-SmokumEmboldened.txt). The v23 metrics
// carry over unchanged (labels 13, markings 12.5,
// stretch 1.5x, tracking 0.6/0.4; numerals 16 pt on the
// same annulus). ALSO in v24: (1) the module's top-left
// wordmark becomes 'WESTERN COMPRESSOR' — the strip
// keeps 'BITEY'; (2) BOTH wordmarks rise so their ink
// centres ride on their top-left screw's centreline;
// (3) the magic eye nudges UP a hair (5 virtual px); (4)
// the GAIN REDUCTION caption moves back UNDER the eye
// (reversing v19), the stack now reading eye / caption /
// INPUT / arrow / knob with audited air at every step.
// Sancreek retires like the rest: file stays in
// Assets/, unembedded. DSP, parameters and every
// control's behaviour are untouched.
// ── Variant state (see PluginEditor.h): v6 defaults, snapshot-only
//    selection for the v7 A/B renders. ─────────────────────────────
namespace osc72gui {
namespace { KnobColour gKnob = KnobColour::black; bool gBeaten = false; }   // v19: black is the shipped knob (was red, the v6 default)
void setKnobColour(KnobColour c) { gKnob = c; }
KnobColour knobColour() { return gKnob; }
void setBeatenUp(bool b) { gBeaten = b; }
bool beatenUp() { return gBeaten; }
} // namespace osc72gui

namespace {
const juce::Colour kSilk(0xffffffff);       // v16: PURE white #FFFFFF, full opacity (Nathan: brightest possible white for all lettering + flange numerals + index mark)
const float kPi = juce::MathConstants<float>::pi;

// Knob colourways. Red = the approved v6 knob. Yellow = aged
// mustard bakelite. Green = matched to the magic eye's phosphor
// (its mid green is juce::Colour(0xff3bea66), bright 0xff86ffa6, deep 0xff0d9c3c
// in drawMagicEye) so knob and eye read as the same green.
// Black (v18) = the panels' gloss black, for the black-pot
// variant still.
struct KnobPalette {
    juce::Colour fluteLight, fluteDark, outline;
    juce::Colour faceHi, faceMid, faceDeep, ridge;
};
KnobPalette knobPalette()
{
    switch (osc72gui::knobColour()) {
        case osc72gui::KnobColour::yellow:
            return { juce::Colour(0xffc49a35), juce::Colour(0xff7d5a14), juce::Colour(0xff3d2c07),
                     juce::Colour(0xffe0b34e), juce::Colour(0xffc08c2a), juce::Colour(0xff8a5f16), juce::Colour(0xffd9a93c) };
        case osc72gui::KnobColour::green:
            return { juce::Colour(0xff3bea66), juce::Colour(0xff0a5c26), juce::Colour(0xff043d1a),
                     juce::Colour(0xff5fe07e), juce::Colour(0xff2fae52), juce::Colour(0xff0d7a30), juce::Colour(0xff37c75f) };
        case osc72gui::KnobColour::black:
            // v18 variant still: the pot in the panels' own
            // gloss black — face lit like polished bakelite
            // (deep blacks, a graphite gloss at the top of
            // the shading range), grip ring near-black.
            return { juce::Colour(0xff3f4247), juce::Colour(0xff0e0f10), juce::Colour(0xff000000),
                     juce::Colour(0xff55585e), juce::Colour(0xff26282b), juce::Colour(0xff0b0c0d), juce::Colour(0xff3a3d42) };
        case osc72gui::KnobColour::red:
        default:
            return { juce::Colour(0xff711c15), juce::Colour(0xff471009), juce::Colour(0xff260806),
                     juce::Colour(0xffa53727), juce::Colour(0xff7e2015), juce::Colour(0xff4d120c), juce::Colour(0xff8a271b) };
    }
}

// ── Panel lettering (v11; face changed v13, v14, v15, v16, v18) ──
// ONE typeface for every string on the faceplate — the numerals
// on the flange, GAIN REDUCTION, LIMIT / MOD, IN / OUT, POWER,
// O/P LEVEL, and the badge's 'B' — as a period instrument plate
// had one lettering hand. v11/v12 set it in Oswald SemiBold;
// Nathan rejected it for v13 as too high-end and clean ("almost
// like a corporate real estate font") and asked for way weirder,
// old school — "very seventies if needed, or almost Art Deco-
// like, more geometric, less vertical and tight". The v13 face
// was LIMELIGHT (Sorkin Type Co, SIL OFL 1.1): wide geometric
// Art Deco display capitals with period inline shading and deco
// numerals, chosen from a 12-candidate sheet rendered with the
// actual panel strings (see docs/build-report-v13.md). Embedded
// as binary data so the panel renders identically on every
// machine — no dependence on the host's font library.
// v14 (Nathan, 2026-10-14): "Completely ditch the Art Deco
// font. Make it a seventies font that's easy to read." The
// face became TITAN ONE — and v15 (Nathan, same day) rejects
// it outright: "way too cartoonish... too bubbly, especially
// that B". The sharpened direction: "more serious looking,
// but 70s", with the anti-cartoon constraint on top — NO
// rounded/bubbly letterforms anywhere, and the B above all
// must be a serious B (flat sides, squared bowls). The v15
// face was MICHROMA: a squared tech grotesque in the
// Eurostile/Microgramma register (full notes in
// docs/build-report-v15.md). Michroma is a single medium
// weight, so the smallest labels were set a touch larger than
// strict scale. Titan One is retired like Oswald and
// Limelight: file stays in Assets/, unembedded.
// v16 (Nathan, 2026-10-14): Michroma is REJECTED as well —
// "it still looks like real estate font, especially that B".
// The direction, in his words: "way more industrial and less
// serious, more utilitarian". So the v16 face was SAIRA
// STENCIL ONE (The Saira Stencil Project Authors / Omnibus-
// Type, SIL OFL 1.1): a true stencil face — the lettering of
// equipment plates, shipping crates and military stencil
// kits, stamped rather than drawn. Chosen from a candidate
// sheet rendered with the actual panel strings + numerals +
// the 'B' + true-size IN/OUT strips (full notes in
// docs/build-report-v16.md).
// v18 (Nathan, 2026-10-14): Saira Stencil One is "better but
// still a little bit too nice, especially the B" — push the
// lettering rougher and less refined. Three candidates were
// rendered with the actual panel strings + numerals + the B
// + true-size small strips: Saira stays vector-crisp — a
// clean-cut stencil, designed; STARDOS STENCIL BOLD carries
// the army-stencil name but its high-contrast Didone serifs
// read as refined vintage PRINT, the same "designed" quality
// in a different costume; SPECIAL ELITE (Brian J.
// Bonislawsky DBA Astigmatic (AOETI), Apache License 2.0)
// is the least "nice" by a wide margin — its letterforms are
// genuinely ERODED: ink-starved, uneven, stamped by a tired
// machine, and the distress is baked into the glyphs so it
// is identical at every size with no raster treatment. The
// B is the test letter and its B is rough, worn and
// industrial. At v18's larger label sizes it stays legible
// everywhere (verified in the render crops). Saira Stencil
// One is retired like the rest: file stays in Assets/,
// unembedded.
// v19 (Nathan, 2026-10-14): Special Elite is "in the right
// direction" — the erosion is right — "but I don't love the
// typeface; try a different font with a similar effect
// that's not typeface [typewriter]". Candidates rendered on
// a sheet with the actual panel strings + numerals + the B:
// RUBIK DIRT (The Rubik Filtered Project Authors, SIL OFL
// 1.1) — chosen: the erosion is baked into a heavy stamped
// slab; strokes are eaten and dirt-filled exactly like
// Special Elite's, but the letterforms are industrial
// blocks, not a typewriter's. Its B is a battered slab —
// rough, not nice. BLACK OPS ONE was rejected: a squared
// military face but VECTOR-CLEAN, no erosion at all — the
// Saira Stencil "too nice" failure in a new costume.
// SANCREEK was rejected: Tuscan western-poster serifs, the
// wrong register entirely. IM FELL ENGLISH never made the
// sheet (and its 17th-century book face is the refined-print
// register already rejected with Stardos). Special Elite is
// retired like the rest: file stays in Assets/, unembedded.
// v20 (Nathan, 2026-10-14): Rubik Dirt's style is right but
// it renders "a little too blurry" — its baked-in erosion
// greys and fattens every stroke at panel sizes. The brief:
// same worn industrial family, SHARPER glyphs, THINNER
// lines, BRIGHTER white. A fresh sheet (actual panel
// strings + numerals at true render size) compared Rubik
// Dirt, Allerta Stencil, Big Shoulders Stencil (variable;
// at any usable weight it stays hairline-condensed) and
// SAIRA STENCIL ONE — plain and with a light procedural
// worn treatment. Saira + the treatment wins: crisp stencil
// letterforms (a stamped B, real stencil breaks carrying
// the industrial read), strokes a full weight thinner than
// Rubik Dirt's slab, and the white staying pure #FFFFFF —
// the "wear" is a sparse speckle of knocked-out pinholes
// (see renderWornStringImage), not grey smear, so edges
// stay hard and the white reads brighter. Allerta Stencil
// was the runner-up: thinner and sharp, but its rounded,
// elegant forms lean "designed" again. Rubik Dirt is
// retired like the rest: file stays in Assets/, unembedded.
// v21 (Nathan, 2026-10-14): a new specimen arrives —
// "WESTERN GRANDE", a vintage Western poster face — and
// the instruction is to TRY THAT FONT "with a little bit
// of that spray paint effect, very subtle". A sheet with
// the actual panel strings + numerals at the new (third-
// smaller) sizes compared the open-licence Western faces:
// SMOKUM (Brian J. Bonislawsky DBA Astigmatic (AOETI),
// Apache License 2.0) is the specimen's own family —
// tall slab lettering with bracketed serifs, sturdy and
// PLAIN at heart, narrow enough to cure the overlaps, and
// its lining numerals are clean and tall. SANCREEK is
// chunkier and more ornate (spurred Tuscan details that
// clog at 11 pt); RYE is a decorated Tuscan with big
// inline notches — poster ornament, not plate lettering;
// EWERT is an outline/inline circus face that turns to
// mush small; FRIJOLE is a rough painted brush face — wide,
// heavy, the wrong skeleton; RIBEYE is a soft storybook
// serif. SMOKUM wins on the stated criteria: sturdiest,
// least ornate, still unmistakably the specimen's Western.
// Saira Stencil One is retired like the rest: file stays
// in Assets/, unembedded.
// v22 (Nathan, 2026-10-14): SMOKUM STAYS — his quarrel was
// never the face itself but the spray-paint treatment
// (deleted outright, see drawTrackedText) and the sizes:
// the face is now set a step smaller and stretched 1.2x
// horizontally in capsFont.
// v23 (Nathan, 2026-10-14): SMOKUM GOES — not its style
// but its stroke: hairline strokes turn to grey mush at
// panel sizes and the lettering reads blurry no matter
// how clean the rendering is. The v23 face is SANCREEK
// (The Sancreek Project Authors, SIL OFL 1.1): the
// chunkier Western slab from the v21 candidate sheet,
// its spurs quiet at these sizes, strokes roughly twice
// Smokum's weight, letterforms naturally wider. Measured
// against Smokum plain and Smokum with synthetic
// emboldening at the v23 sizes (see
// docs/build-report-v23.md): Sancreek's letters are the
// only ones that stay hard-edged and solid white down to
// true window size. Smokum is retired like the rest:
// file stays in Assets/, unembedded.
// v24 (Nathan, 2026-10-14): SANCREEK GOES TOO — "i don't
// like that font". A specimen sheet (docs/font-sheet-
// v24.png) set the actual panel strings in four bold
// candidates — Ultra, Alfa Slab One, Bevan, and Smokum
// emboldened — and Nathan picked #4: SMOKUM EMBOLDENED.
// That is this face: Smokum (Brian J. Bonislawsky DBA
// Astigmatic (AOETI), Apache License 2.0) with every
// glyph outline expanded by a FreeType stroker, radius
// 3.5% of em (72 of 2048 units), baked OFFLINE into a
// derived font (see Assets/NOTE-SmokumEmboldened.txt).
// The emboldening is therefore genuine outline weight —
// drawn once, as clean vector glyphs, no synthetic
// double-strike — and it fixes the v22/v23 failure at
// its root: the strokes are roughly half again as heavy
// as Smokum's hairlines while the letterforms stay the
// Western slab Nathan chose in v21. Sancreek is retired
// like the rest: file stays in Assets/, unembedded.
// v25 (Nathan): "not bad… it's close" — the lettering goes
// a LITTLE BIGGER (labels 13 -> 14.5, markings 12.5 -> 14)
// and a LITTLE THINNER: panelTypeface() is now SMOKUM
// EMBOLDENED THIN, the same offline bake at a reduced
// stroker radius — 48 units = 2.34% of em, down from 72
// (3.5%) (see Assets/NOTE-SmokumEmboldenedThin.txt). The
// flange numerals are explicitly NOT part of that change:
// they keep the v24 bake (numeralTypeface below) — and in
// this same round the flange widens (R 95 -> 100) and the
// numerals themselves grow a hair taller (16 -> 17.5) and
// stretch a bit more (1.5x -> 1.65x), on the wider annulus.
// v26 (Nathan): "all fonts can get thinner… they're all a
// little bit too thick now" — and on the cautious 1.6–1.9%
// target first briefed: "It can still thin out a little
// bit and be okay." So BOTH faces are now the XTHIN bake:
// stroker radius 29 units = 1.42% of em (see
// Assets/NOTE-SmokumEmboldenedXThin.txt), panel text and
// flange numerals alike — v25's heavier numeral bake is
// retired with the rest. Sizes stay v25's (labels 14.5,
// markings 14, numerals 17.5 at 1.65x); the legibility
// gate is re-measured on the finished panel
// (docs/build-report-v26.md).
juce::Typeface::Ptr panelTypeface()
{
    static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor(
        BinaryData::SmokumEmboldenedXThinRegular_ttf,
        (size_t) BinaryData::SmokumEmboldenedXThinRegular_ttfSize);
    return tf;
}

// v24/v25: a heavier bake for the flange numerals only.
// v26: the numerals take the XThin bake too (Nathan:
// "all fonts") — same file as panelTypeface().
juce::Typeface::Ptr numeralTypeface()
{
    static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor(
        BinaryData::SmokumEmboldenedXThinRegular_ttf,
        (size_t) BinaryData::SmokumEmboldenedXThinRegular_ttfSize);
    return tf;
}

juce::Font capsFont(float size, bool = true)
{
    // (Two steps: the one-liner is a most-vexing-parse trap —
    // it declares a function instead of a Font.)
    const juce::FontOptions opts(panelTypeface());
    juce::Font f(opts);
    f.setHeight(size);
    // v22 (Nathan): the lettering is STRETCHED — the Western
    // face set ~20% wider than its natural proportions
    // ("shrink the font and make the font more stretched...
    // stretch it out a tiny bit"). Font::getStringWidth
    // accounts for the horizontal scale, so the tracked-text
    // layout math stays exact.
    // v23 (Nathan): wider still — 1.2x -> 1.5x ("make all the
    // fonts wider, stretchier", "20 to 30% wider and bolder").
    f.setHorizontalScale(1.5f);
    return f;
}

// Flange numerals (v25): the v24-weight face, stretched a
// touch more than the panel lettering (1.65x). (v26: the
// XThin bake, like everything else — see panelTypeface.)
juce::Font numeralFont(float size)
{
    const juce::FontOptions opts(numeralTypeface());
    juce::Font f(opts);
    f.setHeight(size);
    f.setHorizontalScale(1.65f);
    return f;
}

// ── Panel text rendering (v22) ──────────────────────────────
// v20 treated the glyphs with worn pinholes; v21 rendered
// every string into a cached image and spray-painted it
// (overspray halo + mist speckle). Nathan's v22 verdict
// kills the whole apparatus: "completely get rid of the
// spray paint effect... make it just very clean bright
// white lettering" — and the grey boxes he saw around the
// flange numerals WERE the spray underlay images. So from
// v22 ALL panel text — labels, wordmarks, markings and the
// flange numerals — is drawn as DIRECT VECTOR TEXT in pure
// #FFFFFF: no cached images, no halo, no speckle, nothing
// between the glyph and the panel. (The spray machinery —
// sprayTextFor / sprayNumeralImage / sprayPaintImage — is
// deleted outright, not bypassed.)

// Letter-spaced capitals, CT-1 silk-screen style. Draws with the
// current font/colour; alignment via the anchor x meaning.
enum class Anchor { left, centre, right };
void drawTrackedText(juce::Graphics& g, const juce::String& text,
                     const juce::Font& font, float tracking,
                     float anchorX, float baselineY, Anchor anchor)
{
    float total = 0.0f;
    for (int i = 0; i < text.length(); ++i)
        total += font.getStringWidth(text.substring(i, i + 1));
    if (text.length() > 1) total += tracking * (float)(text.length() - 1);

    float x = anchorX;
    if (anchor == Anchor::centre) x -= total * 0.5f;
    if (anchor == Anchor::right)  x -= total;

    g.setFont(font);
    for (int i = 0; i < text.length(); ++i) {
        const auto ch = text.substring(i, i + 1);
        g.drawSingleLineText(ch, juce::roundToInt(x),
                             juce::roundToInt(baselineY));
        x += font.getStringWidth(ch) + tracking;
    }
}

// Point on a circle, angle measured clockwise from 12 o'clock.
juce::Point<float> polar(float cx, float cy, float r, float degFromTop)
{
    const float a = degFromTop * kPi / 180.0f;
    return { cx + r * std::sin(a), cy - r * std::cos(a) };
}

// Fan-shaped path: apex at centre, arc spanning the bottom of the
// circle with half-angle hsDeg either side of 6 o'clock.
juce::Path bottomFanPath(float cx, float cy, float r, float hsDeg)
{
    juce::Path p;
    p.startNewSubPath(cx, cy);
    for (float a = 180.0f - hsDeg; a <= 180.0f + hsDeg; a += 3.0f) {
        const auto pt = polar(cx, cy, r, a);
        p.lineTo(pt.x, pt.y);
    }
    p.closeSubPath();
    return p;
}

// Deterministic pseudo-random source for the panel wear, so
// the finish is identical on every repaint (fixed seed LCG).
struct PanelRand
{
    explicit PanelRand(uint32_t seed) : s(seed) {}
    float next()
    {
        s = s * 1664525u + 1013904223u;
        return (float)((s >> 8) & 0xffffffu) / 16777216.0f;
    }
    float range(float lo, float hi) { return lo + (hi - lo) * next(); }
    uint32_t s;
};

// Big Phillips-head panel screw (v12, after the H2 photo): a
// dark steel head with a cross recess — the two bars of the cross
// drawn near-black, with a hairline of light along the edges the
// panel light rakes across, so the cross reads as a recess and
// not a print. angleDeg varies per corner, as real screws land.
void drawPhillipsScrew(juce::Graphics& g, float cx, float cy,
                       float r, float angleDeg)
{
    // Seat shadow
    g.setColour(juce::Colour(0x80000000));
    g.fillEllipse(cx - r, cy - r + 1.6f, r * 2.0f, r * 2.0f);
    // Domed steel head
    juce::ColourGradient grad(juce::Colour(0xff7d8387), cx - r * 0.8f, cy - r * 0.8f,
                              juce::Colour(0xff2c2f33), cx + r * 0.7f, cy + r * 0.9f, false);
    grad.addColour(0.45, juce::Colour(0xff4a4e52));
    g.setGradientFill(grad);
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour(juce::Colour(0x70000000));
    g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.1f);
    g.setColour(juce::Colour(0x40ffffff));
    g.drawEllipse(cx - r + 0.8f, cy - r + 0.6f, r * 2.0f - 1.6f, r * 2.0f - 1.2f, 0.8f);

    // The cross recess, drawn in the screw's own rotated frame.
    g.saveState();
    g.addTransform(juce::AffineTransform::rotation(
        angleDeg * kPi / 180.0f, cx, cy));
    const float barL = r * 0.74f, barW = r * 0.17f;
    g.setColour(juce::Colour(0xe0121315));
    g.fillRoundedRectangle(cx - barL, cy - barW, barL * 2.0f, barW * 2.0f, barW * 0.7f);
    g.fillRoundedRectangle(cx - barW, cy - barL, barW * 2.0f, barL * 2.0f, barW * 0.7f);
    // Light catching the recess walls that face the light
    g.setColour(juce::Colour(0x66ffffff));
    g.drawLine(cx - barL + 1.0f, cy + barW - 0.5f, cx + barL - 1.0f, cy + barW - 0.5f, 1.0f);
    g.drawLine(cx + barW - 0.5f, cy - barL + 1.0f, cx + barW - 0.5f, cy + barL - 1.0f, 1.0f);
    // Centre of the cross sinks darkest
    g.setColour(juce::Colour(0x55000000));
    g.fillEllipse(cx - barW, cy - barW, barW * 2.0f, barW * 2.0f);
    g.restoreState();
}

// ── v18 finish: GLOSS BLACK with SCRATCHED EDGES ─────────────
// Nathan killed the brushed metal ("still looking a little
// too nice/fancy, high-end — get rid of the brushed aluminum
// and just have it like a GLOSS BLACK with SCRATCHED EDGES").
// Both plates wear the same finish: a smooth, deep gloss
// black — a near-black vertical fall with one soft diagonal
// sheen raking across the whole unit (one light, one
// material, continuous across the seam) — and the damage
// lives ONLY at the edges and corners: the rim is worn
// bright in broken runs, short scratches hug the perimeter,
// corner chips cluster where a unit gets knocked. The faces
// themselves stay clean gloss (Nathan rejected scratches
// across the module face in v16). All deterministic (fixed
// seeds), so the wear is identical on every repaint.
void paintGlossBlack(juce::Graphics& g, juce::Rectangle<float> rect, float panelH)
{
    juce::ColourGradient base(juce::Colour(0xff1b1c1e), rect.getX(), 0.0f,
                              juce::Colour(0xff060607), rect.getX(), panelH, false);
    base.addColour(0.52, juce::Colour(0xff0c0c0d));
    g.setGradientFill(base);
    g.fillRect(rect);
    // The gloss sheen: one soft diagonal band of light,
    // clipped to the plate, continuous in panel space so the
    // module and the strip share the same raking highlight.
    g.saveState();
    g.reduceClipRegion(rect.toNearestInt());
    juce::ColourGradient sheen(juce::Colour(0x00ffffff), 0.0f, 0.0f,
                               juce::Colour(0x00ffffff), 640.0f, panelH, false);
    sheen.addColour(0.30, juce::Colour(0x12ffffff));
    sheen.addColour(0.52, juce::Colour(0x08ffffff));
    sheen.addColour(0.78, juce::Colour(0x00ffffff));
    g.setGradientFill(sheen);
    g.fillRect(0.0f, 0.0f, 640.0f, panelH);
    g.restoreState();
}

void drawScratchedEdges(juce::Graphics& g, juce::Rectangle<float> rect,
                        uint32_t seed, float sev)
{
    PanelRand rng(seed);
    const float x0 = rect.getX(), y0 = rect.getY();
    const float x1 = rect.getRight(), y1 = rect.getBottom();
    auto pale = [&rng, sev](float lo, float hi) {
        return juce::Colour::fromRGBA(208, 214, 204,
            (uint8_t)juce::jmin(255.0f, rng.range(lo, hi) * sev));
    };
    // A point at distance t along edge e, inset d from it.
    auto edgePoint = [&](int e, float t, float d) {
        switch (e) {
            case 0:  return juce::Point<float>(x0 + t, y0 + d);   // top
            case 1:  return juce::Point<float>(x1 - d, y0 + t);   // right
            case 2:  return juce::Point<float>(x0 + t, y1 - d);   // bottom
            default: return juce::Point<float>(x0 + d, y0 + t);   // left
        }
    };
    auto edgeDir = [](int e) {
        return (e == 0 || e == 2) ? juce::Point<float>(1.0f, 0.0f)
                                 : juce::Point<float>(0.0f, 1.0f);
    };

    // Rim wear: the paint is worn off the rim itself in broken
    // bright runs — the perimeter line shows through in dashes.
    for (int e = 0; e < 4; ++e) {
        const float extent = (e == 0 || e == 2) ? (x1 - x0) : (y1 - y0);
        float t = rng.range(0.0f, 6.0f);
        while (t < extent - 4.0f) {
            const float len = rng.range(3.0f, 13.0f);
            if (rng.next() < 0.68f) {
                const auto p = edgePoint(e, t, 0.9f);
                const auto d = edgeDir(e);
                g.setColour(pale(30.0f, 78.0f));
                g.drawLine(p.x, p.y, p.x + d.x * len, p.y + d.y * len, 1.4f);
            }
            t += len + rng.range(2.0f, 15.0f);
        }
    }
    // Scratches hugging the edges: short strokes inside a
    // narrow band along each side, mostly running with the
    // edge, a few raking across it. None reach the face.
    const int nScr = (int)std::lround(52.0f * sev);
    for (int i = 0; i < nScr; ++i) {
        const int e = (int)(rng.next() * 4.0f) % 4;
        const float extent = (e == 0 || e == 2) ? (x1 - x0) : (y1 - y0);
        const float t = rng.range(2.0f, extent - 2.0f);
        const float rr = rng.next();
        const float d = 1.0f + 13.0f * rr * rr;      // biased to the rim
        const auto p = edgePoint(e, t, d);
        const auto dir = edgeDir(e);
        const float nrm = (e == 0) ? 1.0f : (e == 1) ? -1.0f : (e == 2) ? -1.0f : 1.0f;
        const juce::Point<float> inN = (e == 0 || e == 2)
            ? juce::Point<float>(0.0f, nrm) : juce::Point<float>(nrm, 0.0f);
        const float along = rng.range(0.55f, 1.0f) * (rng.next() < 0.5f ? 1.0f : -1.0f);
        const float across = rng.range(-0.45f, 0.45f);
        const float len = rng.range(4.0f, 26.0f);
        const juce::Point<float> v = dir * along + inN * across;
        if (rng.next() < 0.72f)
            g.setColour(pale(20.0f, 58.0f));
        else
            g.setColour(juce::Colour::fromRGBA(0, 0, 0,
                (uint8_t)juce::jmin(255.0f, rng.range(24.0f, 52.0f) * sev)));
        g.drawLine(p.x, p.y, p.x + v.x * len, p.y + v.y * len, rng.range(0.7f, 1.1f));
    }
    // Corner knocks: denser short strokes and a few bright
    // chips tight into each corner.
    const juce::Point<float> corners[4] = {
        { x0, y0 }, { x1, y0 }, { x0, y1 }, { x1, y1 }
    };
    const juce::Point<float> inward[4] = {
        { 1.0f, 1.0f }, { -1.0f, 1.0f }, { 1.0f, -1.0f }, { -1.0f, -1.0f }
    };
    for (int c = 0; c < 4; ++c) {
        const int n = (int)std::lround(rng.range(5.0f, 8.0f) * sev);
        for (int i = 0; i < n; ++i) {
            const auto p = corners[c] + juce::Point<float>(
                inward[c].x * rng.range(0.0f, 18.0f),
                inward[c].y * rng.range(0.0f, 18.0f));
            const float ang = rng.range(0.0f, 6.283f);
            const float len = rng.range(3.0f, 12.0f);
            g.setColour(pale(26.0f, 66.0f));
            g.drawLine(p.x, p.y, p.x + std::cos(ang) * len,
                       p.y + std::sin(ang) * len, 0.9f);
        }
        for (int i = 0, nChip = (int)std::lround(3.0f * sev); i < nChip; ++i) {
            const auto p = corners[c] + juce::Point<float>(
                inward[c].x * rng.range(0.0f, 9.0f),
                inward[c].y * rng.range(0.0f, 9.0f));
            const float r = rng.range(0.8f, 2.4f);
            g.setColour(juce::Colour::fromRGBA(216, 222, 210,
                (uint8_t)juce::jmin(255.0f, rng.range(55.0f, 105.0f) * sev)));
            g.fillEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f);
        }
    }
}

// The big floating 'B' (v12 badge; v14 bare letter on the
// strip) is REMOVED in v19 (Nathan) — replaced by small
// 'BITEY' wordmarks at the panel label size, one top-left
// on each plate, drawn as silk in paint().

// Panel jewel (v12, after the H2 photo): a faceted amber
// indicator jewel in a dark bezel. `on` = full amber glow
// spilling onto the panel, bright core, visible facets;
// off = the same jewel gone dark and dull. Drawn at scale s
// (1.0 = the original v12 jewel). v15: ONE rendering serves
// all three jewels — POWER on the strip (lit while LIMIT is
// in) and the new LIMIT / MOD indicator jewels flanking the
// toggles on the module (lit while their section is in).
void drawPowerJewel(juce::Graphics& g, float cx, float cy, bool on, float s = 1.0f)
{
    // Bezel
    {
        const float br = 12.5f * s;
        juce::ColourGradient bez(juce::Colour(0xff4c4f4c), cx - br, cy - br,
                                 juce::Colour(0xff131415), cx + br, cy + br, false);
        g.setGradientFill(bez);
        g.fillEllipse(cx - br, cy - br, br * 2.0f, br * 2.0f);
        g.setColour(juce::Colour(0x50ffffff));
        g.drawEllipse(cx - br, cy - br, br * 2.0f, br * 2.0f, 1.0f);
        g.setColour(juce::Colour(0x80000000));
        const float ir = 10.5f * s;
        g.drawEllipse(cx - ir, cy - ir, ir * 2.0f, ir * 2.0f, 1.6f * s);
    }
    // Glow spill while lit
    if (on) {
        const float gr = 30.0f * s;
        juce::ColourGradient glow(juce::Colour(0x59ff9d1e), cx, cy,
                                  juce::Colour(0x00ff9d1e), cx, cy + gr, true);
        g.setGradientFill(glow);
        g.fillEllipse(cx - gr, cy - gr, gr * 2.0f, gr * 2.0f);
    }
    // The jewel itself
    {
        const float r = 10.0f * s;
        juce::ColourGradient jw(on ? juce::Colour(0xffffe9a8) : juce::Colour(0xff7d5f30),
                                cx - r * 0.35f, cy - r * 0.4f,
                                on ? juce::Colour(0xffb35c00) : juce::Colour(0xff2e1d05),
                                cx + r * 0.4f, cy + r * 0.6f, true);
        jw.addColour(0.45, on ? juce::Colour(0xfff7b733) : juce::Colour(0xff54401c));
        g.setGradientFill(jw);
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        // Facet rings (the photo's jewel is a textured lens)
        g.setColour(juce::Colour::fromRGBA(120, 58, 0, on ? 90 : 50));
        g.drawEllipse(cx - r * 0.62f, cy - r * 0.62f, r * 1.24f, r * 1.24f, 1.0f);
        g.drawEllipse(cx - r * 0.32f, cy - r * 0.32f, r * 0.64f, r * 0.64f, 1.0f);
        // Specular catch, upper left
        g.setColour(juce::Colour::fromRGBA(255, 255, 255, on ? 170 : 60));
        g.fillEllipse(cx - r * 0.42f, cy - r * 0.48f, 3.4f * s, 2.4f * s);
    }
}

// v13 had a rack BAY here (a deep shadowed box interior) and
// v14 a mounting recess + lip around the module. v15 (Nathan)
// deletes the whole apparatus: "you shouldn't see anything
// but the Bitey module clean face butting right up to the
// face of the power section — none of that lighter grey stuff
// under it". The module plate and the strip are drawn flush
// in paint(), in one plane, sharing one black; there is no
// function here any more because there is nothing to draw
// between the two faces but the seam.
} // namespace

// ─────────────────────────────────────────────────────────────────────
// The knob (v7 realism pass, Nathan 2026-10-14): rendered PER-PIXEL
// from an analytic model of a fluted bakelite knob, supersampled
// 3x, body cached. This replaces the v6 approach — flat alternating
// pie wedges for the flutes and thick drawLine strokes for the
// pointer — which read as digital stripes, and whose square line
// caps left a harsh, differently-coloured rectangular patch around
// the pointer. Palette values are UNCHANGED: the shading only moves
// luminance between the palette's own anchor colours.
//   * Skirt: 28 flutes; each pixel is lit by the flute profile's
//     surface-normal tilt (from the upper left, as the whole panel),
//     with ambient occlusion down in the flute valleys.
//   * Face: gentle dome gradient, soft specular sheen, a whisper of
//     concentric machining marks, darkened fillet at the rim.
//   * Pointer: v7/v8 drew a raised bar on the face (stopping at
//     0.56 R in v8 so it ended well inside the face). v10 REMOVES
//     it entirely, per Nathan: on the real decade box the numbers
//     are the pointer. The top disc now carries only the centre
//     cap screw (below).
//   * Bakelite micro-grain: deterministic per-pixel value noise so
//     nothing reads as flat CG.
//   * v9 stepped profile (Nathan's reference photo of a vintage
//     decade box): the face is a smooth domed TOP DISC stepping
//     down at kFaceR to a fine RIBBED GRIP ring (replacing the old
//     fluted skirt), and a steel cap screw sits at the centre,
//     turning with the knob (v11: Allen hex-socket head). The
//     wide numbered skirt
//     FLANGE around it is drawn by the editor (see paint()) — in
//     v9 it was fixed; v10 rotates its numerals WITH the knob,
//     read against a fixed index mark on the plate, like the
//     photo's box.
// ─────────────────────────────────────────────────────────────────────
namespace {
constexpr float kFaceR = 0.80f;      // face radius, in units of knob R
constexpr int kFlutes = 28;
constexpr float kLightTheta = -0.644f; // light azimuth (0 = up, clockwise)

float smoothstepf(float a, float b, float x)
{
    float t = (x - a) / (b - a);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

// Face shading ~0..1: dome gradient toward the light, radial
// falloff, soft specular blob, faint machining rings.
float knobFaceShade(float nx, float ny, float r)
{
    const float toLight = -(nx * 0.6f + ny * 0.8f); // +ve toward upper-left
    float s = 0.70f + 0.26f * toLight;
    s *= 1.0f - 0.10f * (r / kFaceR) * (r / kFaceR);
    const float dx = nx + 0.30f, dy = ny + 0.38f;
    s += 0.16f * std::exp(-(dx * dx + dy * dy) / (2.0f * 0.17f * 0.17f));
    s += 0.012f * std::sin(r * 220.0f) * (1.0f - r / kFaceR);
    return s;
}

juce::Colour mapFaceColour(float s, const KnobPalette& pal)
{
    s = juce::jlimit(0.0f, 1.0f, s);
    if (s < 0.5f) return pal.faceDeep.interpolatedWith(pal.faceMid, s * 2.0f);
    return pal.faceMid.interpolatedWith(pal.faceHi, (s - 0.5f) * 2.0f);
}

// Flute relief depth (v8): the silhouette scallop AND the shading
// slope both scale from this one constant. v7 used 0.045 — a deep
// scallop that read as a cookie. 0.007 is low-relief fluting under
// a near-round silhouette: a gentle shading undulation. (A first
// v8 pass at 0.012 still read wavy — the shading bands following
// the scallop exaggerated it — so the skirt's shading contrast is
// compressed as well; see knobBodyColour.)
constexpr float kFluteDepth = 0.007f;

// v9 grip ring (Nathan's reference photo — vintage decade-box
// knobs): between the smooth top disc and the wide numbered skirt
// flange sits a narrower RIBBED grip section. Seen from above it is
// the ring outside kFaceR: 56 fine, shallow ribs lit by their
// surface-normal tilt, kept dark — a cylinder side in the disc's
// shadow, not stripes. The silhouette scallop still comes from the
// gentle kFlutes profile, so the outline stays near-round.
constexpr int kGripRibs = 56;
constexpr float kGripDepth = 0.004f;

// Body colour (no pointer) at knob-local (nx, ny) in units of R.
// alphaOut: silhouette coverage (fluted edge), 0 outside the knob.
juce::Colour knobBodyColour(float nx, float ny, float grain,
                            const KnobPalette& pal, float& alphaOut)
{
    const float r = std::sqrt(nx * nx + ny * ny);
    const float theta = std::atan2(nx, -ny); // 0 = up, clockwise
    const float prof = 0.5f + 0.5f * std::cos((float)kFlutes * theta); // 1 crest, 0 valley
    const float edgeR = 1.0f - kFluteDepth * (1.0f - prof);
    alphaOut = 1.0f - smoothstepf(edgeR - 0.006f, edgeR + 0.006f, r);
    if (alphaOut <= 0.0f) return juce::Colour(0x00000000);

    if (r < kFaceR) {
        // The smooth domed TOP DISC of the stepped profile.
        juce::Colour col = mapFaceColour(knobFaceShade(nx, ny, r) + grain * 0.4f, pal);
        const float fil = smoothstepf(kFaceR - 0.035f, kFaceR, r);
        col = col.interpolatedWith(pal.faceDeep, fil * 0.35f);
        // The disc's edge thickness: it steps DOWN to the grip
        // ring at kFaceR, so the last sliver of the rim shades
        // toward the outline tone — the step reads in the light.
        const float step = smoothstepf(kFaceR - 0.012f, kFaceR, r);
        return col.interpolatedWith(pal.outline, step * 0.35f);
    }
    // Ribbed GRIP ring: fine ribs, surface-normal tilt from the rib
    // profile slope, compressed contrast and an overall darker
    // range than the old fluted skirt — plus occlusion in the
    // step's shadow just outside the disc and a roll-off at the
    // outer rim where the grip drops toward the flange.
    const float dRib = kGripDepth * 0.5f * (float)kGripRibs
                     * std::sin((float)kGripRibs * theta);
    const float ribTilt = std::atan(dRib / juce::jmax(0.2f, r));
    float gdiff = 0.5f + 0.5f * std::cos(theta + ribTilt * 0.9f - kLightTheta);
    gdiff = std::pow(gdiff, 1.25f);
    const float ribProf = 0.5f + 0.5f * std::cos((float)kGripRibs * theta);
    float gs = 0.30f + 0.38f * gdiff;                            // compressed contrast
    gs *= 0.90f + 0.10f * ribProf;                               // rib-valley occlusion
    gs *= 1.0f - 0.30f * (1.0f - smoothstepf(kFaceR, kFaceR + 0.05f, r)); // step shadow
    gs *= 1.0f - 0.12f * smoothstepf(0.94f, 1.0f, r / edgeR);    // rim roll-off
    gs += grain * 0.4f;
    return pal.fluteDark.interpolatedWith(pal.fluteLight,
                                          juce::jlimit(0.0f, 1.0f, gs));
}

juce::Image renderKnobBody(int w, int h, const KnobPalette& pal)
{
    const int ss = 3;
    juce::Image img(juce::Image::ARGB, w * ss, h * ss, true);
    // v17: the 5 px rim inset was tuned at the v16 component
    // size (106 px) — scale it with the component so the body
    // keeps the same proportion at the half-size 54 px bounds.
    const float inset = 5.0f * (float)std::min(w, h) / 106.0f;
    const float rPx = (std::min(w, h) * 0.5f - inset) * (float)ss;
    const float cx = img.getWidth() * 0.5f, cy = img.getHeight() * 0.5f;
    juce::Image::BitmapData data(img, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < img.getHeight(); ++y) {
        for (int x = 0; x < img.getWidth(); ++x) {
            const float nx = ((float)x + 0.5f - cx) / rPx;
            const float ny = ((float)y + 0.5f - cy) / rPx;
            // Deterministic bakelite micro-grain from pixel coords.
            uint32_t ih = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
            ih = (ih ^ (ih >> 13)) * 1274126177u;
            const float grain = ((float)((ih >> 8) & 0xffff) / 65535.0f - 0.5f) * 0.05f;
            float alpha = 0.0f;
            juce::Colour col = knobBodyColour(nx, ny, grain, pal, alpha);
            data.setPixelColour(x, y, col.withAlpha(alpha));
        }
    }
    return img;
}

// Centre cap screw (v9, from Nathan's reference photo): a recessed
// screw at the heart of the top disc — the knob's shaft
// screw, so it turns WITH the knob: its head keeps a fixed angle
// relative to the knob's rotation. Drawn per-pixel over the
// finished knob body. In v10 it is the only mark on the top disc:
// the rotating pointer is gone (the numbers are the pointer now).
// v11 (Nathan): the head is an ALLEN (hex-socket) head — a dark
// hexagonal socket recess at its centre instead of the v9 slot.
// v15 (Nathan): the head is BLACK (black-oxide hardware) instead
// of the v9–v14 silver steel — shading range compressed to stay
// readable, socket floor dropped below the head's dark end.
void renderKnobCentreScrew(juce::Image& img, float angle)
{
    // Matches the body render's radius (incl. its v17
    // size-proportional inset: 15 image px at the 318 px
    // v16 render width, scaled with the image).
    const float rPx = (float)img.getWidth() * 0.5f
                    - 15.0f * ((float)img.getWidth() / 318.0f);
    const float cx = img.getWidth() * 0.5f, cy = img.getHeight() * 0.5f;
    constexpr float kScrewR = 0.088f, kRecessR = 0.112f;    // in units of R
    const float hexA = angle + 0.85f;                       // socket orientation: fixed offset from the knob angle

    const int x0 = juce::jmax(0, (int)std::floor(cx - kRecessR * rPx - 3.0f));
    const int x1 = juce::jmin(img.getWidth() - 1, (int)std::ceil(cx + kRecessR * rPx + 3.0f));
    const int y0 = juce::jmax(0, (int)std::floor(cy - kRecessR * rPx - 3.0f));
    const int y1 = juce::jmin(img.getHeight() - 1, (int)std::ceil(cy + kRecessR * rPx + 3.0f));

    juce::Image::BitmapData data(img, juce::Image::BitmapData::readWrite);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float nx = ((float)x + 0.5f - cx) / rPx;
            const float ny = ((float)y + 0.5f - cy) / rPx;
            const float rr = std::sqrt(nx * nx + ny * ny);
            if (rr > kRecessR + 0.01f) continue;
            juce::Colour base = data.getPixelColour(x, y);

            // Recess: a dark countersunk ring around the head.
            const float rec = 1.0f - smoothstepf(kRecessR - 0.008f, kRecessR + 0.004f, rr);
            juce::Colour out = base.interpolatedWith(juce::Colour(0xff141311), rec * 0.85f);

            if (rr < kScrewR + 0.006f) {
                // Domed head: light from the upper left, a
                // small hot specular, rim shade toward the edge.
                // v15 (Nathan): the head is BLACK hardware now,
                // not silver — the same luminance shape is mapped
                // through a black-oxide range (near-black up to a
                // graphite gloss), so the dome still reads and
                // the socket (below) reads against it.
                const float lx = nx / kScrewR, ly = ny / kScrewR;
                float lum = 0.58f + 0.30f * (-(lx * 0.6f + ly * 0.8f));
                lum += 0.22f * std::exp(-((lx + 0.3f) * (lx + 0.3f)
                                        + (ly + 0.35f) * (ly + 0.35f)) / 0.18f);
                lum *= 1.0f - 0.35f * smoothstepf(0.75f, 1.0f, rr / kScrewR);
                juce::Colour steel = juce::Colour(0xff0b0c0d)
                    .interpolatedWith(juce::Colour(0xff484b4f),
                                      juce::jlimit(0.0f, 1.0f, lum));
                // The Allen socket (v11): a hexagonal recess at
                // the centre of the head, turning with the screw.
                // Near-black inside; a facet term breaks the six
                // walls apart, the wall away from the light catches
                // a rim of it (light rakes into a recess and lands
                // on the far wall), and just outside the hex a
                // bevel shadow ring sells the depth — the things
                // that make it read as a hole, not a printed hex.
                {
                    const float ca = std::cos(hexA), sa = std::sin(hexA);
                    const float hx = nx * ca + ny * sa;
                    const float hy = -nx * sa + ny * ca;
                    const float hd = std::max(std::abs(hx),
                                     std::max(std::abs(0.5f * hx + 0.8660254f * hy),
                                              std::abs(-0.5f * hx + 0.8660254f * hy)));
                    constexpr float kHexIn = 0.040f;   // socket inradius, units of R
                    if (hd < kHexIn + 0.006f) {
                        const float hexCov = 1.0f - smoothstepf(kHexIn - 0.004f, kHexIn + 0.005f, hd);
                        const float rel = (nx * 0.6f + ny * 0.8f) / kHexIn;
                        const float wall = smoothstepf(0.35f, 0.95f, hd / kHexIn)
                                         * juce::jmax(0.0f, -rel);
                        const float facet = 0.5f + 0.5f * std::sin(std::atan2(hy, hx) * 3.0f);
                        // v15: on the black head the socket floor
                        // sits BELOW the head's dark end, and its
                        // lit wall tops out below the head's gloss
                        // — the recess reads by depth, not by a
                        // silver/dark contrast that no longer
                        // exists.
                        juce::Colour sock = juce::Colour(0xff020303)
                            .interpolatedWith(juce::Colour(0xff232527),
                                              juce::jlimit(0.0f, 1.0f, 0.16f * facet + 0.55f * wall));
                        steel = steel.interpolatedWith(sock, hexCov);
                    } else if (hd < kHexIn + 0.018f) {
                        const float bev = 1.0f - smoothstepf(kHexIn + 0.004f, kHexIn + 0.018f, hd);
                        steel = steel.interpolatedWith(juce::Colour(0xff000000), bev * 0.45f);
                    }
                }
                const float cov = 1.0f - smoothstepf(kScrewR - 0.004f, kScrewR + 0.006f, rr);
                out = out.interpolatedWith(steel, cov);
            }
            data.setPixelColour(x, y, out);
        }
    }
}
} // namespace

void OscKnobLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y,
                                          int width, int height,
                                          float sliderPosProportional,
                                          float rotaryStartAngle,
                                          float rotaryEndAngle,
                                          juce::Slider&)
{
    const float cx = x + width * 0.5f;
    const float cy = y + height * 0.5f;
    // v17: inset and shadow offset scale with the component
    // (both were absolute at the v16 106 px size).
    const float sizeK = (float)std::min(width, height) / 106.0f;
    const float radius = std::min(width, height) * 0.5f - 5.0f * sizeK;
    const float angle = rotaryStartAngle
                      + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    const auto pal = knobPalette();

    // Drop shadow (as v6)
    g.setColour(juce::Colour(0x70000000));
    g.fillEllipse(cx - radius, cy - radius + 7.0f * sizeK, radius * 2.0f, radius * 2.0f);

    const int palKey = (int)osc72gui::knobColour();
    if (bodyCache_.isNull() || cacheW_ != width || cacheH_ != height
        || cachePalette_ != palKey) {
        bodyCache_ = renderKnobBody(width, height, pal);
        cacheW_ = width; cacheH_ = height; cachePalette_ = palKey;
    }
    juce::Image img = bodyCache_.createCopy();
    // v10: NO pointer on the knob — the rotating numbered flange
    // is the readout, read against the editor's fixed index mark.
    // Only the centre cap screw is drawn over the body, turning
    // with the knob.
    renderKnobCentreScrew(img, angle);
    // NOTE: reset to an opaque brush before drawImage — with a
    // translucent brush still set from the shadow above, this JUCE
    // build's software renderer leaks the brush alpha into the
    // image blit (the knob came out at ~44% over the panel).
    g.setColour(juce::Colour(0xffffffff));
    g.drawImage(img, x, y, width, height, 0, 0, img.getWidth(), img.getHeight());
}

// ─────────────────────────────────────────────────────────────────────
// Bat toggle (v8; restyled v9) — v9 dresses it as the reference
// photo's switch: a heavy BLACK bakelite lever with a rounded
// teardrop tip, pivoting from a dark collar in a black slot, on a
// small black rectangular escutcheon with tiny corner screws. It
// flips UP for 'in' and DOWN for 'out'; the flip is animated
// through the panel plane — the lever foreshortens as it passes
// centre — over ~120 ms, driven by the editor timer. No text on
// the toggle itself: the editor silk-screens 'limit'/'mod' and
// 'in'/'out' around it.
// ─────────────────────────────────────────────────────────────────────
BatToggle::BatToggle() : juce::Button(juce::String())
{
    setClickingTogglesState(true);
    setWantsKeyboardFocus(false);
}

bool BatToggle::advanceAnimation()
{
    const float target = getToggleState() ? 1.0f : 0.0f;
    if (!snapped_) { flipT_ = target; snapped_ = true; return false; }
    if (flipT_ == target) return false;
    const float step = 0.30f;
    flipT_ = target > flipT_ ? juce::jmin(target, flipT_ + step)
                             : juce::jmax(target, flipT_ - step);
    return true;
}

void BatToggle::paintButton(juce::Graphics& g, bool, bool)
{
    if (!snapped_) { flipT_ = getToggleState() ? 1.0f : 0.0f; snapped_ = true; }
    // v15: the artwork below is authored in a virtual 40x54
    // space (the v8–v14 component size); the v15 toggles are
    // 30x41, so the whole drawing is scaled to the real bounds.
    const auto realB = getLocalBounds().toFloat();
    g.addTransform(juce::AffineTransform::scale(realB.getWidth() / 40.0f,
                                                realB.getHeight() / 54.0f));
    const juce::Rectangle<float> b(0.0f, 0.0f, 40.0f, 54.0f);
    const float cxp = b.getCentreX(), cyp = b.getCentreY();
    const float t = flipT_ * flipT_ * (3.0f - 2.0f * flipT_); // eased
    const float sgn = 2.0f * t - 1.0f;    // -1 = out (down), +1 = in (up)
    const float dirY = sgn >= 0.0f ? -1.0f : 1.0f;

    // Escutcheon (v9): a small black rectangular plate echoing the
    // reference photo's switch plate — near-black bakelite, a faint
    // top light catch, four tiny corner screws. (The lever on it
    // was black v9–v14, gold v15–v18, silver from v19.)
    // v18: SHARP corners — every plate/frame angle on the unit is
    // a square 90° now; only the hardware stays round.
    {
        const juce::Rectangle<float> plate(cxp - 18.0f, cyp - 25.0f, 36.0f, 50.0f);
        juce::ColourGradient pg(juce::Colour(0xff1b1c1b), plate.getX(), plate.getY(),
                                juce::Colour(0xff070808), plate.getX(), plate.getBottom(), false);
        g.setGradientFill(pg);
        g.fillRect(plate);
        g.setColour(juce::Colour(0xff000000));
        g.drawRect(plate, 1.2f);
        g.setColour(juce::Colour(0x2affffff));
        g.drawLine(plate.getX() + 4.0f, plate.getY() + 1.0f,
                   plate.getRight() - 4.0f, plate.getY() + 1.0f, 1.0f);
        // Tiny corner screws
        for (float sx : { -13.5f, 13.5f })
            for (float sy : { -20.5f, 20.5f }) {
                g.setColour(juce::Colour(0xff454845));
                g.fillEllipse(cxp + sx - 1.7f, cyp + sy - 1.7f, 3.4f, 3.4f);
                g.setColour(juce::Colour(0xcc101110));
                g.drawLine(cxp + sx - 1.1f, cyp + sy - 0.4f,
                           cxp + sx + 1.1f, cyp + sy + 0.4f, 0.8f);
            }
    }

    // Slot recess the lever travels in: deep black, with a faint
    // rim catch down its left edge.
    {
        g.setColour(juce::Colour(0xff020202));
        g.fillRoundedRectangle(cxp - 3.4f, cyp - 20.0f, 6.8f, 40.0f, 3.4f);
        g.setColour(juce::Colour(0x26ffffff));
        g.drawLine(cxp - 3.4f, cyp - 18.0f, cxp - 3.4f, cyp + 18.0f, 0.8f);
    }

    // Collar at the pivot: a dark steel washer.
    {
        juce::ColourGradient wg(juce::Colour(0xff43463f), cxp - 8.0f, cyp - 8.0f,
                                juce::Colour(0xff0e100e), cxp + 8.0f, cyp + 8.0f, false);
        g.setGradientFill(wg);
        g.fillEllipse(cxp - 8.5f, cyp - 8.5f, 17.0f, 17.0f);
        g.setColour(juce::Colour(0x38ffffff));
        g.drawEllipse(cxp - 8.5f, cyp - 8.5f, 17.0f, 17.0f, 1.0f);
        g.setColour(juce::Colour(0x30000000));
        g.drawEllipse(cxp - 5.6f, cyp - 5.6f, 11.2f, 11.2f, 1.2f);
    }

    // The lever (v9): a heavy lever with a rounded teardrop
    // tip, like the reference photo — narrow at the pivot,
    // swelling to a bulb before the rounded end, gloss-lit along
    // its upper-left flank. v9–v14 it was black bakelite; v15 it
    // is GOLD (Nathan — black read cartoonish). Its drawn length
    // foreshortens through the flip (|sgn| -> 0 mid-travel), as
    // before.
    {
        const float len = 7.0f + 12.0f * std::fabs(sgn);
        auto halfW = [](float u) {   // u: 0 at pivot -> 1 at tip
            return 3.0f + 2.5f * std::sin(u * 2.05f) * (1.0f - 0.18f * u);
        };
        auto leverPath = [&](juce::Point<float> off) {
            juce::Path p;
            const int steps = 10;
            for (int i = 0; i <= steps; ++i) {
                const float u = (float)i / (float)steps;
                const float px = cxp - halfW(u) + off.x;
                const float py = cyp + dirY * len * u + off.y;
                if (i == 0) p.startNewSubPath(px, py); else p.lineTo(px, py);
            }
            // Rounded bulb cap
            p.quadraticTo(cxp + off.x, cyp + dirY * (len + 3.2f) + off.y,
                          cxp + halfW(1.0f) + off.x, cyp + dirY * len + off.y);
            for (int i = steps - 1; i >= 0; --i) {
                const float u = (float)i / (float)steps;
                p.lineTo(cxp + halfW(u) + off.x, cyp + dirY * len * u + off.y);
            }
            p.closeSubPath();
            return p;
        };
        // Cast shadow on the plate
        g.setColour(juce::Colour(0x59000000));
        g.fillPath(leverPath({ 1.6f, 2.6f }));
        // The lever body. v9–v14: glossy black bakelite. v15
        // (Nathan): black read "a little too cartoonish" — the
        // lever turned GOLD, shaded in the knob's aged-gold
        // palette. v19 (Nathan): "just make them a silver
        // color" — the lever is SILVER now: real steel/chrome-
        // family shading (bright silver at the light, through
        // a mid steel to a deep gunmetal at the shadow side),
        // light raking in from the upper left so the left
        // flank carries the highlight, exactly as before.
        // Same heavy teardrop form, serious metal.
        juce::ColourGradient bg(juce::Colour(0xfff4f5f7), cxp - 6.0f, cyp - 4.0f,
                                juce::Colour(0xff565b61), cxp + 6.0f, cyp + 6.0f, false);
        bg.addColour(0.35, juce::Colour(0xffb7bcc2));
        bg.addColour(0.72, juce::Colour(0xff83888f));
        g.setGradientFill(bg);
        g.fillPath(leverPath({ 0.0f, 0.0f }));
        // Specular streak down the bulb's light side, and a fainter
        // rim light on the dark side — the two things that make a
        // black lever read on a black panel.
        {
            juce::Path streak;
            for (int i = 1; i <= 9; ++i) {
                const float u = (float)i / 10.0f;
                const float px = cxp - halfW(u) * 0.52f;
                const float py = cyp + dirY * len * u;
                if (i == 1) streak.startNewSubPath(px, py); else streak.lineTo(px, py);
            }
            // A cool sheen down the silver's flank (v19; on
            // the v15–v18 gold this was a warm tint).
            g.setColour(juce::Colour(0x59ffffff));
            g.strokePath(streak, juce::PathStrokeType(1.5f));

            juce::Path rim;
            for (int i = 1; i <= 9; ++i) {
                const float u = (float)i / 10.0f;
                const float px = cxp + halfW(u) * 0.86f;
                const float py = cyp + dirY * len * u;
                if (i == 1) rim.startNewSubPath(px, py); else rim.lineTo(px, py);
            }
            g.setColour(juce::Colour(0x263a3e44));
            g.strokePath(rim, juce::PathStrokeType(0.9f));
        }
        // Pivot cap hides the joint — deep gunmetal, the
        // silver's own shadowed end of the range (v19; was
        // bronze-gold on the gold lever).
        g.setColour(juce::Colour(0xff4a4f55));
        g.fillEllipse(cxp - 2.8f, cyp - 2.8f, 5.6f, 5.6f);
        g.setColour(juce::Colour(0x66ffffff));
        g.drawLine(cxp - 1.6f, cyp - 1.8f, cxp + 0.2f, cyp - 2.2f, 0.9f);
    }
}

// ── Magic-eye retaining bezel (v20, Nathan) ─────────────────
// Until v19 the eye wore a clean silk-white ring — flat pure
// white, vector-perfect — and next to the now-realistic
// hardware it "looks almost like a graphic" (Nathan). The
// ring becomes what it is on the real unit: a brass RETAINING
// BEZEL, gold to match the jewel bezels' gold tone, drawn as
// aged metal: a gold gradient raked from the upper left (as
// the whole panel is lit), its highlight broken into uneven
// arc runs instead of one perfect circular stroke, fine
// scratches worried into the band and a few chips on the
// rim — all deterministic, so the wear never moves. The
// glass, phosphor and hub inside it are untouched.
void drawEyeBezel(juce::Graphics& g, float cx, float cy,
                  float ringR, float glassR)
{
    const float midR = (ringR + glassR) * 0.5f;
    const float bandW = ringR - glassR;

    // Seat shadow under the bezel's outer rim
    g.setColour(juce::Colour(0x80000000));
    g.drawEllipse(cx - ringR - 0.8f, cy - ringR - 0.8f,
                  (ringR + 0.8f) * 2.0f, (ringR + 0.8f) * 2.0f, 1.6f);

    // The gold band: an aged-brass gradient across the ring
    // (the panel's aged-gold anchors), bright at the upper
    // left, deep at the lower right.
    juce::ColourGradient gold(juce::Colour(0xffe9c96e), cx - ringR, cy - ringR,
                              juce::Colour(0xff6e4a10), cx + ringR, cy + ringR, false);
    gold.addColour(0.38, juce::Colour(0xffc08c2a));
    gold.addColour(0.68, juce::Colour(0xff9a6d1e));
    g.setGradientFill(gold);
    g.drawEllipse(cx - midR, cy - midR, midR * 2.0f, midR * 2.0f, bandW);

    // Unevenness: the highlight is NOT one perfect circle —
    // brighter broken runs where the rim catches the light
    // (upper left), duller runs at the lower right, each at a
    // hair different radius, like a turned ring worn by
    // thumbs. (JUCE arc angles run clockwise from 3 o'clock
    // with y down: pi..1.5pi is the upper left.)
    auto arc = [&](float rOff, float wdt, float a0, float a1,
                   juce::Colour col) {
        juce::Path p;
        const float r = midR + rOff;
        p.addArc(cx - r, cy - r, r * 2.0f, r * 2.0f, a0, a1, true);
        g.setColour(col);
        g.strokePath(p, juce::PathStrokeType(wdt));
    };
    arc( 0.5f, 1.9f, 0.82f * kPi, 1.30f * kPi, juce::Colour(0x8cffe9b0));
    arc( 0.2f, 1.5f, 1.38f * kPi, 1.62f * kPi, juce::Colour(0x59ffe9b0));
    arc(-0.4f, 2.1f, 0.08f * kPi, 0.55f * kPi, juce::Colour(0x663d2c07));
    arc(-0.9f, 1.1f, 0.60f * kPi, 0.95f * kPi, juce::Colour(0x4d3d2c07));

    // Fine scratches worried into the band: short strokes
    // running mostly WITH the ring (turning marks and
    // handling scratches follow the circle), pale where they
    // cut to bright metal, dark where they hold dirt.
    PanelRand rng(0x5eed0be7u);
    for (int i = 0; i < 34; ++i) {
        const float aDeg = rng.range(0.0f, 360.0f);
        const float rr = midR + rng.range(-bandW * 0.36f, bandW * 0.36f);
        const float len = rng.range(1.5f, 7.5f);
        const auto p0 = polar(cx, cy, rr, aDeg);
        const auto p1 = polar(cx, cy, rr + rng.range(-0.5f, 0.5f),
                              aDeg + len * 57.2958f / juce::jmax(8.0f, rr));
        if (rng.next() < 0.55f)
            g.setColour(juce::Colour::fromRGBA(240, 214, 150,
                        (uint8_t)rng.range(22.0f, 60.0f)));
        else
            g.setColour(juce::Colour::fromRGBA(24, 15, 3,
                        (uint8_t)rng.range(24.0f, 56.0f)));
        g.drawLine(p0.x, p0.y, p1.x, p1.y, 0.8f);
    }
    // A few chips on the rim: tiny dark bites, the first
    // with a bright lip where the metal turned over.
    for (int i = 0; i < 3; ++i) {
        const float aDeg = rng.range(0.0f, 360.0f);
        const float rr = midR + rng.range(-bandW * 0.3f, bandW * 0.3f);
        const auto p = polar(cx, cy, rr, aDeg);
        const float r = rng.range(0.7f, 1.4f);
        g.setColour(juce::Colour(0xb014100a));
        g.fillEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f);
        if (i == 0) {
            g.setColour(juce::Colour(0x59ffe9b0));
            g.drawEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f, 0.6f);
        }
    }

    // Inner seat: a dark line where the bezel meets the glass.
    g.setColour(juce::Colour(0xcc000000));
    g.drawEllipse(cx - glassR - 0.4f, cy - glassR - 0.4f,
                  (glassR + 0.4f) * 2.0f, (glassR + 0.4f) * 2.0f, 1.0f);
}

// ─────────────────────────────────────────────────────────────────────
// Magic eye — live gain reduction, drawn like a real 1629 tube.
//   0 dB GR  -> the whole disc glows phosphor green around a small
//               dark round centre hub (see the lit CT-1 reference).
//   GR builds-> black eats into the green from the top, as if the
//               green were being pulled out of the eye: the lit
//               region shrinks back toward 6 o'clock.
//   ~24 dB   -> nearly all black; only a thin green sliver and the
//               dark hub remain.
// ─────────────────────────────────────────────────────────────────────
void Osc72Editor::drawMagicEye(juce::Graphics& g, juce::Rectangle<float> bounds,
                               float grDb, bool dark)
{
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float ringR = bounds.getWidth() * 0.5f;      // ~58 for v6
    const float glassR = ringR - 6.0f;   // bezel inset (eye is 70 px across at v16)

    // v19: POWER off — a dead tube. No phosphor glow, no green,
    // no blade edges, no glass glint: just the dead black, the
    // gold bezel (v20; hardware doesn't go anywhere) and the
    // hub. The draws below are skipped by
    // forcing the lit fraction to zero AND gating the green
    // sector/glow/reflection blocks on !dark (at grDb alone the
    // eye can never be fully dark — 0 dB is full green).
    const float t = dark ? 1.0f : juce::jlimit(0.0f, 1.0f, grDb / 24.0f);
    // Half-angle of the remaining green, about 6 o'clock. Eased so a
    // clear bite is gone by ~10 dB and a sector is all that's left
    // by ~21 dB.
    const float greenHalf = juce::jmax(4.0f, 175.0f * std::pow(1.0f - t, 0.85f));
    const float greenFrac = greenHalf / 175.0f;        // for glow/dim

    // Phosphor glow spilling onto the panel — dies away as the
    // green is pulled out of the eye. (Skipped when dark: a
    // dead tube spills nothing.)
    if (!dark) {
        const auto glowCol = juce::Colour(0x4646ff7d)
                                 .withAlpha(0.30f * greenFrac);
        juce::ColourGradient glow(glowCol, cx, cy,
                                  juce::Colour(0x0046ff7d), cx, cy + ringR * 1.35f, true);
        g.setGradientFill(glow);
        const float gr = ringR * 1.35f;
        g.fillEllipse(cx - gr, cy - gr, gr * 2.0f, gr * 2.0f);
    }

    // Glass disc, clipped
    juce::Path glass;
    glass.addEllipse(cx - glassR, cy - glassR, glassR * 2.0f, glassR * 2.0f);
    g.saveState();
    g.reduceClipRegion(glass);

    // Dead tube black underneath everything
    {
        juce::ColourGradient dead(juce::Colour(0xff0a0f0b), cx, cy - glassR * 0.5f,
                                  juce::Colour(0xff020302), cx, cy + glassR * 0.7f, false);
        g.setGradientFill(dead);
        g.fillRect(bounds);
    }
    // The surviving green, clipped to the shrinking bottom sector.
    // (Skipped when dark — no green at all in a dead tube.)
    if (!dark) {
        g.saveState();
        g.reduceClipRegion(bottomFanPath(cx, cy, glassR + 2.0f, greenHalf));
        juce::ColourGradient phos(juce::Colour(0xff86ffa6), cx, cy + glassR * 0.30f,
                                  juce::Colour(0xff0d9c3c), cx, cy - glassR * 1.15f, true);
        phos.addColour(0.42, juce::Colour(juce::Colour(0xff3bea66)));
        // The phosphor also dims a little as it is pulled out.
        g.setGradientFill(phos);
        g.setOpacity(1.0f - 0.22f * t);
        g.fillRect(bounds);
        g.setOpacity(1.0f);
        // Faint radial phosphor rays inside the lit region
        g.setColour(juce::Colour(0x10ffffff));
        for (int i = 0; i < 24; ++i) {
            const auto pt = polar(cx, cy, glassR, (float)i * 15.0f);
            g.drawLine(cx, cy, pt.x, pt.y, 1.0f);
        }
        g.restoreState();
    }
    // Soft bright edges where the black blades cut into the green
    if (!dark && greenHalf > 6.0f && greenHalf < 172.0f) {
        g.setColour(juce::Colour(0x70c9ffd9));
        for (float sgn : { -1.0f, 1.0f }) {
            const auto pt = polar(cx, cy, glassR, 180.0f + sgn * greenHalf);
            g.drawLine(cx, cy, pt.x, pt.y, 1.6f);
        }
    }
    g.restoreState();

    // Inner bezel shade — a soft shadow where the glass sinks
    // below the retaining ring.
    g.setColour(juce::Colour(0x59000000));
    g.drawEllipse(cx - glassR, cy - glassR, glassR * 2.0f, glassR * 2.0f, 3.0f);
    // The retaining bezel (v20): aged GOLD hardware, scratched
    // and uneven — see drawEyeBezel. (v18–v19 drew a clean
    // silk-white ring here; Nathan: it "looks almost like a
    // graphic" next to the real hardware.)
    drawEyeBezel(g, cx, cy, ringR, glassR);

    // Dark round centre hub — always present, lit or not (the
    // metal cap at the heart of the real tube's target). v11: its
    // screw is an ALLEN head — a hexagonal socket recess at the
    // hub's centre, near-black inside, its lower-right inner walls
    // catching a faint rim of light so it reads as a recess.
    {
        const float hr = glassR * 0.21f;
        juce::ColourGradient hub(juce::Colour(0xff39423c), cx - hr, cy - hr,
                                 juce::Colour(0xff05070a), cx + hr, cy + hr, false);
        g.setGradientFill(hub);
        g.fillEllipse(cx - hr, cy - hr, hr * 2.0f, hr * 2.0f);
        g.setColour(juce::Colour(0x55cfe8d2));
        g.drawEllipse(cx - hr, cy - hr, hr * 2.0f, hr * 2.0f, 1.0f);
        // Hex socket: vertices on the polar dial (0° = top).
        const float hexR = hr * 0.46f;
        juce::Point<float> vtx[6];
        for (int k = 0; k < 6; ++k)
            vtx[k] = polar(cx, cy, hexR, (float)k * 60.0f);
        juce::Path hex;
        hex.startNewSubPath(vtx[0].x, vtx[0].y);
        for (int k = 1; k < 6; ++k) hex.lineTo(vtx[k].x, vtx[k].y);
        hex.closeSubPath();
        g.setColour(juce::Colour(0xff060806));
        g.fillPath(hex);
        // Facet light: edges whose midpoint sits on the lower /
        // right of the dial (90°–210°) are the recess walls that
        // face the upper-left light — stroke them faintly; the
        // opposite walls go darker, deepening the socket.
        for (int k = 0; k < 6; ++k) {
            const auto p1 = vtx[k], p2 = vtx[(k + 1) % 6];
            const float mid = (float)k * 60.0f + 30.0f;
            if (mid >= 80.0f && mid <= 220.0f) {
                g.setColour(juce::Colour(0x59cfe8d2));
                g.drawLine(p1.x, p1.y, p2.x, p2.y, 1.0f);
            } else {
                g.setColour(juce::Colour(0xa0000000));
                g.drawLine(p1.x, p1.y, p2.x, p2.y, 1.0f);
            }
        }
    }

    // Glass reflection (skipped when dark — no glint on a
    // dead tube)
    if (!dark) {
        juce::ColourGradient refl(juce::Colour(0x2effffff), cx - ringR * 0.42f, cy - ringR * 0.58f,
                                  juce::Colour(0x00ffffff), cx - ringR * 0.06f, cy - ringR * 0.20f, true);
        g.setGradientFill(refl);
        g.fillEllipse(cx - ringR * 0.58f, cy - ringR * 0.72f,
                      ringR * 0.84f, ringR * 0.44f);
    }
}

// ─────────────────────────────────────────────────────────────────────
Osc72Editor::Osc72Editor(Osc72Processor& p) : AudioProcessorEditor(&p), proc_(p)
{
    setSize(352, 253);    // v18: v17's 320x230 + ~10% (Nathan); controls/markings grow x1.5-2 on top of that
    displayedGrDb_ = proc_.currentGainReductionDb();
    knob_.setLookAndFeel(&laf_);
    knob_.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob_.setRotaryParameters(kPi * 1.25f, kPi * 2.75f, true);
    addAndMakeVisible(knob_);
    att_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc_.apvts, "allTheWay", knob_);
    addAndMakeVisible(limitToggle_);
    limitAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc_.apvts, "limit", limitToggle_);
    addAndMakeVisible(modToggle_);
    modAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc_.apvts, "warble", modToggle_);
    // POWER toggle (v19): the strip's third BatToggle — a full
    // switch cluster matching LIMIT/MOD — bound to 'power'.
    addAndMakeVisible(powerToggle_);
    powerAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc_.apvts, "power", powerToggle_);
    // O/P LEVEL knobs (v12): small black rotaries bound to the
    // outLevelL/outLevelR trims; double-click returns to 0 dB.
    for (juce::Slider* s : { &outLevelL_, &outLevelR_ }) {
        s->setLookAndFeel(&smallLaf_);
        s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters(kPi * 1.25f, kPi * 2.75f, true);
        s->setDoubleClickReturnValue(true, 0.0);
        addAndMakeVisible(*s);
    }
    outAttL_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc_.apvts, "outLevelL", outLevelL_);
    outAttR_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc_.apvts, "outLevelR", outLevelR_);
    startTimerHz(30);
}

Osc72Editor::~Osc72Editor()
{
    stopTimer();
    knob_.setLookAndFeel(nullptr);
    outLevelL_.setLookAndFeel(nullptr);
    outLevelR_.setLookAndFeel(nullptr);
}

void Osc72Editor::timerCallback()
{
    const float target = proc_.currentGainReductionDb();
    const float cur = displayedGrDb_;
    // Analog-feeling ballistics: fast attack (~40 ms), slower release.
    const float coef = target > cur ? 0.55f : 0.10f;
    displayedGrDb_ = cur + (target - cur) * coef;
    if (std::abs(displayedGrDb_ - cur) > 0.02f)
        repaint(eyeArea_);
    if (limitToggle_.advanceAnimation())
        repaint(limitToggle_.getBounds());
    if (modToggle_.advanceAnimation())
        repaint(modToggle_.getBounds());
    if (powerToggle_.advanceAnimation())
        repaint(powerToggle_.getBounds());
    // v19: a POWER flip changes every light on the unit (all
    // three jewels + the eye) — repaint the whole panel.
    const bool pwr = powerToggle_.getToggleState();
    if (pwr != lastPowerState_) {
        lastPowerState_ = pwr;
        repaint();
    }
    // v15: the section jewels are editor paint — repaint their
    // areas when a toggle state flips so the lights track the
    // levers. (v19: the POWER jewel no longer follows LIMIT —
    // it IS the power light, lit while 'power' is on.)
    const bool lim = limitToggle_.getToggleState();
    const bool mod = modToggle_.getToggleState();
    if (lim != lastLimitState_) {
        lastLimitState_ = lim;
        repaint(juce::Rectangle<int>(12, 24, 38, 37));    // LIMIT jewel, centre device (31, 42) — v31: out with its cluster (v30: (20, 24, 38, 37); v19-v29: (6, 206, 40, 39))
    }
    if (mod != lastModState_) {
        lastModState_ = mod;
        repaint(juce::Rectangle<int>(193, 24, 38, 37));   // MOD jewel, centre device (212, 42) — v31: out with its cluster (v30: (185, 24, 38, 37); v19-v29: (196, 206, 40, 39))
    }
    // v10: the flange numerals rotate with the knob, but the
    // flange is editor paint — the Slider only repaints its own
    // bounds — so repaint the dial area when the value moves.
    const float prop =
        (float)knob_.valueToProportionOfLength(knob_.getValue());
    if (std::abs(prop - lastFlangeProp_) > 0.0005f) {
        lastFlangeProp_ = prop;
        repaint(juce::Rectangle<int>(60, 104, 122, 128)); // dial area (flange + index mark) — v25: knob down, flange R 100
    }
}

void Osc72Editor::resized()
{
    // v19 rack layout (device px; the panel paint is authored
    // in the 640x460 virtual space under a 0.55 transform —
    // see paint()). v19 moves the LIMIT/MOD clusters up and
    // outward off the bottom edge (LIMIT up+left, MOD
    // up+right) and adds the POWER toggle on the strip, a
    // third cluster at the same component size. Centres below
    // are given in device px with their virtual equivalents.
    // v25: knob centre down (virtual (220, 300) -> (220,
    // 310)); POWER toggle down a hair (virtual centre y
    // 176 -> 180); O/P pots apart (virtual centres x
    // 505/575 -> 496/584) and bigger (23 -> 27 device px);
    // eye repaint area follows the eye up (centre virtual
    // (220, 98) -> (220, 77)).
    // v26: the eye stack settles back DOWN partway (centre
    // virtual (220, 77) -> (220, 86)) and the POWER toggle
    // descends a few px further with it (virtual centre
    // y 180 -> ~185.5; the silk cluster line is 186).
    // v27: centreline alignments — the POWER toggle's
    // centre rises onto the eye-hub line (virtual y 86;
    // bounds y 87 -> 32, the same half-px rounding as
    // v26) and the O/P pots' centres rise onto the input
    // knob's centre line (virtual y 310; bounds y 197
    // -> 157).
    // v29 addition: EVERYTHING rises 10 virtual px —
    // each assembly as a whole (eye + caption, input
    // stack, LIMIT/MOD clusters, POWER cluster, OUTPUT
    // section) so the new bottom wordmark bands get
    // room and the switches lift off the panel bottom.
    // The silk moves the full 10; the components follow
    // by 5 device px (10 virtual = 5.5 device at the
    // 0.55 scale — the rounding every round carries),
    // so both centreline alignments (POWER toggle <->
    // eye hub, O/P pots <-> input knob) hold as built.
    // The bottom wordmarks do NOT move — they stay
    // anchored to the screws (see paint()).
    // v30 (EXPERIMENTAL layout trial): the LIMIT/MOD
    // toggle components relocate UP to flank the magic
    // eye, their centres on the eye-hub line — bounds
    // (36,205)/(184,205) -> (48,27)/(172,27). All three
    // toggle components now share device centre row
    // y 42 (virtual 76.4 — the half-px rounding every
    // round carries against the silk's y 76), so the
    // three toggle centres and the eye hub sit on one
    // horizontal line; the silk follows in paint().
    // v31 (Nathan): the flanking pair nudges OUTWARD a
    // little — LIMIT left, MOD right, 15 virtual px of
    // silk (8 device px of component: 15 x 0.55 = 8.25,
    // the standing rounding) — bounds (48,27)/(172,27)
    // -> (40,27)/(180,27); centres stay on the y-42 row,
    // so the one-line alignment with POWER and the eye
    // hub is untouched.
    knob_.setBounds(74, 119, 94, 94);               // centre (121, 166) = virtual (220, 302)
    limitToggle_.setBounds(40, 27, 22, 30);         // centre (51, 42) = virtual (93, 76) — v31: nudged out (v30: (59, 42) = virtual (107, 76))
    modToggle_.setBounds(180, 27, 22, 30);          // centre (191, 42) = virtual (347, 76) — v31: nudged out (v30: (183, 42) = virtual (333, 76))
    powerToggle_.setBounds(286, 27, 22, 30);        // centre (297, 42) = virtual (540, 76) — on the eye-hub line (76)
    outLevelL_.setBounds(259, 152, 27, 27);         // centre (272.5, 165.5) = virtual (495, 301) — on the knob-centre line
    outLevelR_.setBounds(308, 152, 27, 27);         // centre (321.5, 165.5) = virtual (584, 301) — on the knob-centre line
    eyeArea_ = juce::Rectangle<int>(74, 0, 96, 86);  // the eye + its glow, centre device (122, 43) — follows the eye (v29: up with the rise)
}

void Osc72Editor::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds();
    // v18: the panel is still authored in the 640x460 virtual
    // space, now drawn under a 0.55 scale transform — the
    // window is 352x253 (v17 + ~10%). w/h are the VIRTUAL
    // dimensions. Within that space the v18 re-layout grows
    // every control and marking ~x1.5–2 over its v16 size, so
    // the hardware dominates the panel (see the sections
    // below for the new geometry).
    g.addTransform(juce::AffineTransform::scale(0.55f));
    const float w = (float)b.getWidth() / 0.55f, h = (float)b.getHeight() / 0.55f;

    // Edge-wear level for the gloss-black finish: the wear
    // lives at the plates' edges and corners only (the
    // faces stay clean gloss). The snapshot's beaten-up
    // variant knocks the edges around harder.
    const float edgeSev = osc72gui::beatenUp() ? 1.2f : 0.85f;

    // v19: the unit's power state drives every light in this
    // paint — the three jewels and the magic eye.
    const bool powerOn = powerToggle_.getToggleState();

    // ── v18 geometry ────────────────────────────────────────────
    // FLUSH (kept from v15): two plates in one plane fill the
    // whole face, SHARP-cornered (v18). The compressor module
    // is the left 440 px; the utility strip the right 200 px;
    // they share the seam at x=440 and ONE MATERIAL — now the
    // gloss black of paintGlossBlack(), with scratched edges.
    const juce::Rectangle<float> module(0.0f, 0.0f, 440.0f, h);
    const juce::Rectangle<float> strip(440.0f, 0.0f, w - 440.0f, h);  // x 440..640
    const float mcx = module.getCentreX();                           // 220
    const float scx = strip.getCentreX();                            // 540

    // ── The shared GLOSS BLACK (v18: ONE material — smooth
    //    gloss black with a soft sheen, no brush grain, no
    //    face weathering). The face is filled first; each
    //    plate is then drawn over its own rect.
    paintGlossBlack(g, juce::Rectangle<float>(0.0f, 0.0f, w, h), h);

    // The module plate, FLUSH (v15) and SHARP-cornered (v18):
    // the same gloss black, its edges scratched and corners
    // knocked (drawScratchedEdges), its FACES clean — no
    // scratches across the face (Nathan, v16, still in force).
    // No cast shadow — it stands in the same plane as the strip.
    {
        paintGlossBlack(g, module, h);
        drawScratchedEdges(g, module, 0x5eed0001u, edgeSev);
        // Plate edge: a dark seat line around the plate, then a
        // hairline of top light along its top edge — gloss
        // catching the light on the rim.
        g.setColour(juce::Colour(0xff060706));
        g.drawRect(module, 2.0f);
        g.setColour(juce::Colour(0x38ffffff));
        g.drawLine(module.getX() + 2.0f, module.getY() + 1.2f,
                   module.getRight() - 2.0f, module.getY() + 1.2f, 1.2f);
    }
    // The module's four bolts — Phillips, as on the v12 panel
    // corners, each landed at its own angle, inset tight to the
    // plate's corners now that the plate IS the perimeter
    // (v18: a touch larger, with the hardware growth).
    drawPhillipsScrew(g, module.getX() + 18.0f, module.getY() + 18.0f, 9.0f, 24.0f);
    drawPhillipsScrew(g, module.getRight() - 18.0f, module.getY() + 18.0f, 9.0f, 97.0f);
    drawPhillipsScrew(g, module.getX() + 18.0f, module.getBottom() - 18.0f, 9.0f, 140.0f);
    drawPhillipsScrew(g, module.getRight() - 18.0f, module.getBottom() - 18.0f, 9.0f, 63.0f);

    // ── Module TOP band (v29, Nathan): BARE — no text at
    //    the top of the plate at all now, just the four
    //    corner screws. The wordmarks that rode the top
    //    screw line through v19–v28 ('BITEY' at one top
    //    corner anchor, 'WESTERN COMPRESSOR' at the other;
    //    sides swapped in v28) both move to the BOTTOM of
    //    their plates in this round: 'WESTERN COMPRESSOR'
    //    centred between this module's bottom screws
    //    (drawn with the module's bottom silk, below) and
    //    'BITEY' centred between the strip's bottom screws
    //    (drawn with the strip, below). The top-band
    //    history lives in build-reports v19–v28.

    // ── Module: magic eye + its caption ───────────────────────────
    // v18: the eye grows to 110 px across (virtual), upper-centre
    // of the module. v19: with POWER off the eye is a dead tube
    // (dark).
    // v24 (Nathan): the whole eye assembly nudges UP a hair —
    // top edge 48 -> 43 (5 virtual px), centre (220, 103) ->
    // (220, 98), horizontal centre unchanged.
    // v25 (Nathan): the eye rises FOR REAL — top edge 43 ->
    // 22, centre (220, 98) -> (220, 77) — but not hard
    // against the wordmark: the POWER cluster drops a hair
    // in the same round so the caption/POWER alignment is
    // met in the middle, and the bezel keeps a comfortable
    // air under 'WESTERN COMPRESSOR' (audited circle-exact).
    // v26 (Nathan): "gain reduction and magic eye can come
    // back down a little bit" — the stack settles partway
    // back: top edge 22 -> 31, centre (220, 77) -> (220,
    // 86), about 40% of the v24->v25 rise returned. The
    // caption rides with it (baseline 150 -> 159, below);
    // the input stack does not move.
    // v29 addition: the eye rises 10 with everything
    // (top 31 -> 21, centre (220, 86) -> (220, 76));
    // the caption rides with it (159 -> 149).
    drawMagicEye(g, juce::Rectangle<float>(mcx - 55.0f, 21.0f, 110.0f, 110.0f),
                 displayedGrDb_, !powerOn);
    g.setColour(kSilk);
    // v19 text sizes (VIRTUAL points; they land at x0.55 on
    // screen): ONE label size panel-wide — 16.5, the v18
    // LIMIT/MOD function-word size. GAIN REDUCTION comes DOWN
    // from 20 to join POWER and OUTPUT at 16.5 (Nathan).
    // IN/OUT / ON/OFF and L/R stay 16 (subordinate markings);
    // flange numerals are 15.5 on their shrunken ring (below).
    // v21 (Nathan): EVERYTHING shrinks by about a third and
    // the tracking tightens to near-natural — labels 16.5 ->
    // 11 with tracking 0.6, subordinate markings 16 -> 10.5
    // with tracking 0.3, flange numerals 15.5 -> 10.5 (see
    // the flange section). At the new size the caption's ink
    // spans roughly x 181..259 — the BITEY wordmark at the
    // left now ends around x 60, so the v20 crowding of
    // wordmark into caption is gone with room to spare.
    // v22 (Nathan): the spray treatment is deleted (all text
    // is direct vector again — see drawTrackedText) and the
    // sizes step down once more while the face STRETCHES
    // 1.2x in capsFont: labels 11 -> 10 (tracking 0.5),
    // markings 10.5 -> 9.5 (tracking 0.3). The flange
    // numerals go the other way — 10.5 -> 16 — for
    // visibility on the black knob (see the flange section).
    // v23 (Nathan): sizes come back UP ~30% and the stretch
    // widens — labels 10 -> 13 (tracking 0.6), markings
    // 9.5 -> 12.5 (tracking 0.4), stretch 1.2x -> 1.5x —
    // in the bolder SANCREEK face (see panelTypeface).
    // v24: the face is SMOKUM EMBOLDENED (see
    // panelTypeface); every size, the stretch and the
    // tracking carry over from v23 unchanged.
    // v19 (Nathan): the caption RELOCATED to directly ABOVE
    // the eye. It was drawn after the eye so the phosphor
    // glow never washed over it.
    // v24 (Nathan): the caption moves back UNDER the eye,
    // reversing v19 — baseline 169, ink ~159..170, centred
    // on the module. With the eye nudged up (bezel ink
    // bottom ~154.6) that leaves ~4.4 virtual px (~2.4
    // device px) above the ink to the bezel, and INPUT's
    // ink (top ~174 on its 184 baseline) sits the same
    // ~4.4 px below it: eye / caption / INPUT / arrow /
    // knob, each with its own air. It is still drawn
    // after the eye, so the phosphor glow — which reaches
    // past the caption's band — never washes over it.
    // The raised 'WESTERN COMPRESSOR' wordmark lives on
    // the top screw line (ink ~13..24), nowhere near
    // this band.
    // v25 (Nathan): the caption rides up with the eye —
    // baseline 169 -> 150 — so its bottom lands roughly in
    // line with the strip's POWER word (which drops 142 ->
    // 146 in the same round; the two bottoms sit ~4 virtual
    // px apart). Bezel-to-caption air is preserved (the eye
    // rose 21, the caption 19).
    // v26: the caption comes back down WITH the eye —
    // baseline 150 -> 159, the same +9 — so the
    // bezel-to-caption air is again preserved exactly, and
    // the caption-to-INPUT daylight below it grows by the
    // same 9 virtual px.
    // v29 addition: the caption rises 10 with the eye
    // (159 -> 149) and INPUT rises 10 with the knob
    // (190 -> 180), so the caption/INPUT daylight is
    // unchanged; the whole stack gains air toward the
    // bare top band and the wordmark band below.
    drawTrackedText(g, "GAIN REDUCTION", capsFont(14.5f), 0.6f,
                    mcx, 149.0f, Anchor::centre);

    // ── Module: knob skirt flange + numerals (v9; rotating v10) ──
    // From Nathan's reference photo (a vintage decade box): the
    // knob carries a WIDE SKIRT FLANGE printed with the scale.
    // There is NO pointer — the flange and its silk-white
    // numerals 0–10 ROTATE WITH THE KNOB, and the value is the
    // numeral that arrives under the fixed index mark on the
    // plate at 12 o'clock (drawn below). (The gold annulus is
    // rotationally symmetric, so only its print moves; its
    // lighting stays panel-fixed, as room light would.)
    // v25 (Nathan): the whole input stack — knob, flange,
    // INPUT, index triangle — drops a little (centre y 300
    // -> 310) to open clear space between the GAIN
    // REDUCTION caption and INPUT; the flange also WIDENS
    // (R 95 -> 100), the numeral ring rides out with it
    // (R 87 -> 90, the middle of the wider 80..100 annulus).
    // v29 addition: the whole stack rises 10 (centre
    // y 310 -> 300); the index triangle's offsets ride
    // with the centre, unchanged relative to the rim.
    const juce::Point<float> knobC(mcx, 300.0f);
    {
        const auto pal = knobPalette();
        const float flangeR = 100.0f;   // v18: 95 (the dial grows ~x1.6 in virtual space); v25: 100
        // Soft shadow grounding the flange on the plate
        g.setColour(juce::Colour(0x55000000));
        g.fillEllipse(knobC.x - flangeR, knobC.y - flangeR + 6.0f,
                      flangeR * 2.0f, flangeR * 2.0f);
        // Flange body: the knob's gold, a touch deeper than its
        // face, light falling from the top.
        juce::ColourGradient fg(pal.faceMid, knobC.x, knobC.y - flangeR,
                                pal.faceDeep, knobC.x, knobC.y + flangeR, false);
        fg.addColour(0.30, pal.faceHi);
        g.setGradientFill(fg);
        g.fillEllipse(knobC.x - flangeR, knobC.y - flangeR,
                      flangeR * 2.0f, flangeR * 2.0f);
        // Glossy sheen raking across the upper right of the annulus
        {
            juce::Path disc;
            disc.addEllipse(knobC.x - flangeR, knobC.y - flangeR,
                            flangeR * 2.0f, flangeR * 2.0f);
            g.saveState();
            g.reduceClipRegion(disc);
            juce::ColourGradient sheen(juce::Colour(0x2effffff), knobC.x + 57.0f, knobC.y - 24.0f,
                                       juce::Colour(0x00ffffff), knobC.x + 12.0f, knobC.y - 114.0f, true);
            g.setGradientFill(sheen);
            g.fillEllipse(knobC.x - 6.0f, knobC.y - flangeR - 18.0f, 114.0f, 102.0f);
            g.restoreState();
        }
        // Rim, and the dark groove the rotating knob emerges from
        g.setColour(pal.outline);
        g.drawEllipse(knobC.x - flangeR, knobC.y - flangeR,
                      flangeR * 2.0f, flangeR * 2.0f, 2.0f);
        g.setColour(juce::Colour(0xa0000000));
        g.drawEllipse(knobC.x - 80.0f, knobC.y - 80.0f, 160.0f, 160.0f, 2.5f);
        g.setColour(juce::Colour(0x30ffffff));
        g.drawEllipse(knobC.x - 78.5f, knobC.y - 78.5f, 157.0f, 157.0f, 1.2f);
        // Numerals 0–10: pure white (v16), each rotated so its
        // head points outward along its radius, centred on the
        // flange band — and the whole set rotates with the knob.
        // Numeral v's local angle is 225° + 27°·v; the flange
        // rotation is the negative of the control's sweep angle
        // (225° + 270°·pos), so the numeral equal to the current
        // value lands at 12 o'clock under the fixed mark.
        // v19 (Nathan): at the v18 size the numerals FLOAT OUT
        // past the flange's edge — the ring shrinks (R 88 -> 87)
        // and the face comes down (17 -> 15.5) so every glyph
        // sits fully ON the annulus, between the knob's groove
        // (R 80) and the flange rim (R 95), as large as that
        // band honestly allows.
        // v21 (Nathan): the numerals "have gotten too dim —
        // back to being white, and even a little bit smaller".
        // The face came down again (15.5 -> 10.5) and the
        // spray treatment left their cores at full pure white.
        // v22 (Nathan): v21's numerals went too far the other
        // way — "weirdly shrunken", hard to see, and ringed
        // by the grey spray-underlay boxes that spilled past
        // the flange edge and into the flutes. The underlay
        // is DELETED with the rest of the spray path: the
        // numerals are ONE pass of direct vector text, and
        // the face goes back UP (10.5 -> 16, stretched like
        // all the lettering) so they read clearly on the
        // black flange — glyph height ~10 px centred on R 87
        // keeps them fully on the annulus (groove R 80, rim
        // R 95) with air on both sides, and the rect is just
        // the text's layout box, never drawn.
        // v23 (Nathan): the numerals "are a better size
        // height-wise but they should be stretched" — the
        // height STAYS 16 pt (measured ink height 8.8
        // virtual px, v22's 8.7) while the face becomes the
        // bolder SANCREEK and the stretch widens to 1.5x:
        // the digits carry roughly twice the ink of v22's
        // (the parked-at-bottom '0' measures 65 ink px
        // against v22's 35 in the 2x render) on the same
        // annulus, ink r 82.5..91.3.
        // v24: the face is SMOKUM EMBOLDENED at the same
        // 16 pt and the same 1.5x stretch (see
        // panelTypeface) — the height brief is unchanged,
        // the weight now lives in the outlines.
        // v25 (Nathan): the numerals get a hair TALLER
        // (16 -> 17.5) and stretch a bit more (1.65x, in
        // numeralFont) on the wider annulus — ring R 87 ->
        // 90 — and they KEEP the v24-weight bake: the
        // thinner v25 rebake is for panel text only.
        // v26 (Nathan): "all fonts" — the numerals take
        // the XThin bake too (numeralTypeface now loads
        // the same 1.42%-of-em file); size, stretch and
        // ring are unchanged from v25.
        const float knobProp =
            (float)knob_.valueToProportionOfLength(knob_.getValue());
        const float flangeRot = -(225.0f + 270.0f * knobProp);
        for (int v = 0; v <= 10; ++v) {
            const float aDeg = 225.0f + 27.0f * (float)v + flangeRot;
            const auto pt = polar(knobC.x, knobC.y, 90.0f, aDeg);
            const juce::Rectangle<float> nrect(pt.x - 20.0f, pt.y - 12.0f,
                                               40.0f, 24.0f);
            g.saveState();
            g.addTransform(juce::AffineTransform::rotation(
                aDeg * kPi / 180.0f, pt.x, pt.y));
            g.setColour(kSilk);
            g.setFont(numeralFont(17.5f));
            g.drawText(juce::String(v), nrect,
                       juce::Justification::centred, false);
            g.restoreState();
        }
    }

    // ── The main pot's label (v19, Nathan): 'INPUT' ─────────────
    // The word INPUT, at the unified label size, centred
    // directly above the fixed index triangle — the stack reads
    // eye / caption / INPUT / arrow / knob (the caption moved
    // under the eye in v24). It sits where the
    // GAIN REDUCTION caption used to live before v19 moved it
    // above the eye. v20 (Nathan): it drops DOWN to sit almost
    // right on top of the arrow — baseline 188, glyphs
    // ~176..188, just 3 px of air above the triangle's top
    // edge at 191 (and still clear of the eye's bottom edge
    // at 158). v24: the eye's bottom edge is 153 now and the
    // GAIN REDUCTION caption sits between eye and INPUT —
    // its ink ends ~168, ~6 virtual px above INPUT's ink
    // top. v21: the baseline settles at 184 — the measured
    // ink (glyph bottoms run ~3 px under the baseline in this
    // face) keeps ~4 virtual px = ~2.2 device px of air above
    // the triangle: still "almost on top of it", per the v21
    // overlap audit, but never touching.
    // v25: INPUT moves with the stack (baseline 184 ->
    // 194 with the knob's +10) but gives 4 back (190) —
    // the wider flange's index triangle rides 5 higher
    // relative to the knob centre, and INPUT keeps its
    // audited air above the triangle. The caption above
    // it now ends ~151, so the separation Nathan asked
    // for (caption vs INPUT) opens right up.
    g.setColour(kSilk);
    drawTrackedText(g, "INPUT", capsFont(14.5f), 0.6f,
                    knobC.x, 180.0f, Anchor::centre);

    // ── Fixed index mark (v10) ───────────────────────────────────
    // ONE small pure-white triangle (v16: brightest white, like
    // all the markings) printed on the module at 12 o'clock,
    // just outside the flange's top edge. It never moves: the
    // numeral rotated under it is the knob's value.
    {
        // v25: the flange is R 100 now, so the triangle
        // rides 5 higher relative to the centre (-114 /
        // -103) to keep its v24 relationship to the rim
        // (base 14 above it, apex 3 above it).
        juce::Path tri;
        tri.addTriangle(knobC.x - 5.5f, knobC.y - 114.0f,
                        knobC.x + 5.5f, knobC.y - 114.0f,
                        knobC.x,        knobC.y - 103.0f);
        g.setColour(kSilk);
        g.fillPath(tri);
    }

    // ── No text label for the knob (Nathan: no "ALL THE WAY" text
    //    anywhere — the numbered flange is the readout). ──────────

    // ── Module: toggles' silk-screen (v8): function word
    //    above each bat, 'in'/'out' small beside its throws on the
    //    module-centre side (in = up). The bats themselves are
    //    components (limitToggle_/modToggle_). ────────────────────
    g.setColour(kSilk);
    // (Function words sit ABOVE the toggle escutcheons, and
    // the components paint over the editor's silk, so the
    // word baselines keep the glyphs clear of the plates.)
    // v19 (Nathan): the clusters move UP and OUTWARD off the
    // bottom edge — LIMIT centre (96,428) -> (86,410), MOD
    // centre (344,428) -> (354,410), the same shift for both.
    // v21 audit nudges: LIMIT/MOD baselines 377 -> 373 and
    // the IN/OUT anchors 2 px further out (112 -> 114,
    // 328 -> 326), so the measured ink keeps >= 2 device px
    // of air from the escutcheon plates on every side.
    // v29 addition: the clusters rise 10 as whole
    // assemblies — words 373 -> 363, IN/OUT 407/425 ->
    // 397/415, the section jewels 410 -> 400, and the
    // toggle components follow in resized() — lifting
    // the plates visibly off the bottom wordmark band.
    // v30 (Nathan, EXPERIMENTAL — "let's try one where…"):
    // the two clusters RELOCATE UP to FLANK THE MAGIC EYE.
    // Each cluster moves as a whole assembly with its v29
    // internal geometry untouched (word baseline = centre
    // -37, IN/OUT at -3/+15 beside the plate on the
    // module-centre side, jewel at centre -/+ 38 on the
    // outer flank): the toggle centres land at (108, 76)
    // and (332, 76) — mirrored about the module centre
    // (d = 112 each way) with their centres ON THE EYE-HUB
    // LINE (y 76), the same line the POWER toggle's centre
    // rides on the strip, so all three toggles + the hub
    // share one horizontal line. The span d = 112 is set
    // by the OUT ink: at d = 112 its eye-side corner keeps
    // ~3 device px of circle-exact air from the bezel
    // (measured, build-report-v30). The bottom corners the
    // clusters vacated stay empty panel; the knob, the
    // caption and the bottom wordmark do not move.
    // v31 (Nathan): the pair nudges OUTWARD a little —
    // "limit move left a little bit, mod move right a
    // little bit" — 15 virtual px each way as whole
    // assemblies (words, IN/OUT anchors, jewels and the
    // toggle components in resized()): the toggle
    // centres land at (93, 76) and (347, 76), still
    // mirrored about the module centre (d = 127) and
    // still on the eye-hub line; only the span widens.
    drawTrackedText(g, "LIMIT", capsFont(14.5f), 0.6f,
                    93.0f, 39.0f, Anchor::centre);
    drawTrackedText(g, "MOD", capsFont(14.5f), 0.6f,
                    347.0f, 39.0f, Anchor::centre);
    drawTrackedText(g, "IN", capsFont(14.0f), 0.4f,
                    121.0f, 73.0f, Anchor::left);
    drawTrackedText(g, "OUT", capsFont(14.0f), 0.4f,
                    121.0f, 91.0f, Anchor::left);
    drawTrackedText(g, "IN", capsFont(14.0f), 0.4f,
                    319.0f, 73.0f, Anchor::right);
    drawTrackedText(g, "OUT", capsFont(14.0f), 0.4f,
                    319.0f, 91.0f, Anchor::right);

    // ── Module: section indicator jewels (v15, Nathan) ──────────
    // A small amber jewel on each toggle's OUTER flank — left of
    // LIMIT, right of MOD — drawn with the POWER jewel's own
    // rendering, lit while its section is in, dark while out.
    // Locations confirmed by Nathan. They sit on the toggles'
    // centre line, clear of the IN/OUT silk (which lives on the
    // module-centre side) and of the corner screws.
    // v18: the jewels grow with the rest of the hardware —
    // full POWER-jewel scale (1.0) in place of v15's 0.6.
    // v19: they move with their clusters, and POWER gates them
    // — unit off, all lights out, whatever the toggles say.
    // v30: the jewels ride up with their clusters to the
    // eye line — (48, 400) -> (70, 76) and (392, 400) ->
    // (370, 76), still on their toggles' outer flanks
    // (centre -/+ 38), so all three panel jewels now sit
    // on the one horizontal line y 76 with the eye hub.
    // v31: out with the clusters — (70, 76) -> (55, 76)
    // and (370, 76) -> (385, 76), still centre -/+ 38.
    drawPowerJewel(g, 55.0f, 76.0f, powerOn && limitToggle_.getToggleState(), 1.0f);
    drawPowerJewel(g, 385.0f, 76.0f, powerOn && modToggle_.getToggleState(), 1.0f);

    // ── Module wordmark (v29, Nathan): 'WESTERN
    //    COMPRESSOR' at the very BOTTOM of the module,
    //    CENTRED between the two bottom corner screws
    //    (centres x 18 / 422 — so the centre is the
    //    module's own centre, mcx) and riding the BOTTOM
    //    screw centreline (y 442) with the same screw-line
    //    treatment the top band had, mirrored: the top
    //    baseline was 22.4 for ink 13..23 (centre on the
    //    y-18 line); for unflipped text the mirrored
    //    baseline is 446.4, putting the ink at 437..447
    //    with its centre on 442. The string's ink is
    //    ~150 virtual px wide centred at 220 (~145..295):
    //    wide air to both bottom screw heads (9..27 /
    //    413..431), to the flange rim above it (rim
    //    bottom y 410 at centre) and to the LIMIT/MOD
    //    clusters at the sides (audited,
    //    build-report-v29).
    // v29 addition (Nathan): this wordmark does NOT
    // rise with the controls — the wordmarks are
    // anchored to the SCREWS (centred between them,
    // ink centre on their y-442 centreline), independent
    // of the control layout. The controls rising 10 is
    // what buys this band its clearance (audited,
    // build-report-v29).
    g.setColour(kSilk);
    drawTrackedText(g, "WESTERN COMPRESSOR", capsFont(14.5f), 0.6f,
                    mcx, 446.4f, Anchor::centre);

    // ── The utility STRIP (v13; re-blacked v14; FLUSH v15): a
    //    separate narrow panel filling the unit's right side,
    //    full height, after the photo. v18: it wears the SAME
    //    gloss black as the module (paintGlossBlack above) with
    //    its own scratched edges — one material, one finish,
    //    and the two faces meet at one clean SEAM line and
    //    nothing else. ──────────────
    {
        paintGlossBlack(g, strip, h);
        drawScratchedEdges(g, strip, 0x5eed0002u, edgeSev);
        // THE SEAM: the only thing between the two faces — a
        // dark joint line with a whisper of light on the strip
        // side, exactly where the plates butt.
        g.setColour(juce::Colour(0xff060707));
        g.drawLine(strip.getX(), 0.0f, strip.getX(), h, 2.0f);
        g.setColour(juce::Colour(0x24ffffff));
        g.drawLine(strip.getX() + 1.6f, 0.0f, strip.getX() + 1.6f, h, 1.0f);
    }

    // Strip screws (v19, Nathan): FOUR now — one in each of
    // the strip's corners, like the module's (v12–v18 had two,
    // centred top and bottom after the photo). Each landed at
    // its own angle, inset tight to the corners.
    // v22 (Nathan): the strip's screws were drawn BIGGER
    // than the module's (r 10 vs 9) — they now match the
    // module's r 9 exactly.
    drawPhillipsScrew(g, strip.getX() + 18.0f, 18.0f, 9.0f, 24.0f);
    drawPhillipsScrew(g, strip.getRight() - 18.0f, 18.0f, 9.0f, 97.0f);
    drawPhillipsScrew(g, strip.getX() + 18.0f, h - 18.0f, 9.0f, 140.0f);
    drawPhillipsScrew(g, strip.getRight() - 18.0f, h - 18.0f, 9.0f, 63.0f);

    // ── Strip wordmark (v19): 'BITEY' again, top-left on this
    //    plate, matching the module's — the big floating 'B'
    //    that used to hang here is gone entirely.
    // v22: same outward move as the module's (offset 34 ->
    // 40) so its ink clears this plate's corner screw too.
    // v24 (Nathan): the strip KEEPS 'BITEY' — 'WESTERN
    // COMPRESSOR' replaces the wordmark on the compressor
    // module only — and this wordmark gets the same rise
    // as the module's: baseline 27 -> 22, so its ink
    // centre rides on this plate's top-left screw
    // centreline (y 18; measured centre 18.05 in the
    // v24 audit).
    // v25: same size growth and baseline shift as the
    // module's wordmark (13 -> 14.5, baseline 22 -> 22.4).
    // v26 (Nathan): the strip's wordmark MOVES to the
    // top-RIGHT screw — the exact mirror of its top-left
    // perch: right-anchored at stripRight - 39 (its layout
    // box mirrors the old stripX + 40 box across the
    // strip, then +1 so the measured INK gap matches too:
    // 'B' and 'Y' carry different sidebearings, and at
    // stripRight - 40 the ink sat 6.55 device px from the
    // right screw head against the module wordmark's 6.05
    // from its left one). Same baseline 22.4, so the ink
    // still rides the screw centreline (y 18) and its
    // right edge keeps the same air to the right screw
    // head that its left edge kept to the left one
    // (audited, build-report-v26). The module's WESTERN
    // COMPRESSOR stays top-left.
    // v27 (Nathan): the strip loses its wordmark ENTIRELY
    // — 'BITEY' moves to the compressor module's top-right
    // screw (drawn with the module, above), and this
    // plate's top band is now plain panel: just the four
    // corner screws, no text at all.
    // v29 (Nathan): 'BITEY' RETURNS to this plate — at
    // its very BOTTOM, centred between the two bottom
    // corner screws on the bottom screw centreline,
    // drawn after the OUTPUT section below. This
    // plate's top band stays bare (plain panel, four
    // screws, no text).
    // (v27 fix: the deleted wordmark draw carried the
    // setColour(kSilk) the POWER texts below relied on —
    // restore it explicitly here.)
    g.setColour(kSilk);

    // ── Strip POWER cluster (v19, Nathan): a FULL switch
    //    cluster matching the module's LIMIT/MOD scheme part
    //    for part, at the same sizes — the word 'POWER' above
    //    the toggle (with clear air between the label and the
    //    jewel, as he asked), the BatToggle component on its
    //    black escutcheon with a SILVER lever, state labels
    //    'ON'/'OFF' beside the throws where IN/OUT sit on the
    //    module clusters, and the amber jewel on the toggle's
    //    left flank — on the cluster's own centre line
    //    (y 176), exactly as the module clusters are built.
    //    (Until v19 moved the caption above the eye, that
    //    line was also the GAIN REDUCTION caption's line;
    //    the cluster's internal geometry is unchanged.)
    //    The jewel is the power light now: lit while 'power'
    //    is on, dark while off (v12–v18 it followed LIMIT —
    //    there was no power state to follow).
    // v21 audit nudges: POWER baseline 143 -> 142 and the
    // ON/OFF anchor 2 px further out (566 -> 568), for the
    // same >= 2 device px of measured air around the plate.
    // v25 (Nathan): the whole POWER cluster drops A HAIR
    // (+4 virtual: word 142 -> 146, jewel line 176 -> 180,
    // ON 173 -> 177, OFF 191 -> 195; the toggle component
    // follows in resized()) so the GAIN REDUCTION caption
    // alignment is met in the middle — the eye doesn't
    // have to climb the whole way.
    // v26 (Nathan): "power can come back down a little
    // bit" too — the cluster descends +6 further (word
    // 146 -> 152, jewel line 180 -> 186, ON 177 -> 183,
    // OFF 195 -> 201; the toggle follows in resized()),
    // keeping step with the eye stack's own +9 descent so
    // the caption/POWER near-alignment stays in the same
    // spirit (audited in build-report-v26).
    // v27 (Nathan): the whole POWER cluster rises so the
    // CENTRE OF THE TOGGLE sits on the magic eye's hub
    // line — the cluster line 186 -> 86, the eye's centre
    // (its hub screw is drawn at (220, 86)); word, jewel
    // and ON/OFF ride up the same -100, internal scheme
    // untouched, and the toggle follows in resized().
    // v29 addition: the cluster rises 10 with everything
    // (word 52 -> 42, jewel line 86 -> 76, ON/OFF 83/101
    // -> 73/91; the toggle follows in resized()) — the
    // toggle's centre stays on the eye-hub line, which
    // rises the same 10 to y 76.
    drawTrackedText(g, "POWER", capsFont(14.5f), 0.6f,
                    scx, 42.0f, Anchor::centre);
    drawTrackedText(g, "ON", capsFont(14.0f), 0.4f,
                    568.0f, 73.0f, Anchor::left);
    drawTrackedText(g, "OFF", capsFont(14.0f), 0.4f,
                    568.0f, 91.0f, Anchor::left);
    drawPowerJewel(g, 497.0f, 76.0f, powerOn, 1.0f);

    // ── Strip bottom: the O/P LEVEL section (v12) ────────────────
    // Two small black knobs (components: outLevelL_/outLevelR_)
    // under a shared caption, each read against five silk dot
    // markers around its travel — min at 7:30, unity at
    // 12 o'clock, max at 4:30, like the H2's dots.
    g.setColour(kSilk);
    // v19 (Nathan): the caption is just 'OUTPUT' now — the
    // word LEVEL goes away entirely; L/R stay under the knobs.
    // v25 (Nathan): the two pots move FURTHER APART
    // (centres 505/575 -> 496/584) and LARGER (components
    // 23 -> 27 device px in resized(); the SmallKnob look
    // scales with bounds, so hit area and drawing grow
    // together — easier to grab with a mouse). The dot
    // rings follow the new centres and grow with the knobs
    // (R 27 -> 30); the caption rises a little (348 ->
    // 342) to make room, and L/R tuck UP closer under
    // their knobs (436 -> 424).
    // v27 (Nathan): the whole OUTPUT section rises so
    // the pots' CENTRES sit on the input knob's centre
    // line — pot/dot centre y 383 -> 310 (the decade
    // knob's centre screw is drawn at (220, 310)); the
    // caption (342 -> 269) and L/R (424 -> 351) ride up
    // the same -73, relative spacing untouched, and the
    // knob components follow in resized().
    // v29 addition: the section rises 10 with everything
    // (caption 269 -> 259, pot/dot centres 310 -> 300,
    // L/R 351 -> 341; the components follow in
    // resized()) — the pot centres stay on the input
    // knob's centre line, which rises the same 10.
    drawTrackedText(g, "OUTPUT", capsFont(14.5f), 0.6f,
                    scx, 259.0f, Anchor::centre);
    for (float kcx : { 496.0f, 584.0f }) {
        for (int i = 0; i <= 4; ++i) {
            const float aDeg = 225.0f + 67.5f * (float)i;
            const auto dp = polar(kcx, 300.0f, 30.0f, aDeg);
            const float dr = (i == 2) ? 2.0f : 1.6f;
            g.setColour(kSilk.withAlpha(0.9f));
            g.fillEllipse(dp.x - dr, dp.y - dr, dr * 2.0f, dr * 2.0f);
        }
    }
    g.setColour(kSilk);
    drawTrackedText(g, "L", capsFont(14.0f), 0.4f,
                    496.0f, 341.0f, Anchor::centre);
    drawTrackedText(g, "R", capsFont(14.0f), 0.4f,
                    584.0f, 341.0f, Anchor::centre);

    // ── Strip wordmark (v29, Nathan): 'BITEY' at the
    //    very BOTTOM of the strip, CENTRED between the
    //    two bottom corner screws (centres x 458 / 622 —
    //    so the centre is the strip's own centre, scx)
    //    and riding the bottom screw centreline (y 442)
    //    with the same treatment as the module's bottom
    //    wordmark: baseline 446.4, ink 437..447, centre
    //    on 442. The ink (~37 virtual px wide, ~521..559)
    //    keeps wide air to both bottom screw heads
    //    (449..467 / 613..631) and to the OUTPUT section
    //    above it (the L/R ink ends ~352). The strip's
    //    TOP band stays bare (see the history above).
    // v29 addition: like the module's wordmark, this
    // one stays anchored to the screws when the
    // controls rise 10 — the OUTPUT section above it
    // rises away (L/R ink now ends ~342), widening
    // this band's air (audited, build-report-v29).
    g.setColour(kSilk);
    drawTrackedText(g, "BITEY", capsFont(14.5f), 0.6f,
                    scx, 446.4f, Anchor::centre);

    // ── The chassis' outer rim ────────────────────────────────────
    // v18: SHARP 90° corners — the rim is a square rectangle
    // now, no corner radius anywhere on the frame.
    g.setColour(juce::Colour(0xff0a0b0c));
    g.drawRect(1.0f, 1.0f, w - 2.0f, h - 2.0f, 2.0f);
    g.setColour(juce::Colour(0x1affffff));
    g.drawLine(3.0f, 2.5f, w - 3.0f, 2.5f, 1.2f);
}
