#pragma once
// Hotdog editor (v16) — a WIDE RACK unit after Nathan's
// H2 Audio 002 photo, two plates in ONE plane: at the left the
// Bitey compressor MODULE — a clean brushed-black plate
// carrying only the 1629-style magic eye (full glowing green
// with a dark centre hub at rest; black eats into the green as
// gain reduction builds), the ONE large decade knob, and the
// two LIMIT/MOD lever switches — and at the right a SEPARATE
// narrow utility strip. v15: the module face butts FLUSH
// against the strip face (bay/lip/moat deleted), the LIMIT/MOD
// levers are GOLD with amber indicator jewels on their outer
// flanks, and the knob's Allen centre screw is BLACK. v16
// corrections per Nathan: (1) the module face is CLEAN — all
// scratches/scuffs/ageing removed, it draws no weathering;
// (2) ONE MATERIAL — the module's black is now the strip's
// brushed black (same gradient + brush grain) on both plates;
// (3) the lettering changes AGAIN — Michroma was "still real
// estate font, especially that B" — to SAIRA STENCIL ONE:
// stamped-equipment / crate-and-military-stencil lettering,
// "way more industrial and less serious, more utilitarian",
// every string in PURE WHITE #FFFFFF at full opacity
// (flange numerals and the index triangle too); (4) the
// window shrinks again, to 640x460; (5) the strip aligns to
// the module: the bare 'B' drops so its centre sits on the
// magic eye's centre line, and the POWER jewel drops so it
// sits on the GAIN REDUCTION text's line (POWER label above
// it; O/P LEVEL knobs stay at the strip bottom). No P48
// toggles (Nathan: omit them).
// v17: SIZE ONLY — the window halves to 320x230 (Nathan:
// "make it half the size"). The panel paint stays authored
// in the v16 640x460 space under a 0.5 transform; component
// bounds and repaint rects are re-based at half scale; the
// smallest labels sit a touch above strict 0.5 scale for
// legibility (see docs/build-report-v17.md).
// v18 (Nathan, 2026-10-14): the window grows ~10% (352x253)
// while every control and marking grows ~x1.5–2 (decade knob,
// eye, O/P pots, toggles, jewels, all lettering) — hardware
// dominant, panel field shrunk; all panel/frame corners are
// SHARP 90° angles; the brushed metal is KILLED — both plates
// are smooth GLOSS BLACK with the scratch/wear moved to the
// EDGES and corners only (faces clean); and the lettering
// changes again — Saira Stencil was still "a little too
// nice, especially the B" — to SPECIAL ELITE: eroded,
// ink-starved typewriter glyphs, rough and utilitarian.
// The snapshot-only 'black' knob colourway joins red/yellow/
// green for the v18 black-pot variant still.
// v19 (Nathan, 2026-10-14): (1) flange numerals shrink onto
// the flange (smaller ring + face); (2) the LIMIT/MOD switch
// clusters move up and outward, off the bottom edge; (3) ALL
// panel labels unify at the LIMIT/MOD size (GAIN REDUCTION
// comes down); (4) the lettering changes again — Special
// Elite was "in the right direction" but the typewriter face
// isn't loved — to RUBIK DIRT: the same erosion in a heavy
// stamped slab, industrial not typed. (5) The knob ships
// BLACK (the v18 variant becomes the product) and the
// LIMIT/MOD levers go SILVER. (6) The big floating 'B' is
// GONE — small 'BITEY' wordmarks sit top-left on each plate
// instead. (7) The strip gains a full POWER switch cluster
// matching LIMIT/MOD (word above, silver lever, ON/OFF
// labels, amber jewel on the GAIN REDUCTION line), bound to
// the processor's 'power' parameter; off = all lights out
// and the audio bypassed. (8) The strip gets four corner
// screws, like the module.
// v20 (Nathan, 2026-10-14): he likes v19 — three
// refinements. (1) 'INPUT' drops down to sit almost right
// on top of the knob's index triangle. (2) The lettering
// keeps Rubik Dirt's worn style but must render SHARPER,
// THINNER and brighter: the face returns to SAIRA STENCIL
// ONE with a light procedural worn treatment in the text
// rendering (sparse deterministic pinholes, hard edges,
// pure white). (3) The magic eye's clean white ring — "it
// looks almost like a graphic" — becomes an AGED GOLD
// retaining bezel with fine scratches and uneven wear.
// v21 (Nathan, 2026-10-14): the lettering changes to a
// vintage WESTERN face — SMOKUM, the register of his
// "WESTERN GRANDE" specimen — with a VERY SUBTLE
// spray-paint treatment in the text rendering (faint
// overspray halo + edge speckle; glyph cores stay crisp
// pure white) replacing the worn pinholes. And the v20
// sizes come DOWN by about a third with the tracking
// tightened to near-natural: the lettering had gotten too
// big, too spaced out and overlapping (screws and labels
// included) — the top band is re-audited, nothing touches
// anything. The flange numerals are bright pure white
// again, a touch smaller than the general shrink.
// v22 (Nathan, 2026-10-14): the spray paint is GONE
// completely — "make it just very clean bright white
// lettering". The pre-rendered text-image path (string
// caches, overspray halo, numeral underlays — the grey
// boxes around the flange numerals) is deleted: ALL text
// is direct vector text in pure #FFFFFF. Smokum stays,
// a step SMALLER (labels 10, markings 9.5) and STRETCHED
// 1.2x horizontally in capsFont; the flange numerals go
// UP to 16 pt so they read clearly on the black flange,
// fully on the annulus. The BITEY wordmarks move clear
// of the corner screws, the strip's corner screws shrink
// to the module screws' size (r 9), and the zero-overlap
// audit passes again. Everything else as v21.
// v23 (Nathan, 2026-10-14): v22's lettering is "impossible
// to read" — Smokum's hairline strokes anti-alias to grey
// mush at panel sizes. The face becomes SANCREEK, the
// bolder Western slab (roughly double the stroke weight),
// set ~30% BIGGER (labels 13, markings 12.5) and
// stretched 1.5x; the flange numerals keep v22's 16 pt
// height in the bolder, wider face. Legibility gate at
// true size and the zero-overlap audit both pass.
// Everything else as v22.
// v24 (Nathan, 2026-10-14): Sancreek is rejected ("i don't
// like that font"); from a four-face specimen sheet Nathan
// picks SMOKUM EMBOLDENED — the Western face with real
// outline weight baked in (FreeType stroker, 3.5% of em)
// — at v23's exact metrics. The module wordmark becomes
// 'WESTERN COMPRESSOR' (the strip keeps 'BITEY'); both
// wordmarks rise onto their top-left screw's centreline;
// the magic eye nudges up 5 virtual px; and the GAIN
// REDUCTION caption moves back under the eye. Everything
// else as v23.
// v25 (Nathan, 2026-10-14): "not bad… it's close." The
// lettering goes a little bigger (labels 14.5, markings
// 14) and a little thinner (SMOKUM EMBOLDENED THIN — the
// bake at 2.34% of em; the flange numerals keep the v24
// bake). The eye rises for real (top 43 -> 22) while the
// POWER cluster drops a hair, meeting the caption/POWER
// alignment in the middle; the input stack drops a
// little (knob centre 300 -> 310) and the flange widens
// (R 95 -> 100, numerals 17.5 pt at R 90); the O/P pots
// spread (496/584), grow (27 device px) and their L/R
// labels tuck up. Everything else as v24.
// v26 (Nathan, 2026-10-14): small moves back. The eye +
// GAIN REDUCTION caption settle DOWN partway (eye centre
// 77 -> 86, caption baseline 150 -> 159) and the POWER
// cluster descends a few px further with them (word 146
// -> 152, cluster line 180 -> 186). ALL fonts thin out
// — SMOKUM EMBOLDENED XTHIN, the bake at 1.42% of em,
// numerals included this time — at v25's sizes. And the
// strip's BITEY wordmark mirrors from the top-left screw
// to the top-RIGHT screw, same centreline, same air.
// Everything else as v25.
// v27 (Nathan, 2026-10-14): "everything looks good, the
// font looks good — one last change": 'BITEY' leaves the
// power strip entirely (the strip's top is plain panel,
// just its four corner screws) and moves onto the
// compressor module at its top-RIGHT screw, mirrored
// against 'WESTERN COMPRESSOR' — same screw centreline,
// same measured air to its screw. Plus two centreline
// alignments: the POWER cluster rises so the toggle's
// centre sits on the magic eye's hub line (y 86), and
// the OUTPUT section rises so the pots' centres sit on
// the input knob's centre line (y 310). Everything else
// as v26.
// v28 (Nathan, 2026-10-14): the two module wordmarks
// SWAP sides — 'BITEY' takes the top-left screw anchor,
// 'WESTERN COMPRESSOR' the top-right — same screw
// centreline, same anchors, same measured air to the
// neighbouring screw heads. Everything else as v27.
// v29 (Nathan, 2026-10-14): the wordmarks leave the
// TOP band entirely — both plates' tops are bare panel
// and screws. 'WESTERN COMPRESSOR' goes to the very
// BOTTOM of the compressor module, centred between its
// two bottom corner screws on the bottom screw
// centreline (baseline 446.4, ink centre on y 442);
// 'BITEY' returns at the very BOTTOM of the power
// strip, centred between its bottom screws the same
// way. ADDITION (same round): every control assembly
// rises 10 virtual px as a whole — eye + caption,
// input stack, LIMIT/MOD clusters, POWER cluster,
// OUTPUT section — so the wordmark bands get room
// and the switches lift off the bottom; the wordmarks
// stay anchored to the screws (centred both ways),
// and the two centreline alignments (POWER toggle
// <-> eye hub, OUTPUT pots <-> input knob) hold.
// Everything else as v28.
// v30 (Nathan, 2026-10-14, EXPERIMENTAL — "let's try
// one where…"): the LIMIT and MOD switch clusters
// RELOCATE UP to FLANK THE MAGIC EYE — each cluster
// as a whole assembly (word above, toggle on its
// plate, IN/OUT beside the throws, amber jewel on
// its outer flank), toggle centres at (108, 76) and
// (332, 76), ON THE EYE-HUB LINE, so all three
// toggles + the eye hub share one horizontal line
// (the POWER toggle already rides that line on the
// strip). The vacated module bottom corners stay
// empty panel; the knob, caption and bottom
// wordmarks do not move. Everything else as v29.
// v31 (Nathan, 2026-10-14): the flanking pair nudges
// OUTWARD a little — LIMIT left, MOD right — 15
// virtual px each way as whole assemblies, toggle
// centres (108, 76) / (332, 76) -> (93, 76) /
// (347, 76), span d 112 -> 127, still mirrored about
// the module centre and still on the eye-hub line.
// Everything else as v30.
// v32 (Nathan, 2026-10-14): a SECOND build with the
// same audio (the plugin is renamed WESTERN
// COMPRESSOR with this build) — the LIMIT/MOD
// clusters return to the BOTTOM of the module at
// the v29 flank x positions (centres 86 / 354),
// with the toggle CENTRES on the line through the
// FLANGE'S BOTTOM EDGE (knob centre 300 + flange
// R 100 = y 400 — the v29 bottom height, internal
// cluster geometry unchanged). The strip does not
// move: POWER stays on the eye-hub line, OUTPUT on
// the knob-centre line. Everything else as v31.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "PluginProcessor.h"

