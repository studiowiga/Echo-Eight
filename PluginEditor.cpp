#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
const Colour kSilk  (0xffe6ecf1);   // serigraphie blanche du visuel
const Colour kIce   (0xffbfe6ff);   // bleu glace des LED
}

// =============================================================================
PedalView::PedalView (EightEchoProcessor& p) : proc (p)
{
    // images tirees du visuel : pedale (potards sans repere, LED eteintes), LED allumees,
    // reperes des potards, lentilles allumees (diffusion de lumiere calculee sur l'image)
    bg      = ImageCache::getFromMemory (BinaryData::background_png, BinaryData::background_pngSize);
    ledsOn  = ImageCache::getFromMemory (BinaryData::leds_on_png, BinaryData::leds_on_pngSize);
    x2On    = ImageCache::getFromMemory (BinaryData::x2_on_png, BinaryData::x2_on_pngSize);
    syncOn  = ImageCache::getFromMemory (BinaryData::sync_on_png, BinaryData::sync_on_pngSize);
    lensIce = ImageCache::getFromMemory (BinaryData::lens_left_ice_png, BinaryData::lens_left_ice_pngSize);
    lensRed = ImageCache::getFromMemory (BinaryData::lens_right_red_png, BinaryData::lens_right_red_pngSize);
    lensGreen  = ImageCache::getFromMemory (BinaryData::lens_right_green_png, BinaryData::lens_right_green_pngSize);
    lensOrange = ImageCache::getFromMemory (BinaryData::lens_right_orange_png, BinaryData::lens_right_orange_pngSize);
    pointers[0] = ImageCache::getFromMemory (BinaryData::pointer0_png, BinaryData::pointer0_pngSize);
    pointers[1] = ImageCache::getFromMemory (BinaryData::pointer1_png, BinaryData::pointer1_pngSize);
    pointers[2] = ImageCache::getFromMemory (BinaryData::pointer2_png, BinaryData::pointer2_pngSize);
    pointers[3] = ImageCache::getFromMemory (BinaryData::pointer3_png, BinaryData::pointer3_pngSize);
    pointers[4] = ImageCache::getFromMemory (BinaryData::pointer4_png, BinaryData::pointer4_pngSize);
    setInterceptsMouseClicks (true, false);

    auto addKnob = [this] (const char* id, Point<float> c, float r, bool isMode)
    {
        knobs.push_back ({ id, c, r, isMode, nullptr, 0.5f, 0.5 });
    };
    // centres mesures sur le visuel (pixels de l'image)
    // centre du chapeau (axe de rotation) mesure sur le visuel
    addKnob ("elevel",   { 204.0f, 157.0f }, 54, false);
    addKnob ("mix",      { 394.5f, 157.0f }, 54, false);
    addKnob ("feedback", { 203.5f, 329.0f }, 54, false);
    addKnob ("time",     { 394.5f, 328.0f }, 54, false);
    addKnob ("mode",     { 396.5f, 495.0f }, 54, true);

    for (size_t k = 0; k < knobs.size(); ++k)
    {
        auto* par = proc.apvts.getParameter (knobs[k].id);
        knobs[k].def = par->getDefaultValue();
        knobs[k].att = std::make_unique<ParameterAttachment> (*par, [this, k] (float v) { knobs[k].value = v; repaint(); });
        knobs[k].att->sendInitialUpdate();
    }
    longAtt = std::make_unique<ParameterAttachment> (*proc.apvts.getParameter ("long"), [this] (float v) { longOn = v > 0.5f; repaint(); });
    longAtt->sendInitialUpdate();
    syncAtt = std::make_unique<ParameterAttachment> (*proc.apvts.getParameter ("sync"), [this] (float v) { syncParam = v > 0.5f; repaint(); });
    syncAtt->sendInitialUpdate();
    startTimerHz (30);
}

PedalView::~PedalView() { stopTimer(); }

void PedalView::setParam (const char* id, float v)
{
    if (auto* par = proc.apvts.getParameter (id))
    {
        par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 (v)); par->endChangeGesture();
    }
}

