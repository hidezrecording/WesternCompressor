// tests/snapshot.cpp — headless GUI snapshot for visual verification.
// Renders the plugin editor to a PNG without a display (xvfb).
// Usage: OscSnapshot [output.png] [tokens...]
//   yellow | green = v7 A/B knob colour + beaten-up case
//                    (default = shipped v6: red knob, std weathering)
//   black          = v18 black-pot variant (gloss black knob +
//                    flange) + beaten-up case
//   warble         = 'mod' toggle IN (default: out)
//   phaser         = MOD flavour P selected (default: A) —
//                    gui-v53: the A/P mini toggle's lever
//                    toward P ('modPhaser' true).
//   limitout       = 'limit' toggle OUT (default: in)
//                    v53: the toggle is the RATIO mode switch —
//                    IN (default here) renders COMP (lever up),
//                    'limitout' renders LIMIT (lever down); the
//                    jewel is lit in both. gui-v50-a is rendered
//                    knob=5 + limitout = the plugin's actual
//                    default opening state (INPUT 5, LIMIT).
//   poweroff       = strip 'power' toggle OFF (default: on) —
//                    v19: the whole unit dark and bypassed.
//   (v19: the knob ships BLACK, so no colour token is the
//   black-pot product shot; 'yellow' remains available.)
// v8 shots: gui-v8-a.png = "yellow" (limit in, mod out);
//           gui-v8-b.png = "yellow warble" (limit in, mod in).
// v10 shots: gui-v10-a.png / gui-v10-b.png, same tokens — with
// the knob parked at 7.0 the rotating flange puts the '7'
// numeral at 12 o'clock under the plate's fixed index mark.
// v13 shots: gui-v13-a.png / gui-v13-b.png, same tokens — the
// editor is now the wide 1180x800 rack chassis (module in the
// bay at left, utility strip at right), so the renders come
// out at 1770x1200.
// v14 shots: gui-v14-a.png / gui-v14-b.png, same tokens — the
// rack tightens to 1064x760 (renders 1596x1140): the module
// bolts into a slim mounting lip, the 'B' floats bare on the
// strip, and the lettering is Titan One (seventies).
// v15 shots: gui-v15-a.png / gui-v15-b.png, same tokens — the
// unit shrinks to 798x570 (renders 1197x855), the module face
// sits FLUSH against the strip (no bay/lip/moat), the lettering
// is Michroma (serious squared 70s tech; Titan One rejected as
// cartoonish), the LIMIT/MOD levers are GOLD with amber
// indicator jewels on their outer flanks (A: LIMIT jewel lit,
// MOD jewel dark; B: both lit), the knob's Allen centre screw
// is black, and — per Nathan — the magic eye is shown AT REST
// (0 dB GR, entirely green) so he can judge the colour
// composition. The eye override below is therefore 0 dB for
// v15; the plugin's live eye still displays real GR in a DAW.
// v16 shots: gui-v16-a.png / gui-v16-b.png, same tokens — the
// unit shrinks to 640x460 (renders 960x690), the module face
// is CLEAN (no weathering at all) and wears the strip's own
// brushed black (one material on both plates), the lettering
// is Saira Stencil One (stamped-equipment stencil; Michroma
// rejected as "real estate font") in PURE WHITE everywhere,
// and the strip aligns to the module: 'B' centred on the eye's
// centre line, POWER jewel on the GAIN REDUCTION line.
// v17 shots: gui-v17-a.png / gui-v17-b.png, same tokens —
// SIZE ONLY: the window halves to 320x230 ("make it half the
// size"), the panel paint authored in the v16 space under a
// 0.5 transform. The snapshot scale rises to 2.0x so the
// stills stay crisp: 640x460 PNGs of the 320x230 window.
// (docs/gui-v17-context.png additionally shows the A render
// downscaled to true native 320x230 centred on a 1440x900
// desktop canvas.)
// v18 shots: gui-v18-a.png / gui-v18-b.png, same tokens —
// the window grows ~10% to 352x253 while every control and
// marking grows x1.5-2, all panel/frame corners go SHARP,
// the finish becomes GLOSS BLACK with SCRATCHED EDGES (faces
// clean), and the lettering becomes SPECIAL ELITE (eroded,
// utilitarian; Saira Stencil still read "too nice"). Renders
// are 704x506 at the 2.0x scale. gui-v18-c.png = "black"
// token: state A with the main pot + flange in gloss black
// for Nathan's side-by-side. (docs/gui-v18-context.png shows
// the A render at true native 352x253 centred on the same
// 1440x900 canvas.)
// v19 shots: gui-v19-a.png (no colour token — the knob now
// SHIPS black, so the default IS the black pot), gui-v19-b.png
// ("warble"), gui-v19-off.png ("poweroff" — the strip POWER
// toggle off: every light out, eye dark, audio bypassed).
// The lettering is RUBIK DIRT (eroded stamped slab; Special
// Elite's typewriter face wasn't loved), all labels sit at
// one size, the flange numerals sit ON the flange, the switch
// clusters ride higher, the levers are silver, 'BITEY'
// wordmarks replace the big 'B', and the strip has four
// corner screws and a full POWER switch cluster.
// v20 shots: gui-v20-a.png / gui-v20-b.png ("warble") /
// gui-v20-off.png ("poweroff"), same tokens. Refinements
// only: 'INPUT' sits almost on top of the index triangle;
// the lettering is SAIRA STENCIL ONE again with a light
// procedural worn treatment (Rubik Dirt read "a little too
// blurry" — same worn style, sharper/thinner/brighter);
// and the magic eye's ring is an aged GOLD retaining bezel
// with fine scratches (it read "almost like a graphic" in
// clean white). (docs/gui-v20-context.png = the A render at
// true native size on the 1440x900 canvas, as v17–v19.)
// v21 shots: gui-v21-a.png / gui-v21-b.png ("warble") /
// gui-v21-off.png ("poweroff"), same tokens. The lettering
// becomes SMOKUM — a vintage Western poster face after
// Nathan's "WESTERN GRANDE" specimen — with a very subtle
// spray-paint treatment (overspray halo + edge speckle) in
// place of the worn pinholes; EVERY string shrinks by
// about a third and the tracking tightens to near-natural
// (v20's lettering had gotten too big, too spaced out and
// overlapping — screws and labels included), and the
// flange numerals are bright pure white again, a touch
// smaller. (docs/gui-v21-context.png = the A render at
// true native size on the 1440x900 canvas, as v17–v20.)
// v22 shots: gui-v22-a.png / gui-v22-b.png ("warble") /
// gui-v22-off.png ("poweroff"), same tokens. The spray
// paint and the whole pre-rendered text-image path are
// DELETED — all lettering and the flange numerals are
// direct vector text in clean pure #FFFFFF; Smokum is
// set a step smaller and stretched 1.2x horizontally;
// the numerals grow to 16 pt for visibility on the black
// flange; the BITEY wordmarks clear the corner screws
// and the strip's screws match the module's size.
// (docs/gui-v22-context.png = the A render at true
// native size on the 1440x900 canvas, as v17–v21.)
// v23 shots: gui-v23-a.png / gui-v23-b.png ("warble") /
// gui-v23-off.png ("poweroff"), same tokens. The
// lettering becomes SANCREEK — a genuinely BOLDER
// Western slab (Smokum's hairline strokes read as grey
// mush at panel sizes) — set ~30% bigger than v22 and
// stretched 1.5x; the flange numerals keep v22's 16 pt
// height in the bolder, wider face. Clean direct vector
// text only. (docs/gui-v23-context.png = the A render
// at true native size on the 1440x900 canvas, as
// v17–v22.)
// v24 shots: gui-v24-a.png / gui-v24-b.png ("warble") /
// gui-v24-off.png ("poweroff"), same tokens. Sancreek
// is rejected; the lettering is SMOKUM EMBOLDENED —
// Nathan's pick from the four-face specimen sheet
// (docs/font-sheet-v24.png) — the Western face with a
// FreeType-stroker outline expansion (3.5% of em) baked
// into a derived font, at v23's metrics. The module
// wordmark reads 'WESTERN COMPRESSOR' (the strip keeps
// 'BITEY'), both wordmarks ride on their top-left
// screw's centreline, the eye sits 5 virtual px higher,
// and 'GAIN REDUCTION' is back under the eye.
// v25 shots: gui-v25-a.png / gui-v25-b.png ("warble") /
// gui-v25-off.png ("poweroff"), same tokens. Lettering
// is SMOKUM EMBOLDENED THIN (the bake at 2.34% of em),
// a little bigger (labels 14.5, markings 14); numerals
// keep the v24 bake at 17.5 pt on the widened flange
// (R 100, ring R 90). The eye is up at y 22, the POWER
// cluster down a hair, the knob stack down (centre
// 310), and the O/P pots are spread and enlarged.
// v26 shots: gui-v26-a.png / gui-v26-b.png ("warble") /
// gui-v26-off.png ("poweroff"), same tokens. Small
// moves back: the eye stack settles down partway (eye
// centre 77 -> 86, caption baseline 150 -> 159), the
// POWER cluster descends +6 further (line 180 -> 186),
// ALL lettering thins to SMOKUM EMBOLDENED XTHIN (the
// bake at 1.42% of em, numerals included) at v25's
// sizes, and the strip's BITEY wordmark mirrors to the
// top-RIGHT screw.
// v27 shots: gui-v27-a.png / gui-v27-b.png ("warble") /
// gui-v27-off.png ("poweroff"), same tokens. 'BITEY'
// moves off the strip (its top is now plain panel) onto
// the compressor module's top-right screw, mirrored
// against WESTERN COMPRESSOR; the POWER cluster rises
// onto the eye-hub line (toggle centre y 86) and the
// OUTPUT section rises onto the input-knob centre line
// (pot centres y 310).
// v28 shots: gui-v28-a.png / gui-v28-b.png ("warble") /
// gui-v28-off.png ("poweroff"), same tokens. The module's
// two wordmarks swap sides: 'BITEY' to the top-left screw
// anchor, 'WESTERN COMPRESSOR' to the top-right. Everything
// else as v27.
// v29 shots: gui-v29-a.png / gui-v29-b.png ("warble") /
// gui-v29-off.png ("poweroff"), same tokens. The wordmarks
// leave the top band: 'WESTERN COMPRESSOR' sits at the
// very bottom of the compressor module, centred between
// its bottom corner screws; 'BITEY' returns at the very
// bottom of the power strip, centred between its bottom
// screws. Both top bands are bare panel and screws.
// Addition: all control assemblies rise 10 virtual px
// as wholes (wordmarks stay screw-anchored), lifting
// the LIMIT/MOD switches off the bottom wordmark band.
// Everything else as v28.
// v30 shots: gui-v30-a.png / gui-v30-b.png ("warble") /
// gui-v30-off.png ("poweroff"), same tokens. EXPERIMENTAL
// layout trial ("let's try one where…"): the LIMIT/MOD
// switch clusters relocate UP to flank the magic eye —
// toggle centres at virtual (108, 76) and (332, 76), on
// the eye-hub line with the POWER toggle; words above,
// IN/OUT beside the throws, section jewels on the outer
// flanks at (70, 76) / (370, 76). The module's bottom
// corners are empty panel. Everything else as v29.
// v31 shots: gui-v31-a.png / gui-v31-b.png ("warble") /
// gui-v31-off.png ("poweroff"), same tokens. The
// flanking LIMIT/MOD clusters nudge OUTWARD a little
// — 15 virtual px each way as whole assemblies:
// toggle centres (93, 76) / (347, 76), section jewels
// at (55, 76) / (385, 76), same height, still on the
// eye-hub line. Everything else as v30.
// v32 shots: gui-v32-a.png / gui-v32-b.png ("warble") /
// gui-v32-off.png ("poweroff"), same tokens. The
// LIMIT/MOD clusters return to the module's bottom
// corners: toggle centres (86, 400) / (354, 400) —
// on the line through the flange's bottom edge —
// section jewels at (48, 400) / (392, 400). The
// strip does not move. Everything else as v31.
// v33 shots (audio v3 verification): two new tokens —
//   knob=<0..10>  park the knob at an exact value (default 7.0,
//                 the documented session endpoint where the
//                 crushed path equals the dry path). The knob
//                 series knob=0/2.5/5/7.5/10 proves the v3 flange
//                 fix: numerals ascend CLOCKWISE and the numeral
//                 under the fixed index equals the value.
//   gr=<dB>       magic-eye display override in dB (default 0 —
//                 the standard A/B/off set keeps the 0 dB rest
//                 override per Nathan's standing instruction).
//                 The eye series gr=0/10/25 proves the v3 eye
//                 fix: the whole disc dims uniformly with GR —
//                 no wedge/sector at any value.
// v33 shots: docs/gui-v33-knob-{0,2.5,5,7.5,10}.png,
// docs/gui-v33-eye-{0,10,25}.png, plus the standard
// gui-v33-a.png / gui-v33-b.png ("warble") /
// gui-v33-off.png ("poweroff") set.
// v34 shots (ENGINE v4 + display replacement): the standard
// set gui-v34-a.png / gui-v34-b.png ("warble") /
// gui-v34-off.png ("poweroff") — now showing the ATTACK /
// RELEASE toggles flanking the new 202W-style GR display
// (defaults: attack FAST up, release SLOW down) — plus the
// GR proof series gui-v34-gr-{0,10,25}.png via the gr=
// token: the display's lit field narrows symmetrically as
// GR rises (full glow / partly closed / near-dark).
// v35 shots: gui-v35-a.png / gui-v35-b.png ("warble") /
// gui-v35-off.png ("poweroff") + gui-v35-gr-{0,10,25}.png,
// same tokens. Two changes: (1) the ATTACK / RELEASE
// clusters drop down — words on the GAIN REDUCTION caption
// line (baseline 149), toggles directly under their words
// at virtual centres (93, 186) / (347, 186), FAST/SLOW
// still on the outer flanks; (2) the display's rib field
// gains the 202W's woven centre band of interlocking
// curved triangles, closing with the same converging-
// darkness law (the gr= series shows the patterned field
// narrowing: full woven glow / partly closed / narrow
// patterned centre strip).
// v49 shots: gui-v49-a.png / gui-v49-off.png ("poweroff") /
// gui-v49-gr-{20,25}.png + gui-v49-display-crop.png
// (the 0 dB display at 3x). v49 changes (Nathan, on the
// v48 stills): RIB SHADING DEPTH ONLY — "make all the
// vertical shading slightly darker so it looks deeper
// and more 3D, but taper it off toward the edges." In
// buildGrFieldImage the rib louver's two amplitudes
// scale together about their 0.62 base by
// 1 + 0.18 * cos^2(pi * (u - 1/2)): the full +18% of
// shading contrast at the field centre (groove point
// 0.425 -> 0.390, crest-to-valley swing 0.615 -> 0.726),
// easing to ZERO extra depth at the far edges, whose
// columns keep exactly the v48 shading. The ramp's
// colour stops, the closing masks, the fin span, the
// geometry and the lit law do not move.
// v51 shots: gui-v51-a.png ("knob=5 limitout" — the
// default opening state, as v50-a) / gui-v51-b.png
// ("knob=5 limitout gr=12" — driven at exactly 12 dB so
// the new scale can be checked against the fronts).
// v51 change (Nathan): a GR SCALE silk-screened under
// the display — symmetric ticks + negative numerals
// (0 / -6 / -12 / -20 / -28) positioned by the meter's
// own lit law, so each tick stands where its dark front
// comes to rest; the centre tick is the -28 meeting
// point. Editor artwork only; the meter field, masks,
// palette and law are untouched (build-report-v55.md).
// v56 shots: gui-v56-a.png ("knob=5 limitout warble" —
// MOD in, A selected, the A lamp lit) / gui-v56-b.png
// ("knob=5 limitout warble phaser" — MOD in, P selected,
// the P lamp lit) + gui-v56-mod-crop.png (2x MOD corner).
// v56 changes (Nathan): the A/P selector becomes a round
// PUSH BUTTON (MiniPushButton — the v53-v55 mini bat is
// dropped: "let's just make it a push button"), the whole
// assembly shifts +6.36 virtual px right so the A ink's
// left edge stands in the IN/OUT labels' ink column
// (378.18), and the ACTIVE flavour's letter lights steady
// in the jewels' amber while MOD is in and POWER is on
// (the MOD jewel's own logic — no pulsing; Nathan dropped
// the in-sync flashing). Editor artwork only
// (build-report-v60.md).
// v53 shots: gui-v53-a.png ("knob=5 limitout" — the
// default opening state: INPUT 5, LIMIT, MOD out, the
// new A/P mini toggle at A) / gui-v53-b.png ("knob=5
// limitout phaser" — same, lever toward P).
// v53 change (Nathan): the MOD section gains its A/P
// flavour selector — a miniature silver bat toggle on
// the MOD cluster's centre line, right of the IN/OUT
// silk, 'A' lettered left / 'P' right, bound to
// 'modPhaser' (A = the Reel ADT, default; P = the
// Phase 90 of docs/mod-phaser-design.md). Editor
// artwork + one component; the MOD word, jewel and
// IN/OUT behaviour are unchanged, and nothing else on
// the face moves (build-report-v57.md).
// v54 shots: gui-v54-a.png ("knob=5 limitout" — the
// default opening state, A/P lever at A) / gui-v54-b.png
// ("knob=5 limitout phaser" — lever toward P).
// v54 change (Nathan, correcting v53): the A/P mini
// toggle moves off the MOD cluster's centre line up to
// the MOD WORD's own line, reading as a continuation of
// it — MOD, the panel's own tracked word space (5.45
// virtual px, measured inside GAIN REDUCTION), 'A', the
// toggle, 'P'. Same component, binding and legend; the
// v53 slot by the module edge is plain panel again.
// Editor artwork only (build-report-v58.md).
// v52 shots: gui-v52-a.png ("knob=5 limitout") /
// gui-v52-b.png ("knob=5 limitout gr=10" — fronts on
// the -10 ticks) / gui-v52-c.png ("knob=5 limitout
// gr=20" — fronts met at the centre, under the glyph).
// v52 changes (Nathan, on v51): the arrangement is
// approved but the RANGE is too big — the meter
// rescales to 20 dB full-scale: display law
// lit = (1 - gr/20)^0.85 (DISPLAY ONLY — the audio
// path and real GR are untouched; deeper reduction
// simply tops the meter out) and the scale set becomes
// 0 / -5 / -10 / -15 / -20 under the same law, centre
// tick -20. Addendum: the centre label is the INFINITY
// glyph — a stroked vector lemniscate drawn in
// PluginEditor, not text (the Smokum bake has no
// U+221E). Editor artwork only (build-report-v56.md).
// v48 shots: gui-v48-a.png / gui-v48-off.png ("poweroff") /
// gui-v48-gr-{20,25}.png + gui-v48-display-crop.png
// (the 0 dB display at 3x). v48 changes (Nathan, on the
// v47 stills): MASK FALLOFF ONLY — the centre brightness
// is approved, but the gradient falls too far to the
// edges, so the centre-to-edge darkening is reduced by
// about a third: the leading edge stays (28, 155, 65),
// the delta (5, 26, 11) shrinks to (3, 17, 7) and the
// mask floor rises (23, 129, 54) -> (25, 138, 58). The
// field ramp's dark end re-anchors on the new floor
// (its intermediate stops keep their relative positions;
// the v41 invariant holds), the dead-glass gradient
// tracks in step, and the field BRIGHT stops, geometry
// and lit law do not move.
// v47 shots: gui-v47-a.png / gui-v47-off.png ("poweroff") /
// gui-v47-gr-{20,25}.png + gui-v47-display-crop.png
// (the 0 dB display at 3x). v47 changes (Nathan, on the
// v46 stills): MASK LADDER ONLY — the dark triangles
// toward the centre are "pretty good" but it gets too
// dark by the edges, so the gradient's shape stays and
// the whole ladder shifts up one step: mask floor
// (18, 103, 43) -> (23, 129, 54) (v46's leading edge
// becomes the edges' brightness), leading edge
// (23, 129, 54) -> (28, 155, 65). The field ramp's
// dark end steps x1.25 with it (floor lands exactly on
// the new mask floor; the v41 invariant holds), the
// dead-glass gradient tracks in step, and the field
// BRIGHT stops, geometry and lit law do not move.
// v46 shots: gui-v46-a.png / gui-v46-off.png ("poweroff") /
// gui-v46-gr-{0,20,25}.png + gui-v46-display-crop.png
// (the 0 dB display at 3x). v46 changes (Nathan, on the
// v45 stills): (1) the v45 meter was a little TOO short
// — the frame grows ~15% taller than v45: virtual
// (124, 45.8, 192, 60.4), same width, still centred
// (220, 76), artwork re-rendered natively (height line:
// 70 -> 52.5 -> 60.4 across v44/v45/v46); (2) a global
// brightening of the meter palette — the field ramp's
// dark end steps x1.25 (floor (14, 82, 34) ->
// (18, 103, 43)), its bright stops x1.20 (peak
// (138, 250, 150) -> (166, 255, 180), G clamped at
// 255), the mask gradient steps up with it (floor
// (18, 103, 43), leading edge (23, 129, 54), shape
// unchanged), and the dead-glass gradient tracks in
// step ((19, 90, 36) -> (11, 59, 24)). Nothing outside
// the meter artwork moves.
// v45 shots: gui-v45-a.png / gui-v45-b.png ("warble") /
// gui-v45-off.png ("poweroff") + gui-v45-gr-{0,10,20,25}.png
// + gui-v45-display-crop.png (the 0 dB display at 3x).
// (The v37–v44 iterations — bow-tie field, static artwork
// + masks, palette lifts — are recorded in their build
// reports, docs/build-report-v37..v46.md.) v45 changes
// (Nathan, on 1.1.6): (1) the display assembly is a
// QUARTER SHORTER — frame virtual (124, 49.75, 192, 52.5),
// same width, still centred (220, 76), artwork rendered
// natively at the new height; (2) the closing masks are
// no longer flat — a horizontal gradient: the leading
// edge rides at the floor brightened ~25% ((18, 103, 43))
// falling to the floor (14, 82, 34) at the field edges,
// the rib shading still reading through the feather.
// v36 shots: gui-v36-a.png / gui-v36-b.png ("warble") /
// gui-v36-off.png ("poweroff") + gui-v36-gr-{0,10,25}.png
// + gui-v36-display-crop.png (the 0 dB display at 3x).
// Four changes: (1) the display field is rendered
// PER-PIXEL (buildGrFieldImage) — the woven band of bright
// diamonds and smooth curved triangles formed by
// per-column vertical shading alone; the v35 teeth, their
// feather and the shading overlay (the render's horizontal
// lines) are gone; (2) the GAIN REDUCTION caption rides
// ABOVE the display (baseline 30); ATTACK/RELEASE stay at
// 149; (3) the bottom clusters swap jewel <-> IN/OUT —
// jewels to the inner flanks (125/315, 400), labels to the
// outer; (4) the strip gains a LINK cluster under the
// raised OUTPUT caption (baseline 211): LINK/UNLINK left
// of the toggle, jewel right of it, toggle plate bottom
// on the pots' top-dot line (y ~270); 'outLink' defaults
// linked, so its jewel is lit in these stills.
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI gui;

    // GUI variant / toggle-state selection (exploration only).
    bool warbleOn = false;
    bool limitIn = true;
    bool powerOn = true;
    bool phaserOn = false;     // gui-v53: 'modPhaser' (default A)
    float knobVal = 7.0f;      // v33: knob=<0..10> token
    float grOverride = 0.0f;   // v33: gr=<dB> token
    for (int i = 2; i < argc; ++i) {
        const juce::String v(argv[i]);
        if (v.startsWith("knob=")) {
            knobVal = v.substring(5).getFloatValue();
        } else if (v.startsWith("gr=")) {
            grOverride = v.substring(3).getFloatValue();
        } else if (v == "yellow") {
            osc72gui::setKnobColour(osc72gui::KnobColour::yellow);
            osc72gui::setBeatenUp(true);
        } else if (v == "green") {
            osc72gui::setKnobColour(osc72gui::KnobColour::green);
            osc72gui::setBeatenUp(true);
        } else if (v == "black") {
            osc72gui::setKnobColour(osc72gui::KnobColour::black);
            osc72gui::setBeatenUp(true);
        } else if (v == "warble") {
            warbleOn = true;
        } else if (v == "phaser") {
            phaserOn = true;
        } else if (v == "limitout") {
            limitIn = false;
        } else if (v == "poweroff") {
            powerOn = false;
        }
    }

    Osc72Processor proc;
    proc.prepareToPlay(48000.0, 512);
    if (auto* p = proc.apvts.getParameter("allTheWay"))
        p->setValueNotifyingHost(knobVal / 10.0f);
    if (auto* p = proc.apvts.getParameter("warble"))
        p->setValueNotifyingHost(warbleOn ? 1.0f : 0.0f);
    if (auto* p = proc.apvts.getParameter("modPhaser"))
        p->setValueNotifyingHost(phaserOn ? 1.0f : 0.0f);
    if (auto* p = proc.apvts.getParameter("limit"))
        p->setValueNotifyingHost(limitIn ? 1.0f : 0.0f);
    if (auto* p = proc.apvts.getParameter("power"))
        p->setValueNotifyingHost(powerOn ? 1.0f : 0.0f);

    // Run some audio through so the DSP state is live.
    juce::AudioBuffer<float> buf(2, 4096);
    juce::MidiBuffer midi;
    for (int r = 0; r < 10; ++r) {
        auto* dl = buf.getWritePointer(0);
        auto* dr = buf.getWritePointer(1);
        for (int i = 0; i < 4096; ++i) {
            dl[i] = 0.03f * (float)std::sin(i * 0.05);
            dr[i] = 0.02f * (float)std::sin(i * 0.063 + 1.0);
        }
        proc.processBlock(buf, midi);
    }

    // v15: the magic eye is shown AT REST for the still — 0 dB GR,
    // the full glowing green disc (display only — the audio path
    // is untouched, and the live plugin eye still shows real GR).
    // Nathan wants to judge the colour composition: all that
    // green against the gold and the lettering. Set BEFORE the
    // editor is created so the editor initialises from it.
    // (v1–v14 stills used a 21 dB override for a part-closed eye.)
    // v33: the override value comes from the gr=<dB> token
    // (default 0 dB — the standard set keeps the rest override).
    proc.setGainReductionDisplayOverride(grOverride);

    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setVisible(true);

    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), false, 2.0f);   // 2x of the editor window (352x253 through v32; 704x506 from audio v2)
    juce::File f(argc > 1 ? argv[1] : "/tmp/osc72-gui.png");
    f.deleteFile();
    juce::FileOutputStream fos(f);
    if (fos.openedOk()) {
        juce::PNGImageFormat png;
        png.writeImageToStream(img, fos);
    }
    return 0;
}