// ── GUI variant exploration (v7, dev-only) ────────────────────────
// The shipped default is the approved v6 panel: red knob, standard
// weathering. Two A/B variants for Nathan to judge are selectable
// here — a yellow knob or an eye-green knob, both on a more beaten-
// up case. Only the headless snapshot tool sets these; a DAW always
// gets the v6 defaults below.
namespace osc72gui {
enum class KnobColour { red, yellow, green, black };
void setKnobColour(KnobColour c);
KnobColour knobColour();
void setBeatenUp(bool b);   // heavier case weathering (variants)
bool beatenUp();
} // namespace osc72gui
// (v19: the shipped default knob colour is BLACK — Nathan is
// "liking the black"; red/yellow/green remain snapshot-only
// exploration variants.)

// Bakelite knob with a white index line, drawn in code.
// v7: the knob body is rendered per-pixel into a cached image from
// an analytic fluted-cylinder shading model (see PluginEditor.cpp)
// — no flat wedges, no line-stroke pointer.
// v8: fluting is low-relief under a near-round silhouette (v7's
// deep scallops read as a cookie), and the pointer bar is shorter
// so it ends well inside the face, clear of the ring.
// v9: stepped profile from Nathan's decade-box photo — smooth
// domed top disc, fine ribbed grip ring, centre cap screw
// turning with the knob; the numbered skirt flange is drawn by the
// editor (see paint()).
// v10: NO pointer on the knob (the centre screw is the only mark
// on the disc) — the flange's numerals rotate WITH the knob and
// are read against a fixed index triangle on the plate at
// 12 o'clock, exactly like the photo's box.
// v11: ALL panel text is set in one embedded period face, and
// both centre screws (knob cap screw + magic-eye hub) are Allen
// hex-socket heads.
// v12: the knob moves onto the rack panel unchanged in profile;
// its Allen centre screw carries over.
// v13: the knob lives on the compressor MODULE bolted into the
// rack bay; the knob's profile and Allen centre screw are unchanged.
// v14: the module is bolted tight into a shrunken mounting lip;
// panel face is Titan One (seventies, rounded, easy to read).
// v15: module face sits FLUSH against the utility strip at 3/4
// size; panel face is Michroma (serious squared 70s tech
// lettering — Titan One rejected as cartoonish); the knob's
// Allen centre screw is BLACK.
class OscKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