// ----------------------------------------------------------------------------
Point<float> PedalView::modeLabelPos (int i) const
{
    return knobs[4].c.getPointOnCircumference (63.0f, modeAngle (i));
}

// etiquettes alignees vers l'exterieur (a gauche du potard : alignees a droite, etc.)
Rectangle<float> PedalView::modeLabelRect (int i, Justification& just) const
{
    const auto p = modeLabelPos (i);
    const float s = std::sin (modeAngle (i)), w = 72.0f, h = 14.0f;
    if (s > 0.3f)  { just = Justification::centredLeft;  return { p.x + 1, p.y - h * 0.5f, w, h }; }
    if (s < -0.3f) { just = Justification::centredRight; return { p.x - w - 1, p.y - h * 0.5f, w, h }; }
    just = Justification::centred;
    return { p.x - w * 0.5f, p.y - h - 1, w, h };
}

int PedalView::knobAt (Point<float> p) const
{
    for (size_t k = 0; k < knobs.size(); ++k)
        if (p.getDistanceFrom (knobs[k].c) <= knobs[k].r) return (int) k;
    return -1;
}

int PedalView::modeLabelAt (Point<float> p) const
{
    for (int i = 0; i < wiga::kNumModes; ++i)
    {
        Justification j (Justification::centred);
        auto r = modeLabelRect (i, j);
        if (r.expanded (2, 2).contains (p) && p.getDistanceFrom (knobs[4].c) > knobs[4].r) return i;
    }
    return -1;
}

int PedalView::slotAt (Point<float> p) const
{
    for (int i = 0; i < EightEchoProcessor::kNumSlots; ++i)
        if (p.x > 22 && p.x < 100 && std::abs (p.y - slotY (i)) < 20) return i;
    return -1;
}

