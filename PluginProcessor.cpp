#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>

using namespace wiga;

namespace
{
const float kDivMult[] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.75f, 2.0f / 3.0f, 1.0f / 3.0f };

// mode, E.LEVEL, FEEDBACK, TIME (knob 0..1), stereo, long, division
// TIME : 20 + 780 x t^2 ms (GLT : 10 + 390 x t^2, REVERSE : 300 + 4700 x t^2, SHIM : 200 + 600 x t^2)
struct Preset { const char* name; int mode; float level, feedback, time; int stereo; bool longMode; int division; };
const int W = STEREO_WIDE, P = STEREO_PANNING, I = STEREO_INDEPENDENT;
const Preset kPresets[] = {
    // --- classiques ---
    { "Clean Slapback",        STANDARD, 0.55f, 0.05f, 0.33f, I, false, 2 },
    { "Rockabilly Tape",       TAPE,     0.70f, 0.15f, 0.36f, I, false, 2 },
    { "Doubler",               STANDARD, 0.45f, 0.00f, 0.19f, W, false, 2 },
    { "Bread & Butter",        STANDARD, 0.45f, 0.38f, 0.60f, I, false, 2 },
    { "Dotted 8th Arena",      STANDARD, 0.55f, 0.45f, 0.69f, W, false, 4 },
    { "Ping Pong Eighths",     STANDARD, 0.50f, 0.50f, 0.58f, P, false, 3 },
    // --- vintage ---
    { "BBD Crunch Lead",       ANALOG,   0.60f, 0.40f, 0.62f, I, false, 2 },
    { "Dark BBD Wash",         ANALOG,   0.70f, 0.78f, 0.90f, W, false, 2 },
    { "Floating Tape Solo",    TAPE,     0.60f, 0.55f, 0.80f, W, false, 2 },
    { "Long Tape Ambience",    TAPE,     0.55f, 0.65f, 1.00f, W, true,  2 },
    // --- doux / modernes ---
    { "Soft Ballad",           WARM,     0.50f, 0.42f, 0.70f, P, false, 2 },
    { "Jazz Room",             WARM,     0.35f, 0.20f, 0.45f, I, false, 2 },
    { "Lush Chorus Echo",      MOD,      0.55f, 0.40f, 0.55f, I, false, 2 },
    { "80s Stereo Lead",       MOD,      0.50f, 0.50f, 0.75f, W, false, 4 },
    // --- espace ---
    { "Echo Hall",             PLUSRV,   0.55f, 0.45f, 0.75f, W, false, 2 },
    { "Cathedral",             PLUSRV,   0.65f, 0.70f, 1.00f, W, true,  2 },
    { "Heaven Swell",          SHIM,     0.60f, 0.60f, 0.80f, W, false, 2 },
    { "Octave Haze",           SHIM,     0.45f, 0.35f, 0.40f, I, false, 2 },
    { "Dream Swell (hold)",    WARP,     0.38f, 0.45f, 0.65f, W, false, 2 },
    { "Space Freeze (hold)",   WARP,     0.40f, 0.60f, 0.90f, P, true,  2 },
    // --- a l'envers / glitch ---
    { "Backwards Solo",        REVERSE,  1.00f, 0.25f, 0.30f, P, false, 2 },
    { "Backwards Pad",         REVERSE,  0.70f, 0.60f, 0.60f, W, true,  2 },
    { "Machine Gun (hold)",    GLT,      0.48f, 0.70f, 0.35f, I, false, 2 },
    { "Stutter Groove (hold)", GLT,      0.45f, 0.35f, 0.70f, P, false, 3 },
    { "Granular Buzz (hold)",  GLT,      0.45f, 0.75f, 0.15f, I, false, 2 },
    // --- looper ---
    { "Looper",                LOOP,     0.80f, 0.40f, 0.50f, I, false, 2 },
};
constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
}

const juce::StringArray& EightEchoProcessor::tapDivisionNames()
{
    static const juce::StringArray n { "Whole", "Half", "Quarter", "Eighth", "Dotted 8th", "Quarter Triplet", "Eighth Triplet" };
    return n;
}