private:
    juce::Image bodyCache_;
    int cacheW_ = 0, cacheH_ = 0, cachePalette_ = -1;
};

// ─────────────────────────────────────────────────────────────────────
// Bat toggle (v8; restyled v9; recoloured v15): vertical
// throw, UP = in, DOWN = out. (Placement: the plate's bottom
// corners through v29; v30 moves the LIMIT/MOD pair up to
// flank the magic eye — see the v30 note above; v32 returns
// the pair to the bottom corners, toggle centres on the
// flange's bottom-edge line — see the v32 note above.) v9
// renders it as the reference photo's switch: a heavy lever
// with a rounded teardrop tip on a small black escutcheon
// plate. v15 (Nathan): the black levers read "a little too
// cartoonish" — the lever turned GOLD, in the knob's aged-gold
// palette, same heavy form; the escutcheon plate stayed dark.
// v19 (Nathan): the levers go SILVER — real steel/chrome-
// family shading, same heavy modelling, dark plate unchanged.
// A third BatToggle on the strip is the POWER switch (v19),
// bound to the 'power' parameter.
// The artwork is drawn in a virtual 40x54 space and scaled to
// the component bounds (v15 shrinks the toggles to 30x41).
// Bound to a bool parameter via a ButtonAttachment; the
// silk-screen labels ('limit'/'mod', 'in'/'out') are drawn by
// the editor around it. The lever flips through the panel plane
// (foreshortened mid-flip), animated from the editor timer.
// ─────────────────────────────────────────────────────────────────────
class BatToggle : public juce::Button
{
public:
    BatToggle();
    // Advance the flip animation one editor-timer tick; returns
    // true while the bat is still travelling (caller repaints).
    bool advanceAnimation();
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;

private:
    float flipT_ = 0.0f;      // 0 = out (down), 1 = in (up)
    bool snapped_ = false;    // first paint snaps to the real state
};