// ----------------------------------------------------------------------------
void PedalView::mouseMove (const MouseEvent& e)
{
    auto p = e.position;
    const bool hot = knobAt (p) >= 0 || modeLabelAt (p) >= 0 || slotAt (p) >= 0
                  || p.getDistanceFrom (cSync) < 28 || p.getDistanceFrom (cX2) < 36
                  || p.getDistanceFrom (cPrev) < 18 || p.getDistanceFrom (cNext) < 18
                  || p.getDistanceFrom (cTap) < 52 || p.getDistanceFrom (cBypass) < 52;
    setMouseCursor (hot ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void PedalView::mouseDown (const MouseEvent& e)
{
    const auto p = e.position;
    if (e.mods.isPopupMenu()) { if (onBackgroundRightClick) onBackgroundRightClick (e); return; }

    // footswitches
    if (p.getDistanceFrom (cBypass) < 52)
    {
        pressed = BYPASSFS;
        if (auto* par = proc.apvts.getParameter ("footswitch")) { par->beginChangeGesture(); par->setValueNotifyingHost (1.0f); }
        repaint(); return;
    }
    if (p.getDistanceFrom (cTap) < 52)
    {
        pressed = TAPFS; tapPressTime = Time::getMillisecondCounterHiRes(); twistSent = false;
        if (auto* par = proc.apvts.getParameter ("tap")) { par->beginChangeGesture(); par->setValueNotifyingHost (1.0f); }
        repaint(); return;
    }
    if (p.getDistanceFrom (cX2) < 36) { setParam ("long", longOn ? 0.0f : 1.0f); return; }
    // SYNC : cale le delay sur le tempo du DAW (re-clic = retour au TIME / tap)
    if (p.getDistanceFrom (cSync) < 28) { syncAtt->setValueAsCompleteGesture (syncParam ? 0.0f : 1.0f); return; }
    if (p.getDistanceFrom (cPrev) < 18 || p.getDistanceFrom (cNext) < 18)
    {
        const int n = EightEchoProcessor::kNumSlots, cur = proc.getCurrentSlot();
        proc.loadSlot (p.getDistanceFrom (cPrev) < 18 ? (cur + n - 1) % n : (cur + 1) % n);
        repaint(); return;
    }
    if (int s = slotAt (p); s >= 0)
    {
        slotPress = s; slotPressTime = Time::getMillisecondCounterHiRes(); slotSaved = false;
        repaint(); return;
    }
    if (int m = modeLabelAt (p); m >= 0 && knobAt (p) < 0) { setParam ("mode", (float) m); return; }
    if (int k = knobAt (p); k >= 0)
    {
        dragKnob = k; dragStartY = p.y; dragStartVal = knobs[(size_t) k].value;
        dragStartIndex = roundToInt (knobs[(size_t) k].value);
        knobs[(size_t) k].att->beginGesture();
    }
}

void PedalView::mouseDrag (const MouseEvent& e)
{
    if (dragKnob < 0) return;
    auto& k = knobs[(size_t) dragKnob];
    const float dy = dragStartY - e.position.y;
    if (k.isMode)
    {
        const int idx = jlimit (0, wiga::kNumModes - 1, dragStartIndex + (int) std::floor (dy / 18.0f + 0.5f));
        k.att->setValueAsPartOfGesture ((float) idx);
    }
    else
    {
        const float sens = e.mods.isShiftDown() ? 900.0f : 260.0f;   // Maj = reglage fin
        auto* par = proc.apvts.getParameter (k.id);
        const float v = jlimit (0.0f, 1.0f, dragStartVal + dy / sens);
        k.att->setValueAsPartOfGesture (par->convertFrom0to1 (v));
    }
}

void PedalView::mouseUp (const MouseEvent&)
{
    if (dragKnob >= 0) { knobs[(size_t) dragKnob].att->endGesture(); dragKnob = -1; }
    if (pressed == BYPASSFS)
        if (auto* par = proc.apvts.getParameter ("footswitch")) { par->setValueNotifyingHost (0.0f); par->endChangeGesture(); }
    if (pressed == TAPFS)
    {
        if (auto* par = proc.apvts.getParameter ("tap")) { par->setValueNotifyingHost (0.0f); par->endChangeGesture(); }
        if (twistSent) setParam ("twist", 0.0f);
    }
    if (slotPress >= 0 && ! slotSaved) proc.loadSlot (slotPress);    // clic court = rappel
    slotPress = -1; slotSaved = false;
    pressed = NONE; twistSent = false;
    repaint();
}

void PedalView::saveToSlot (int i)
{
    proc.saveSlot (i);
    flashSlot = i; flashStart = anim;
    repaint();
}

void PedalView::mouseDoubleClick (const MouseEvent& e)
{
    if (int k = knobAt (e.position); k >= 0 && ! knobs[(size_t) k].isMode)
    {
        auto* par = proc.apvts.getParameter (knobs[(size_t) k].id);
        knobs[(size_t) k].att->setValueAsCompleteGesture (par->convertFrom0to1 ((float) knobs[(size_t) k].def));
    }
}

void PedalView::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (int k = knobAt (e.position); k >= 0)
    {
        auto& kn = knobs[(size_t) k];
        auto* par = proc.apvts.getParameter (kn.id);
        if (kn.isMode)
            kn.att->setValueAsCompleteGesture ((float) jlimit (0, wiga::kNumModes - 1, roundToInt (kn.value) + (w.deltaY > 0 ? 1 : -1)));
        else
            kn.att->setValueAsCompleteGesture (par->convertFrom0to1 (jlimit (0.0f, 1.0f, par->getValue() + w.deltaY * 0.08f)));
    }
}

void PedalView::timerCallback()
{
    anim += 1.0f / 30.0f;
    // TAP maintenu : TWIST (et, en LOOP, maintien 2 s = effacer)
    if (pressed == TAPFS && ! twistSent)
    {
        const double held = Time::getMillisecondCounterHiRes() - tapPressTime;
        const bool loop = (int) proc.apvts.getRawParameterValue ("mode")->load() == wiga::LOOP;
        if ((! loop && held > 450.0) || (loop && held > 2000.0))
        {
            if (auto* par = proc.apvts.getParameter ("twist")) { par->beginChangeGesture(); par->setValueNotifyingHost (1.0f); par->endChangeGesture(); }
            twistSent = true;
        }
    }
    // LED 1-7 maintenue 1 s : sauvegarde du reglage en cours dans cette memoire
    if (slotPress >= 0 && ! slotSaved && Time::getMillisecondCounterHiRes() - slotPressTime > 1000.0)
    {
        saveToSlot (slotPress); slotSaved = true;
    }
    if (flashSlot >= 0 && anim - flashStart > 1.2f) flashSlot = -1;
    repaint();
}

// ----------------------------------------------------------------------------
//  Dessin
// ----------------------------------------------------------------------------
void PedalView::drawKnob (Graphics& g, const Knob& k)
{
    // le potard (moletage, reflet conique, ombre) est celui du visuel, fixe : l'eclairage ne tourne pas.
    // Seul le repere blanc, decoupe sur l'image, tourne autour de l'axe du chapeau.
    float angle;
    if (k.isMode) angle = modeAngle (jlimit (0, wiga::kNumModes - 1, roundToInt (k.value)));   // crante sur chaque mode
    else angle = degreesToRadians (-135.0f + 270.0f * k.value);
    const size_t idx = (size_t) (&k - knobs.data());
    const auto& img = pointers[jmin (idx, pointers.size() - 1)];
    const float ax = img.getWidth() * 0.5f, ay = 52.0f;      // axe : 52 px sous le haut du repere
    g.drawImageTransformed (img, AffineTransform::translation (-ax, -ay).rotated (angle).translated (k.c.x, k.c.y), false);
}

void PedalView::drawLedPipe (Graphics& g, int i, float b)
{
    // LED d'origine (tube + diffusion sur la plaque) superposee avec l'intensite voulue
    if (b <= 0.01f) return;
    const float top = (i == 0 ? 95.0f : 0.5f * (slotY (i - 1) + slotY (i)));
    const float bot = (i == EightEchoProcessor::kNumSlots - 1 ? 465.0f : 0.5f * (slotY (i) + slotY (i + 1)));
    const int sy = (int) top - 90, h = (int) (bot - top);
    Graphics::ScopedSaveState ss (g);
    g.setOpacity (jlimit (0.0f, 1.0f, b));
    g.drawImage (ledsOn, 0, (int) top, ledsOn.getWidth(), h, 0, sy, ledsOn.getWidth(), h);
}

void PedalView::drawLens (Graphics& g, bool left, const Image* lit, float amount)
{
    if (lit == nullptr || amount <= 0.01f) return;
    const Point<float> o = left ? Point<float> { 104.0f, 581.0f } : Point<float> { 394.0f, 580.0f };
    Graphics::ScopedSaveState ss (g);
    g.setOpacity (jlimit (0.0f, 1.0f, amount));
    g.drawImageAt (*lit, (int) o.x, (int) o.y);
}

Colour PedalView::checkColour() const
{
    const bool on = std::fmod (anim * 2.5f, 1.0f) < 0.5f;
    const float ph = proc.tapBlinkPhase.load();
    const Colour red (0xffff3324), green (0xff3dff7a), orange (0xffffa424);
    switch (proc.ledState.load())
    {
        case EightEchoProcessor::LED_RED:    return red;
        case EightEchoProcessor::LED_GREEN:  return green;
        case EightEchoProcessor::LED_ORANGE: return orange;
        case EightEchoProcessor::LED_RED_BLINK: return proc.tapModeActive.load() ? (ph < 0.25f ? red : Colours::transparentBlack) : (on ? red : Colours::transparentBlack);
        case EightEchoProcessor::LED_GREEN_BLINK: return on ? green : Colours::transparentBlack;
        case EightEchoProcessor::LED_REDGREEN_BLINK: return ph < 0.25f ? red : green;
        default: return Colours::transparentBlack;
    }
}

String PedalView::statusLine() const
{
    auto& s = proc.apvts;
    const int mode = (int) s.getRawParameterValue ("mode")->load();
    auto pc = [&] (const char* id) { return String (roundToInt (s.getRawParameterValue (id)->load() * 100.0f)); };
    const int slot = proc.getCurrentSlot();
    String t = "P" + String (slot + 1) + (proc.slotMatchesCurrent (slot) ? " " : "*");
    if (flashSlot >= 0) return "SAVED TO MEMORY " + String (flashSlot + 1);
    if (slotPress >= 0 && ! slotSaved && Time::getMillisecondCounterHiRes() - slotPressTime > 250.0)
        return "HOLD TO SAVE IN MEMORY " + String (slotPress + 1) + "...";
    if (mode == wiga::LOOP)
    {
        const char* st[] = { "EMPTY", "REC", "PLAY", "OVERDUB", "STOP" };
        return t + " LOOP " + st[jlimit (0, 4, proc.looperState.load())] + " " + String (proc.loopSeconds.load(), 1) + "s/40s VOL " + pc ("elevel");
    }
    t << " " << wiga::modeName (mode) << " " << String (roundToInt (proc.effectiveDelayMs.load())) << "ms";
    t << " LV" << pc ("elevel") << (mode == wiga::GLT ? " DP" : " FB") << pc ("feedback") << " MX" << pc ("mix");
    if (proc.hostLocked.load()) t << " SYNC" << String (roundToInt (proc.hostBpm.load()));
    else if (syncParam) t << " SYNC?";                                // SYNC demande mais pas de tempo du DAW
    if (proc.tapModeActive.load()) t << " TAP";
    if (proc.twistAmount.load() > 0.05f) t << " TWIST";
    if (proc.glitching.load()) t << " GLT";
    if (proc.warpAmount.load() > 0.05f) t << " WARP";
    if (proc.looperState.load() == 2 || proc.looperState.load() == 3) t << " +LOOP";   // boucle qui tourne sous l'effet
    return t;
}

void PedalView::paint (Graphics& g)
{
    g.drawImage (bg, Rectangle<float> (0, 0, imgW, imgH));

    // ligne d'etat sous "DC 9V"
    {
        const auto txt = statusLine();
        const Rectangle<int> sr (64, 52, 450, 26);
        g.setFont (FontOptions (Font::getDefaultMonospacedFontName(), 17.5f, Font::bold));
        g.setColour (Colours::black.withAlpha (0.45f));                  // ombre : lisible comme une serigraphie
        g.drawFittedText (txt, sr.translated (0, 1), Justification::centred, 1, 0.85f);
        g.setColour (kSilk.withAlpha (0.92f));
        g.drawFittedText (txt, sr, Justification::centred, 1, 0.85f);
    }

    // etiquettes du selecteur de mode (comme la serigraphie DD-8)
    const int modeIdx = jlimit (0, wiga::kNumModes - 1, roundToInt (knobs[4].value));
    g.setFont (FontOptions (11.5f, Font::bold));
    for (int i = 0; i < wiga::kNumModes; ++i)
    {
        Justification just (Justification::centred);
        auto r = modeLabelRect (i, just);
        const bool sel = i == modeIdx;
        auto t1 = knobs[4].c.getPointOnCircumference (knobs[4].r + 2.0f, modeAngle (i)), t2 = knobs[4].c.getPointOnCircumference (knobs[4].r + 6.5f, modeAngle (i));
        g.setColour (sel ? kIce : kSilk.withAlpha (0.65f)); g.drawLine ({ t1, t2 }, 1.6f);
        if (sel) { g.setColour (kIce.withAlpha (0.25f)); g.drawText (wiga::modeName (i), r.translated (0, 0.8f), just); }
        g.setColour (sel ? kIce : kSilk.withAlpha (0.82f));
        g.drawText (wiga::modeName (i), r, just);
    }

    for (auto& k : knobs) drawKnob (g, k);

    // LED des memoires 1 a 7
    const int cur = proc.getCurrentSlot();
    const bool dirty = ! proc.slotMatchesCurrent (cur);
    for (int i = 0; i < EightEchoProcessor::kNumSlots; ++i)
    {
        float b = 0.0f;
        if (i == flashSlot) b = std::fmod ((anim - flashStart) * 5.0f, 1.0f) < 0.5f ? 1.0f : 0.15f;   // confirmation
        else if (i == slotPress && ! slotSaved)
        {
            const float held = (float) ((Time::getMillisecondCounterHiRes() - slotPressTime) / 1000.0);
            b = held > 0.25f ? 0.3f + 0.7f * jlimit (0.0f, 1.0f, (held - 0.25f) / 0.75f) * (0.6f + 0.4f * std::sin (anim * 14.0f)) : 1.0f;
        }
        else if (i == cur) b = dirty ? 0.45f + 0.15f * std::sin (anim * 3.0f) : 1.0f;   // memoire modifiee : LED adoucie
        drawLedPipe (g, i, b);
    }

    // bouton SYNC : le capuchon depoli s'eclaire en bleu glace quand le delay suit le tempo du DAW
    const float ph = proc.tapBlinkPhase.load();
    const float beatFlash = jlimit (0.0f, 1.0f, 1.0f - ph * 4.0f);
    if (syncParam)
    {
        const bool lockedNow = proc.hostLocked.load();
        const float a = lockedNow ? 0.86f + 0.14f * beatFlash                     // respire au tempo
                                  : 0.25f + 0.15f * std::sin (anim * 4.0f);        // pas de tempo du DAW
        Graphics::ScopedSaveState ss (g);
        g.setOpacity (a);
        g.drawImageAt (syncOn, 139, 426);
    }

    // LED de tempo (au-dessus de TAP) et LED CHECK (au-dessus de BYPASS) : lentilles d'origine allumees
    // la LED de tempo bat les temps ; le premier temps de la mesure (DAW) / du tap est plus fort
    const bool twisting = proc.twistAmount.load() > 0.05f;
    const bool accent = proc.tapAccent.load();
    const float peak = (! accent || proc.tapBeat.load() == 0) ? 1.0f : 0.55f;
    const float tempoAmt = twisting ? (std::fmod (anim * 12.0f, 1.0f) < 0.5f ? 1.0f : 0.15f)
                                    : peak * beatFlash;                         // eclair puis extinction douce
    drawLens (g, true, &lensIce, tempoAmt);
    const auto cc = checkColour();
    if (! cc.isTransparent())
    {
        const Image* img = cc == Colour (0xff3dff7a) ? &lensGreen : cc == Colour (0xffffa424) ? &lensOrange : &lensRed;
        drawLens (g, false, img, 1.0f);
    }

    // X2 (temps double) : la serigraphie X2 s'illumine (meme forme, meme taille)
    if (longOn) g.drawImageAt (x2On, 220, 727);

    // footswitches enfonces
    auto pressFs = [&g] (Point<float> c)
    {
        g.setColour (Colours::black.withAlpha (0.28f)); g.fillEllipse (c.x - 36, c.y - 36, 72, 72);
        g.setColour (Colours::white.withAlpha (0.10f)); g.drawEllipse (c.x - 33, c.y - 31, 66, 66, 2.0f);
    };
    if (pressed == TAPFS) pressFs (cTap);
    if (pressed == BYPASSFS) pressFs (cBypass);

    // Studio Wiga entre les deux footswitchs, en bas
    // logo Studio Wiga : integre a la plaque dans background.png (toujours fixe)

}

// =============================================================================
EightEchoEditor::EightEchoEditor (EightEchoProcessor& p) : AudioProcessorEditor (&p), proc (p), pedal (p)
{
    addAndMakeVisible (pedal);
    pedal.onBackgroundRightClick = [this] (const MouseEvent&) { showMenu(); };

    // l'ordre compte : setResizable() d'abord (il cree le coin de redimensionnement),
    // puis notre contrainte stricte, pour que le coin ET l'hote l'utilisent
    setResizable (true, true);
    constrainer.setMinimumSize (baseW, baseH);
    constrainer.setMaximumSize (refW, refH);
    constrainer.setFixedAspectRatio ((double) baseW / (double) baseH);
    constrainer.setMinimumOnscreenAmounts (0x10000, 0x10000, 0x10000, 0x10000);
    setConstrainer (&constrainer);
    const float sc = jlimit (1.0f, maxScale, proc.uiScale.load());
    setSize (roundToInt (baseW * sc), roundToInt (baseH * sc));
}

void EightEchoEditor::setScale (float s)
{
    s = jlimit (1.0f, maxScale, s); proc.uiScale = s;
    setSize (roundToInt (baseW * s), roundToInt (baseH * s));
}

void EightEchoEditor::resized()
{
    // filet de securite : si un hote impose malgre tout une taille plus petite, on revient a la taille par defaut
    if (getWidth() < baseW || getHeight() < baseH)
    {
        juce::Component::SafePointer<EightEchoEditor> self (this);
        juce::MessageManager::callAsync ([self] { if (self != nullptr) self->setSize (baseW, baseH); });
        return;
    }
    const float s = jmin ((float) getWidth() / (float) baseW, (float) getHeight() / (float) baseH);
    proc.uiScale = s;
    const auto a = pedalArea();
    const float ps = a.getHeight() / PedalView::imgH;
    pedal.setBounds (0, 0, (int) PedalView::imgW, (int) PedalView::imgH);
    pedal.setTransform (AffineTransform::scale (ps).translated (a.getX(), a.getY()).scaled (s));
}

Rectangle<float> EightEchoEditor::pedalArea() const
{
    // comme Textural : la pedale fait 1805/2000 de la hauteur de la fenetre, centree
    const float h = (float) baseH * 1805.0f / 2000.0f, w = h * PedalView::imgW / PedalView::imgH;
    return { ((float) baseW - w) * 0.5f, ((float) baseH - h) * 0.5f, w, h };
}

// Motif de la pedale : deux groupes d'anneaux concentriques + le "8" entrelace central
static void drawPedalMotif (Graphics& g, Point<float> c, float sc, float alpha)
{
    g.setColour (Colour (0xffa9c2d6).withAlpha (alpha));
    auto rings = [&] (Point<float> o, float r0, float r1, float step)
    {
        for (float r = r0; r <= r1; r += step)
            g.drawEllipse (c.x + (o.x - r) * sc, c.y + (o.y - r) * sc, 2 * r * sc, 2 * r * sc, 2.2f * sc);
    };
    rings ({ -148, -3 }, 64, 110, 9.0f);
    rings ({  148, -3 }, 64, 110, 9.0f);
    rings ({  -62,  5 }, 28, 92, 9.0f);
    rings ({   62,  5 }, 28, 92, 9.0f);
}

void EightEchoEditor::paint (Graphics& g)
{
    // fond noir, eclairage doux derriere la pedale, motifs d'anneaux de la pedale en transparence
    const float s = currentScale();
    g.addTransform (AffineTransform::scale (s));
    const float W = (float) baseW, H = (float) baseH;
    g.setGradientFill (ColourGradient (Colour (0xff1b1f25), W * 0.5f, H * 0.42f, Colour (0xff060708), W * 0.5f + W * 0.62f, H * 0.42f, true));
    g.fillRect (0.0f, 0.0f, W, H);
    const float cx = W * 0.5f, k = 0.62f;
    drawPedalMotif (g, { cx, 470.0f }, k, 0.12f);
    drawPedalMotif (g, { cx, 22.0f }, k, 0.12f);
    drawPedalMotif (g, { 30.0f, 246.0f }, k * 0.8f, 0.09f);
    drawPedalMotif (g, { W - 30.0f, 246.0f }, k * 0.8f, 0.09f);
    drawPedalMotif (g, { 24.0f, 110.0f }, k * 0.55f, 0.07f);
    drawPedalMotif (g, { W - 24.0f, 382.0f }, k * 0.55f, 0.07f);
    drawPedalMotif (g, { 24.0f, 400.0f }, k * 0.45f, 0.06f);
    drawPedalMotif (g, { W - 24.0f, 96.0f }, k * 0.45f, 0.06f);

    // vignettage leger des bords
    g.setGradientFill (ColourGradient (Colours::transparentBlack, cx, H * 0.5f, Colours::black.withAlpha (0.55f), 0.0f, 0.0f, true));
    g.fillRect (0.0f, 0.0f, W, H);
    // ombre portee sous la pedale (boitier arrondi)
    const auto a = pedalArea();
    const auto body = a.reduced (a.getWidth() * 0.035f, a.getHeight() * 0.02f);
    Path p; p.addRoundedRectangle (body, a.getWidth() * 0.06f);
    DropShadow (Colours::black.withAlpha (0.75f), 22, { 0, 9 }).drawForPath (g, p);
}

void EightEchoEditor::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) showMenu();
}