juce::AudioProcessorValueTreeState::ParameterLayout EightEchoProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> v;
    auto pct = [] (float x, int) { return String (juce::roundToInt (x * 100.0f)) + "%"; };

    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "elevel", 1 }, "E.Level", NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                                                        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                                                        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int)
                                                        { return x < 0.495f ? "Dry " + String (roundToInt ((1 - 2 * x) * 100)) + "%" : x > 0.505f ? "Wet " + String (roundToInt ((2 * x - 1) * 100)) + "%" : String ("Dry+Wet"); })));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "feedback", 1 }, "Feedback", NormalisableRange<float> (0.0f, 1.0f), 0.4f,
                                                        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "time", 1 }, "Time", NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                                                        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    StringArray modes; for (int i = 0; i < kNumModes; ++i) modes.add (modeName (i));
    v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", modes, (int) STANDARD));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "on", 1 }, "Effect On", true));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "footswitch", 1 }, "Footswitch (hold)", false));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "tap", 1 }, "Tap / Loop Stop", false));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "twist", 1 }, "Twist / Loop Clear", false));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "carryover", 1 }, "Carryover", true));
    v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "stereo", 1 }, "Stereo Mode", StringArray { "Independent", "Wide", "Panning" }, 0));
    v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Tap Division", tapDivisionNames(), 2));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "long", 1 }, "Long Delay (x2)", false));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "drykill", 1 }, "Direct Mute", false));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Host Sync", false));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "exp", 1 }, "Expression", NormalisableRange<float> (0.0f, 1.0f), 1.0f,
                                                        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "expLevel", 1 }, "Exp > E.Level", false));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "expFeedback", 1 }, "Exp > Feedback", false));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "expTime", 1 }, "Exp > Time", false));
    return { v.begin(), v.end() };
}

EightEchoProcessor::EightEchoProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pOn = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("on"));
    rMode = p ("mode"); rFootswitch = p ("footswitch"); rTap = p ("tap"); rTwist = p ("twist");
    rExp = p ("exp"); rLevel = p ("elevel"); rFeedback = p ("feedback"); rTime = p ("time");
    rExpLevel = p ("expLevel"); rExpFeedback = p ("expFeedback"); rExpTime = p ("expTime"); rLong = p ("long");
    rSync = p ("sync"); rDivision = p ("division"); rDryKill = p ("drykill"); rMix = p ("mix");
    rStereo = p ("stereo"); rOn = p ("on"); rCarryover = p ("carryover");
    taps.reserve (8);
    for (int i = 0; i < numQueued; ++i) { pendingValue[i].store (0.0f); pendingSet[i].store (false); }
    resetMidiDefaults();
    initSlots();
    startTimerHz (60);
}

EightEchoProcessor::~EightEchoProcessor()
{
    stopTimer();
}

// ---------------------------------------------------------------------------
//  Anti-gel v2 (Studio Wiga) : jamais d'appel a l'hote (setValueNotifyingHost) depuis le thread audio.
//  Le footswitch met la valeur en file ; un timer du thread de messages la transmet au DAW.
// ---------------------------------------------------------------------------
const char* EightEchoProcessor::queuedId (int i)
{
    static const char* ids[numQueued] = { "on", "elevel", "mix", "feedback", "time", "exp", "mode", "long", "sync" };
    return ids[juce::jlimit (0, numQueued - 1, i)];
}

void EightEchoProcessor::setParamFromAudio (const char* id, float plain)
{
    for (int i = 0; i < numQueued; ++i)
        if (std::strcmp (id, queuedId (i)) == 0)
        {
            pendingValue[i].store (plain, std::memory_order_relaxed);
            pendingSet[i].store (true, std::memory_order_release);
            return;
        }
}

float EightEchoProcessor::queuedOrCurrent (const char* id, std::atomic<float>* current) const
{
    for (int i = 0; i < numQueued; ++i)
        if (std::strcmp (id, queuedId (i)) == 0 && pendingSet[i].load (std::memory_order_acquire))
            return pendingValue[i].load (std::memory_order_relaxed);
    return current->load (std::memory_order_relaxed);
}

void EightEchoProcessor::timerCallback()
{
    for (int i = 0; i < numQueued; ++i)
        if (pendingSet[i].exchange (false, std::memory_order_acquire))
            if (auto* prm = apvts.getParameter (queuedId (i)))
                prm->setValueNotifyingHost (prm->convertTo0to1 (pendingValue[i].load (std::memory_order_relaxed)));

    // PREV / NEXT MEMORY recus en MIDI : rappel de la memoire ici (thread de messages)
    if (const int step = pendingSlotStep.exchange (0); step != 0)
        loadSlot (((currentSlot + step) % kNumSlots + kNumSlots) % kNumSlots);
}