// ─────────────────────────────────────────────────────────────────────
// Mini bat toggle (gui-v53; placement corrected in
// gui-v54, Nathan 2026-10-17): the MOD section's A/P
// flavour selector — a MINIATURE silver bat with a
// HORIZONTAL throw, sitting on the MOD WORD's own line,
// reading as a continuation of it: MOD, a word space,
// 'A', the toggle, 'P'. Lever toward A (left) = the
// Reel ADT,
// toward P (right) = the Phase 90 (APVTS 'modPhaser',
// false = A, the default). Same rendering language as
// BatToggle — black escutcheon plate, silver teardrop
// lever foreshortening through the flip, editor-timer
// animation — authored in a 52x40 space and scaled to its
// (much smaller) bounds. The 'A' / 'P' silk is drawn by
// the editor around it, like the other toggles' labels.
// gui-v56 replaced this bat with a round push button and
// lit the active letter as a lamp; gui-v57 (Nathan,
// 2026-10-17) RESTORES the bat — "revert the mod button
// back to a small toggle that toggles left right" — at
// the v56 aligned position, and drops the lamp letters
// entirely ("Don't make the A and the P light up"): the
// letters are plain silk in both states, and the lever
// position alone shows the flavour.
// gui-v58 (Nathan, 2026-10-17): a SECOND instance joins
// the panel — the 1X / 2X timing selector on the RATIO
// word's line, the A/P assembly's mirror (reading
// 2X · toggle · 1X · RATIO, lever toward the word at
// 1X). It is built MIRRORED: its off state (1X) throws
// the lever +x (toward RATIO) and its on state (2X)
// throws it -x (outward) — the A/P sense inverted —
// selected by the constructor flag.
// ─────────────────────────────────────────────────────────────────────
class MiniBatToggle : public juce::Button
{
public:
    explicit MiniBatToggle(bool mirrored = false);
    // Same contract as BatToggle::advanceAnimation.
    bool advanceAnimation();
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;

private:
    float flipT_ = 0.0f;      // 0 = off state, 1 = on state
    bool snapped_ = false;    // first paint snaps to the real state
    bool mirrored_ = false;   // gui-v58: lever sense inverted (1X/2X)
};