void EightEchoEditor::showMenu()
{
    auto& s = proc.apvts;
    auto val = [&s] (const char* id) { return s.getRawParameterValue (id)->load(); };
    PopupMenu m, presets, stereo, div, exp, size;
    for (int i = 0; i < proc.getNumPrograms(); ++i) presets.addItem (100 + i, proc.getProgramName (i));
    const char* st[] = { "Independent (default)", "Wide stereo", "Panning (ping-pong)" };
    for (int i = 0; i < 3; ++i) stereo.addItem (200 + i, st[i], true, (int) val ("stereo") == i);
    auto& dn = EightEchoProcessor::tapDivisionNames();
    for (int i = 0; i < dn.size(); ++i) div.addItem (300 + i, dn[i], true, (int) val ("division") == i);
    exp.addItem (401, "Expression > E.LEVEL", true, val ("expLevel") > 0.5f);
    exp.addItem (402, "Expression > FEEDBACK", true, val ("expFeedback") > 0.5f);
    exp.addItem (403, "Expression > TIME", true, val ("expTime") > 0.5f);
    const float cur = currentScale();
    // memes tailles que Textural / Freeze Shimmer (100 % = 1984 x 2000)
    const float pct[] = { 0.6f, 0.75f, 1.0f };
    size.addItem (501, "Default size", true, std::abs (cur - 1.0f) < 0.02f);
    for (int i = 0; i < 3; ++i)
        size.addItem (502 + i, String (roundToInt (pct[i] * 100.0f)) + "%", true, std::abs (cur - pct[i] * maxScale) < 0.02f);
    PopupMenu save;
    for (int i = 0; i < EightEchoProcessor::kNumSlots; ++i) save.addItem (700 + i, "Memory " + String (i + 1));

    m.addSubMenu ("Factory presets (load into current slot P" + String (proc.getCurrentSlot() + 1) + ")", presets);
    m.addSubMenu ("Save current settings to memory", save);
    m.addSeparator();
    m.addSubMenu ("Stereo mode", stereo);
    m.addSubMenu ("Tap division", div);
    m.addItem (601, "Carryover (trails when bypassed)", true, val ("carryover") > 0.5f);
    m.addItem (602, "SYNC : delay locked to host tempo", true, val ("sync") > 0.5f);
    m.addSubMenu ("Expression pedal (EXP parameter)", exp);
    m.addSeparator();
    m.addSubMenu ("Window size", size);

    m.showMenuAsync (PopupMenu::Options(), [this] (int r)
    {
        auto set = [this] (const char* id, float v)
        {
            if (auto* par = proc.apvts.getParameter (id)) { par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 (v)); par->endChangeGesture(); }
        };
        auto toggle = [this, &set] (const char* id) { set (id, proc.apvts.getRawParameterValue (id)->load() > 0.5f ? 0.0f : 1.0f); };
        if (r >= 100 && r < 200) proc.setCurrentProgram (r - 100);
        else if (r >= 200 && r < 300) set ("stereo", (float) (r - 200));
        else if (r >= 300 && r < 400) set ("division", (float) (r - 300));
        else if (r == 401) toggle ("expLevel");
        else if (r == 402) toggle ("expFeedback");
        else if (r == 403) toggle ("expTime");
        else if (r >= 501 && r <= 504) { const float sc[] = { 1.0f, 0.6f * maxScale, 0.75f * maxScale, maxScale }; setScale (sc[r - 501]); }
        else if (r >= 700 && r < 700 + EightEchoProcessor::kNumSlots) pedal.saveToSlot (r - 700);
        else if (r == 601) toggle ("carryover");
        else if (r == 602) toggle ("sync");
    });
}