// ---------------------------------------------------------------------------
//  MIDI Learn : table des commandes apprenables (nom = serigraphie de la pedale)
//  Potards a partir de CC 20, selecteur / inverseurs ensuite, footswitchs a partir de CC 80.
// ---------------------------------------------------------------------------
const EightEchoProcessor::Learnable& EightEchoProcessor::learnable (int i)
{
    static const Learnable t[kNumLearn] = {
        { "elevel",     "E.LEVEL",     20, kContinuous },
        { "mix",        "MIX",         21, kContinuous },
        { "feedback",   "FEEDBACK",    22, kContinuous },
        { "time",       "TIME",        23, kContinuous },
        { "mode",       "MODE",        24, kSelector },
        { "long",       "X2",          25, kToggle },
        { "sync",       "SYNC",        26, kToggle },
        { "exp",        "EXP",         27, kContinuous },     // pedale d'expression (menu Expression pedal)
        { "footswitch", "BYPASS",      80, kFootswitch },     // tenu tant que CC >= 64 (comme le footswitch)
        { "tap",        "TAP",         81, kFootswitch },     // tenu : TWIST (en LOOP : 2 s = effacer)
        { "prev",       "PREV MEMORY", 82, kAction },         // bouton au-dessus de TAP
        { "next",       "NEXT MEMORY", 83, kAction },         // bouton au-dessus de BYPASS
    };
    return t[juce::jlimit (0, kNumLearn - 1, i)];
}

int EightEchoProcessor::learnIndexOf (const juce::String& id)
{
    for (int i = 0; i < kNumLearn; ++i)
        if (id == learnable (i).id) return i;
    return -1;
}

void EightEchoProcessor::resetMidiDefaults()
{
    for (int i = 0; i < kNumLearn; ++i) ccNum[i].store (learnable (i).defaultCC);
}

// Thread audio : aucune allocation, aucun appel a l'hote. Les valeurs passent par la file (timer 60 Hz).
void EightEchoProcessor::handleMidi (juce::MidiBuffer& midi)
{
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (! msg.isController()) continue;
        const int cc = msg.getControllerNumber(), v = msg.getControllerValue();

        // learn : le premier CC recu est affecte a la commande visee (et n'agit pas sur le son)
        const int lt = learnTarget.load();
        if (lt >= 0 && lt < kNumLearn)
        {
            if (cc >= 120) continue;                       // messages de mode de canal : ignores
            for (int i = 0; i < kNumLearn; ++i)
                if (i != lt && ccNum[i].load() == cc) ccNum[i].store (-1);   // deja pris : l'autre commande est liberee
            ccNum[lt].store (cc);
            ccPrevValue[lt] = v;
            lastLearned.store (lt);
            learnTarget.store (-1);
            continue;
        }

        for (int i = 0; i < kNumLearn; ++i)
        {
            if (ccNum[i].load (std::memory_order_relaxed) != cc) continue;
            const auto& d = learnable (i);
            const bool high = v >= 64;
            switch (d.type)
            {
                case kContinuous: setParamFromAudio (d.id, (float) v / 127.0f); break;          // plages 0..1
                case kToggle:     setParamFromAudio (d.id, high ? 1.0f : 0.0f); break;
                case kSelector:   setParamFromAudio (d.id, (float) juce::jmin (kNumModes - 1, v * kNumModes / 128)); break;
                case kFootswitch:
                    if (std::strcmp (d.id, "footswitch") == 0) { ccFsDown = high; ccFsHit = ccFsHit || high; }
                    else
                    {
                        if (high && ! ccTapDown) ccTapPressSample = sampleClock + meta.samplePosition;
                        ccTapDown = high; ccTapHit = ccTapHit || high;
                    }
                    break;
                case kAction:
                    if (high && ccPrevValue[i] < 64) pendingSlotStep.fetch_add (std::strcmp (d.id, "prev") == 0 ? -1 : 1);
                    break;
                default: break;
            }
            ccPrevValue[i] = v;
        }
    }
}

