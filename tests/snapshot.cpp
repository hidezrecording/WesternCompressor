// tests/snapshot.cpp — headless GUI snapshot for visual verification.
// Renders the plugin editor to a PNG without a display (xvfb).
// Usage: OscSnapshot [output.png] [tokens...]
//   yellow | green = v7 A/B knob colour + beaten-up case
//                    (default = shipped v6: red knob, std weathering)
//   black          = v18 black-pot variant (gloss black knob +
//                    flange) + beaten-up case
//   warble         = 'mod' toggle IN (default: out)
//   limitout       = 'limit' toggle OUT (default: in)
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
// The knob is parked at 7.0 for the screenshot — the documented
// session endpoint where the crushed path equals the dry path.
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
    for (int i = 2; i < argc; ++i) {
        const juce::String v(argv[i]);
        if (v == "yellow") {
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
        } else if (v == "limitout") {
            limitIn = false;
        } else if (v == "poweroff") {
            powerOn = false;
        }
    }

    Osc72Processor proc;
    proc.prepareToPlay(48000.0, 512);
    if (auto* p = proc.apvts.getParameter("allTheWay"))
        p->setValueNotifyingHost(0.7f);
    if (auto* p = proc.apvts.getParameter("warble"))
        p->setValueNotifyingHost(warbleOn ? 1.0f : 0.0f);
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
    proc.setGainReductionDisplayOverride(0.0f);

    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setVisible(true);

    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), false, 2.0f);   // v18: 2x of the 352x253 window
    juce::File f(argc > 1 ? argv[1] : "/tmp/osc72-gui.png");
    f.deleteFile();
    juce::FileOutputStream fos(f);
    if (fos.openedOk()) {
        juce::PNGImageFormat png;
        png.writeImageToStream(img, fos);
    }
    return 0;
}