// ─────────────────────────────────────────────────────────────────────
// Small output knob (v12): the H2 002's little black O/P LEVEL
// knobs — a small black fluted knob with a single white pointer
// line, read against the dot markers the editor silk-screens
// around its travel. Vector-drawn (at ~42 px a per-pixel render
// is unnecessary); the pointer convention matches the editor's
// polar() helper: angle from 12 o'clock, clockwise.
// ─────────────────────────────────────────────────────────────────────
class SmallKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override
    {
        const float cx = (float)x + (float)width * 0.5f;
        const float cy = (float)y + (float)height * 0.5f;
        const float R = std::min((float)width, (float)height) * 0.5f - 1.0f;
        // v17: the fixed stroke widths below were tuned at the
        // v16 bounds (R = 12) — scale them with the knob so the
        // half-size 13 px bounds (R = 5.5) keep the same look.
        const float u = R / 12.0f;
        const float angle = rotaryStartAngle
                          + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        auto pt = [&](float r) {
            return juce::Point<float>(cx + r * std::sin(angle),
                                      cy - r * std::cos(angle));
        };

        // Drop shadow
        g.setColour(juce::Colour(0x66000000));
        g.fillEllipse(cx - R, cy - R + 2.0f * u, R * 2.0f, R * 2.0f);
        // Body: near-black bakelite, light from the upper left
        {
            juce::ColourGradient bg(juce::Colour(0xff4b4e52), cx - R, cy - R,
                                    juce::Colour(0xff101112), cx + R, cy + R, false);
            g.setGradientFill(bg);
            g.fillEllipse(cx - R, cy - R, R * 2.0f, R * 2.0f);
        }
        // Flutes: fine alternating light/dark radial ticks in the
        // rim band — subtle, so the knob reads as knurled black,
        // not striped.
        for (int k = 0; k < 16; ++k) {
            const float a = (float)k * juce::MathConstants<float>::twoPi / 16.0f;
            const float sx = std::sin(a), cv = -std::cos(a);
            g.setColour(k % 2 == 0 ? juce::Colour(0x2effffff)
                                   : juce::Colour(0x38000000));
            g.drawLine(cx + sx * R * 0.76f, cy + cv * R * 0.76f,
                       cx + sx * R * 0.96f, cy + cv * R * 0.96f, 1.5f * u);
        }
        // Top cap: a slightly domed disc inside the rim
        {
            juce::ColourGradient cap(juce::Colour(0xff3a3d40), cx - R * 0.6f, cy - R * 0.6f,
                                     juce::Colour(0xff17181a), cx + R * 0.5f, cy + R * 0.6f, false);
            g.setGradientFill(cap);
            const float cr = R * 0.68f;
            g.fillEllipse(cx - cr, cy - cr, cr * 2.0f, cr * 2.0f);
            g.setColour(juce::Colour(0x30ffffff));
            g.drawEllipse(cx - cr, cy - cr, cr * 2.0f, cr * 2.0f, 0.8f * u);
        }
        // The white pointer line (the photo's marker) — v18:
        // pure brightest white, like every other marking.
        {
            const auto p0 = pt(R * 0.16f), p1 = pt(R * 0.88f);
            g.setColour(juce::Colour(0xffffffff));
            g.drawLine(p0.x, p0.y, p1.x, p1.y, 2.3f * u);
            g.fillEllipse(p0.x - 1.15f * u, p0.y - 1.15f * u, 2.3f * u, 2.3f * u);
            g.fillEllipse(p1.x - 1.15f * u, p1.y - 1.15f * u, 2.3f * u, 2.3f * u);
        }
    }
};