// ---------------------------------------------------------------------------
//  Memoires 1 a 7 : chaque LED du visuel = un emplacement
// ---------------------------------------------------------------------------
const juce::StringArray& EightEchoProcessor::slotParamIds()
{
    static const juce::StringArray ids { "elevel", "mix", "feedback", "time", "mode", "stereo", "division", "long", "carryover", "sync" };
    return ids;
}

void EightEchoProcessor::initSlots()
{
    // 7 memoires de depart, prises dans les presets d'usine
    const char* names[kNumSlots] = { "Bread & Butter", "BBD Crunch Lead", "Floating Tape Solo", "Echo Hall", "Heaven Swell", "Lush Chorus Echo", "Backwards Solo" };
    for (int s = 0; s < kNumSlots; ++s)
    {
        juce::ValueTree t ("SLOT"); t.setProperty ("index", s, nullptr);
        for (int k = 0; k < kNumPresets; ++k)
            if (juce::String (kPresets[k].name) == names[s])
            {
                const auto& pr = kPresets[k];
                t.setProperty ("elevel", pr.level, nullptr); t.setProperty ("mix", 0.5f, nullptr);
                t.setProperty ("feedback", pr.feedback, nullptr); t.setProperty ("time", pr.time, nullptr);
                t.setProperty ("mode", pr.mode, nullptr); t.setProperty ("stereo", pr.stereo, nullptr);
                t.setProperty ("division", pr.division, nullptr); t.setProperty ("long", pr.longMode ? 1 : 0, nullptr);
                t.setProperty ("carryover", 1, nullptr); t.setProperty ("sync", 0, nullptr);
            }
        slots[(size_t) s] = t;
    }
}

void EightEchoProcessor::loadSlot (int i)
{
    if (! juce::isPositiveAndBelow (i, kNumSlots)) return;
    currentSlot = i;
    const auto& t = slots[(size_t) i];
    for (auto& id : slotParamIds())
        if (t.hasProperty (id))
            if (auto* par = apvts.getParameter (id))
                par->setValueNotifyingHost (par->convertTo0to1 ((float) (double) t.getProperty (id)));
    tapOverride = false;
}

void EightEchoProcessor::saveSlot (int i)
{
    if (! juce::isPositiveAndBelow (i, kNumSlots)) return;
    juce::ValueTree t ("SLOT"); t.setProperty ("index", i, nullptr);
    for (auto& id : slotParamIds())
        if (auto* par = apvts.getParameter (id))
            t.setProperty (id, par->convertFrom0to1 (par->getValue()), nullptr);
    slots[(size_t) i] = t;
    currentSlot = i;
}

bool EightEchoProcessor::slotMatchesCurrent (int i) const
{
    if (! juce::isPositiveAndBelow (i, kNumSlots)) return false;
    const auto& t = slots[(size_t) i];
    for (auto& id : slotParamIds())
        if (auto* par = apvts.getParameter (id))
            if (std::abs (par->convertFrom0to1 (par->getValue()) - (float) (double) t.getProperty (id, -999.0)) > 0.004f)
                return false;
    return true;
}

bool EightEchoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void EightEchoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    monoTmp.assign ((size_t) juce::jmax (samplesPerBlock, 4096), 0.0f);
    engine.prepare (sampleRate);
}

void EightEchoProcessor::registerTap()
{
    const int64_t now = sampleClock;
    if (! taps.empty() && (double) (now - taps.back()) / sr > 3.0) { taps.clear(); tapSeq = 0; }
    taps.push_back (now);
    // la LED de tempo repart exactement sur chaque tap (le tap = un temps), le 1er tap = 1er temps
    ledResync = true; ledBeat = tapSeq % 4; ++tapSeq;
    if (taps.size() > 4) taps.erase (taps.begin());
    if (taps.size() >= 2)
    {
        double sum = 0; for (size_t i = 1; i < taps.size(); ++i) sum += (double) (taps[i] - taps[i - 1]);
        double q = 1000.0 * sum / (double) (taps.size() - 1) / sr;
        tapQuarterMs = (float) juce::jlimit (200.0, 2500.0, q);   // BPM 24..300
        tapOverride = true;
        knobAtTap = rTime->load (std::memory_order_relaxed);
    }
}

void EightEchoProcessor::handleFootswitch (bool pressed, int mode, int blockLen)
{
    auto& looper = engine.getLooper();
    const bool on = queuedOrCurrent ("on", rOn) > 0.5f;   // bascule : valeur en file si elle existe

    if (pressed && ! fsPrev)
    {
        fsPressSample = sampleClock; fsConsumed = false;
        if (mode == LOOP)
        {
            const bool running = looper.state == Looper::PLAYING || looper.state == Looper::OVERDUB;
            if (running && (double) (sampleClock - lastLoopReleaseSample) / sr < 1.0)
            {
                // double appui (< 1 s) : arret de la lecture
                if (looper.state == Looper::OVERDUB) looper.state = Looper::PLAYING;
                looper.stop();
                lastLoopReleaseSample = -1000000000;
            }
            else { looper.press(); lastLoopReleaseSample = sampleClock; }
        }
        else if (tapModeActive.load()) { registerTap(); fsConsumed = true; }
    }
    else if (pressed && fsPrev)
    {
        const double held = (double) (sampleClock + blockLen - fsPressSample) / sr;
        if (mode == LOOP)
        {
            if (held >= 2.0 && ! fsConsumed) { looper.clearAll(); fsConsumed = true; }
        }
        else if ((mode == WARP || mode == GLT) && on && ! tapModeActive.load())
        {
            if (held >= 0.25) { fsHoldFx = true; fsConsumed = true; }
        }
        else if (held >= 2.0 && ! fsConsumed)
        {
            tapModeActive = ! tapModeActive.load();   // appui 2 s : entree / sortie du mode TEMPO
            fsConsumed = true;
        }
    }
    else if (! pressed && fsPrev)
    {
        fsHoldFx = false;
        if (mode != LOOP && ! fsConsumed && pOn != nullptr)
            setParamFromAudio ("on", on ? 0.0f : 1.0f);      // transmis a l'hote par le timer
    }
    fsPrev = pressed;
}

void EightEchoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    if (n == 0) return;

    // mono -> stereo
    if (numIn == 1 && buffer.getNumChannels() > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);

    handleMidi (midi);

    const int mode = (int) rMode->load (std::memory_order_relaxed);
    // footswitchs : parametre (souris / DAW) OU CC appris tenu (un appui bref dans le bloc compte aussi)
    const bool fs = rFootswitch->load (std::memory_order_relaxed) > 0.5f || ccFsDown || ccFsHit;
    const bool tapP = rTap->load (std::memory_order_relaxed) > 0.5f || ccTapDown || ccTapHit;
    // TAP tenu par CC : TWIST apres 0,45 s (en LOOP : effacer apres 2 s), comme a la souris
    const double ccTapHeld = ccTapDown ? (double) (sampleClock - ccTapPressSample) / sr : 0.0;
    const bool ccTwist = ccTapDown && ccTapHeld > (mode == LOOP ? 2.0 : 0.45);
    const bool twistP = rTwist->load (std::memory_order_relaxed) > 0.5f || ccTwist;
    ccFsHit = false; ccTapHit = false;

    handleFootswitch (fs, mode, n);
    if (mode == LOOP) tapModeActive = false;

    // footswitch externe : TAP (B) / TWIST (A) - en LOOP : STOP / CLEAR
    if (tapP && ! tapPrev) { if (mode == LOOP) engine.getLooper().stop(); else registerTap(); }
    if (twistP && ! twistPrevForLoop && mode == LOOP) engine.getLooper().clearAll();
    tapPrev = tapP; twistPrevForLoop = twistP;

    // ---- expression ----
    const float exp = rExp->load (std::memory_order_relaxed);
    float level = rLevel->load (std::memory_order_relaxed), fb = rFeedback->load (std::memory_order_relaxed), time = rTime->load (std::memory_order_relaxed);
    if (rExpLevel->load (std::memory_order_relaxed) > 0.5f)    level *= exp;
    if (rExpFeedback->load (std::memory_order_relaxed) > 0.5f) fb *= exp;
    if (rExpTime->load (std::memory_order_relaxed) > 0.5f)     time *= exp;

    // ---- temps de retard ----
    if (tapOverride && std::abs (rTime->load (std::memory_order_relaxed) - knobAtTap) > 0.012f) tapOverride = false;   // tourner TIME annule le tap
    const bool longMode = rLong->load (std::memory_order_relaxed) > 0.5f;
    float lo, hi; modeRangeMs (mode, lo, hi);
    float ms = lo + (hi - lo) * time * time;
    float quarter = -1.0f;
    juce::Optional<juce::AudioPlayHead::PositionInfo> pos;
    if (auto* ph = getPlayHead()) pos = ph->getPosition();
    double bpm = 0.0;
    if (pos.hasValue()) if (auto b = pos->getBpm()) bpm = *b;
    const bool syncOn = rSync->load (std::memory_order_relaxed) > 0.5f;            // bouton SYNC : delay cale sur le tempo du DAW
    const bool locked = syncOn && bpm > 1.0;
    if (locked) quarter = (float) (60000.0 / bpm);
    hostBpm = (float) bpm; hostLocked = locked;
    if (quarter < 0 && tapOverride) quarter = tapQuarterMs;
    if (quarter > 0)
    {
        const int div = juce::jlimit (0, 6, (int) rDivision->load (std::memory_order_relaxed));
        float t = juce::jlimit (67.0f, 10000.0f, quarter * kDivMult[div]);
        if (mode == GLT) t *= 0.25f;
        ms = longMode ? t * 0.5f : t;     // la valeur tapee est le temps reel, Long compris
    }

    EngineParams ep;
    ep.mode = mode; ep.level = level; ep.feedback = fb; ep.timeKnob = time; ep.delayMs = ms;
    ep.longMode = longMode; ep.dryKill = rDryKill->load (std::memory_order_relaxed) > 0.5f; ep.mix = rMix->load (std::memory_order_relaxed);
    if (mode == LOOP) loopLevel = level;            // E.LEVEL en LOOP = volume de boucle, memorise
    ep.loopLevel = loopLevel;
    ep.stereo = (int) rStereo->load (std::memory_order_relaxed);
    ep.effectOn = rOn->load (std::memory_order_relaxed) > 0.5f; ep.carryover = rCarryover->load (std::memory_order_relaxed) > 0.5f;
    ep.holdFx = fsHoldFx; ep.twist = twistP && mode != LOOP;
    engine.lastStereoWide = ep.stereo == STEREO_WIDE;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0);
    if (buffer.getNumChannels() > 1) engine.process (L, R, L, R, n, ep);
    else
    {
        if (monoTmp.size() < (size_t) n) monoTmp.resize ((size_t) n);   // rare : bloc plus grand qu'annonce
        float* tmp = monoTmp.data(); std::copy (L, L + n, tmp);
        engine.process (L, tmp, L, tmp, n, ep);
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + tmp[i]);
    }
    for (int c = numIn; c < numOut && c >= 2; ++c) buffer.clear (c, 0, n);

    sampleClock += n;

    // ---- etat pour l'interface ----
    effectiveDelayMs = ms * (longMode ? 2.0f : 1.0f);
    twistAmount = engine.getTwistAmount(); warpAmount = engine.getWarpAmount(); glitching = engine.isGlitching();
    // LED de tempo : bat les temps.
    //  - SYNC + DAW en lecture : calee sur la position du DAW (temps de la mesure, 1er temps accentue)
    //  - tempo tape : repart sur chaque tap (chaque tap = un temps), 1er tap = 1er temps
    //  - sinon : clignote au temps de retard
    const double qms = quarter > 0 ? (double) quarter : (double) juce::jmax (60.0f, ms * (longMode ? 2.0f : 1.0f));
    bool fromHost = false;
    if (locked && pos.hasValue() && pos->getIsPlaying())
        if (auto ppq = pos->getPpqPosition())
        {
            int num = 4, den = 4;
            if (auto ts = pos->getTimeSignature()) { num = juce::jmax (1, ts->numerator); den = juce::jmax (1, ts->denominator); }
            const double beatQ = 4.0 / (double) den;                         // longueur d'un temps en noires
            const double bar = pos->getPpqPositionOfLastBarStart().orFallback (0.0);
            const double endPpq = *ppq + (double) n / sr * bpm / 60.0;          // fin du bloc = ce que l'on voit
            double inBar = (endPpq - bar) / beatQ;
            if (inBar < 0) inBar = endPpq / beatQ;
            const double beats = std::floor (inBar);
            ledPhase = inBar - beats;
            ledBeat = (int) std::fmod (beats, (double) num); if (ledBeat < 0) ledBeat += num;
            fromHost = true;
        }
    if (! fromHost)
    {
        if (ledResync) { ledPhase = 0.0; ledResync = false; }
        ledPhase += (double) n / sr * 1000.0 / qms;
        while (ledPhase >= 1.0) { ledPhase -= 1.0; ledBeat = (ledBeat + 1) % 4; }
    }
    tapBlinkPhase = (float) ledPhase;
    tapBeat = ledBeat;
    tapAccent = fromHost || (tapOverride && ! locked) || locked;
    auto& lp = engine.getLooper();
    looperState = (int) lp.state;
    loopSeconds = (float) (lp.state == Looper::RECORDING ? lp.pos : lp.len) / (float) sr;
    if (mode == LOOP)
    {
        switch (lp.state)
        {
            case Looper::EMPTY:     ledState = LED_OFF; break;
            case Looper::RECORDING: ledState = LED_RED_BLINK; break;
            case Looper::PLAYING:   ledState = LED_GREEN; break;
            case Looper::OVERDUB:   ledState = LED_ORANGE; break;
            case Looper::STOPPED:   ledState = lp.len > 0 ? LED_GREEN_BLINK : LED_OFF; break;
        }
    }
    else if (tapModeActive.load()) ledState = ep.effectOn ? LED_REDGREEN_BLINK : LED_RED_BLINK;
    else ledState = ep.effectOn ? LED_RED : LED_OFF;
}