class Osc72Editor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit Osc72Editor(Osc72Processor&);
    ~Osc72Editor() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    // GR display (v4, Nathan 2026-10-15): the round magic eye
    // is replaced by the Pioneer 202W-style wide rectangular
    // ribbed fluorescent display in the same top-centre spot,
    // framed by the eye's aged-gold bezel reshaped rectangular.
    // grDb 0 -> the full green ribbed field lit (the photo);
    // as GR rises the field's edges go black and the darkness
    // closes symmetrically toward the centre; 20 dB -> totally
    // dark (v52 range; 28 dB through v51). Driven by the
    // single overall GR, mono or stereo.
    // dark = POWER off: a dead display — no green at all.
    void drawGrDisplay(juce::Graphics& g, float grDb, bool dark = false);

    // v36: the GR field is rendered PER-PIXEL into a cached
    // image — Nathan: the original 202W has NO horizontal
    // lines across its field; its pattern is formed by the
    // vertical slices' individual shading alone. The builder
    // shades every column as its own vertical gradient (rib
    // louver + vertical phosphor profile + the woven diamond
    // band emerging from neighbouring columns' stop
    // positions), so no stroke, seam or band edge can draw a
    // horizontal line.
    // v40 (Nathan, on 1.1.0): the artwork is built ONCE, at
    // full width, and never rebuilt — GR moves only the two
    // black masks in drawGrDisplay. "The green should not be
    // moving against the black; it should be the black
    // moving against the green."
    juce::Image buildGrFieldImage();

    Osc72Processor& proc_;
    OscKnobLookAndFeel laf_;
    juce::Slider knob_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att_;
    BatToggle limitToggle_, modToggle_, powerToggle_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limitAtt_, modAtt_, powerAtt_;
    // gui-v53 (Nathan): the MOD flavour selector (A/P),
    // bound to 'modPhaser' — a MiniBatToggle (restored in
    // gui-v57 after gui-v56's push-button round).
    MiniBatToggle phaserToggle_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> phaserAtt_;
    // gui-v58 (Nathan): the 1X / 2X timing selector,
    // bound to 'timesTwo' (the v62 DSP) — the A/P
    // toggle's mirrored twin on the RATIO word's line.
    MiniBatToggle timesTwoToggle_ { true };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> timesTwoAtt_;
    // v4: ATTACK / RELEASE ballistics toggles flanking the GR
    // display at the v31 positions (no jewels on these two).
    // v35: the pair drops down — words on the line the GAIN
    // REDUCTION caption then shared (baseline 149), toggles
    // directly under their words at virtual centres
    // (93, 186) / (347, 186).
    // v36: the caption moves ABOVE the display; the words
    // stay at 149, now their own line.
    BatToggle attackToggle_, releaseToggle_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attackAtt_, releaseAtt_;
    // v36 (Nathan): the strip's LINK toggle — a POWER-style
    // cluster (jewel to the RIGHT of the toggle, LINK/UNLINK
    // labels to its LEFT — the reverse of POWER's order,
    // per Nathan's explicit arrangement). Bound to 'outLink'
    // (default linked); the linkage itself is editor-side
    // (see the constructor).
    // v37 (Nathan): the cluster drops below the output pots
    // to the bottom row — toggle centre on virtual y 400,
    // the LIMIT/MOD toggle centres' line.
    BatToggle linkToggle_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAtt_;
    bool linkingPots_ = false;   // reentrancy guard for the pot linkage
    bool linkageArmed_ = false;  // v36: set at the end of the constructor, so the
                                 // attachments' initial updates can't equalise
                                 // saved unequal pot values at load
    SmallKnobLookAndFeel smallLaf_;
    juce::Slider outLevelL_, outLevelR_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outAttL_, outAttR_;
    // v38 (Nathan): the MIX pot — the same small knob as the
    // O/P LEVEL pots, centred under the POWER toggle on the
    // ATTACK/RELEASE centre line (virtual (540, 186)), bound
    // to 'mix' (0 = dry, 1 = wet, default wet). Its silk —
    // dot ring, '0'/'100' end numerals and the MIX word
    // under the pot, L/R-style — is drawn in paint().
    juce::Slider mixKnob_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAtt_;
    // v41 (Nathan): the SIDECHAIN pot — the detector low-cut
    // ('scCutoff', 10–200 Hz, default 10 = filter out). The
    // strip's four small pots become a 2x2 grid on the
    // output pots' columns: top row SIDECHAIN (left) / MIX
    // (right, moved from x 540), bottom row OUTPUT L / R
    // unmoved; all four shrink a touch (54 -> 50 device px,
    // silk radii scale with them). Its silk (word, dot ring,
    // '10'/'200' end numerals) is drawn in paint(), MIX-style.
    // v42: the low-cut glyph that sat beside the word in v41
    // is deleted — the SIDECHAIN word alone centres on the pot.
    juce::Slider scKnob_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> scAtt_;

    float displayedGrDb_ = 0.0f;   // ballistics-smoothed eye value
    float lastFlangeProp_ = -1.0f; // v10: knob pos at last flange repaint
    juce::Rectangle<int> eyeArea_; // repaint region for the eye
    // v36/v40: the per-pixel GR field image cache (see
    // buildGrFieldImage) — the static full-width artwork,
    // built once on first use and never rebuilt.
    juce::Image grFieldCache_;
    // v15: the LIMIT/MOD/POWER jewels are editor paint, so their
    // areas need a repaint when a toggle state flips.
    bool lastLimitState_ = true;
    bool lastModState_ = false;
    bool lastPowerState_ = true;   // v19: power flip repaints the whole panel (all lights + eye)
    bool lastLinkState_ = true;    // v36: LINK jewel repaint tracking

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Osc72Editor)
};