// ----------------------------------------------------------------------------
int EightEchoProcessor::getNumPrograms() { return kNumPresets; }

const juce::String EightEchoProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, kNumPresets) ? juce::String (kPresets[index].name) : juce::String();
}

void EightEchoProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, kNumPresets)) return;
    currentProgram = index;
    const auto& pr = kPresets[index];
    auto set = [this] (const char* id, float v) { if (auto* par = apvts.getParameter (id)) par->setValueNotifyingHost (par->convertTo0to1 (v)); };
    set ("mode", (float) pr.mode); set ("elevel", pr.level); set ("feedback", pr.feedback); set ("time", pr.time); set ("stereo", (float) pr.stereo);
    set ("long", pr.longMode ? 1.0f : 0.0f); set ("division", (float) pr.division); set ("drykill", 0.0f); set ("mix", 0.5f);
    tapOverride = false;
}

void EightEchoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    state.setProperty ("tapMs", tapQuarterMs, nullptr);
    state.setProperty ("tapOn", tapOverride, nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("slot", currentSlot, nullptr);
    for (int i = 0; i < kNumLearn; ++i)
        state.setProperty (juce::String ("midiCC_") + learnable (i).id, ccNum[i].load(), nullptr);
    state.removeChild (state.getChildWithName ("SLOTS"), nullptr);
    juce::ValueTree sl ("SLOTS");
    for (auto& t : slots) sl.appendChild (t.createCopy(), nullptr);
    state.appendChild (sl, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void EightEchoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            uiScale = (float) (double) vt.getProperty ("uiScale", 1.0);
            tapQuarterMs = (float) (double) vt.getProperty ("tapMs", 0.0);
            tapOverride = (bool) vt.getProperty ("tapOn", false);
            currentProgram = (int) vt.getProperty ("program", 0);
            currentSlot = juce::jlimit (0, kNumSlots - 1, (int) vt.getProperty ("slot", 0));
            // assignations MIDI (ancien projet sans ces proprietes : numeros par defaut)
            for (int i = 0; i < kNumLearn; ++i)
                ccNum[i].store ((int) vt.getProperty (juce::String ("midiCC_") + learnable (i).id, learnable (i).defaultCC));
            auto sl = vt.getChildWithName ("SLOTS");
            if (sl.isValid())
            {
                for (int k = 0; k < sl.getNumChildren() && k < kNumSlots; ++k) slots[(size_t) k] = sl.getChild (k).createCopy();
                vt.removeChild (sl, nullptr);
            }
            knobAtTap = (float) (double) vt.getChildWithProperty ("id", "time").getProperty ("value", 0.5);
            apvts.replaceState (vt);
            // les commandes momentanees ne doivent jamais rester enfoncees apres un rechargement
            for (auto* id : { "footswitch", "tap", "twist" })
                if (auto* par = apvts.getParameter (id)) par->setValueNotifyingHost (0.0f);
        }
}

juce::AudioProcessorEditor* EightEchoProcessor::createEditor() { return new EightEchoEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EightEchoProcessor(); }
