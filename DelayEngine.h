// =============================================================================
//  Eight Echo (Studio Wiga) - moteur DSP
//  Emulation comportementale d'un delay compact 11 modes (type DD-8).
//  Tout le traitement est dans ce fichier : lignes de retard, couleurs par mode,
//  reverse, shimmer, reverb (+RV), glitch (GLT), warp, twist, looper.
//  Le signal sec ne passe JAMAIS par ici (chemin direct sans latence).
// =============================================================================
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <array>

namespace wiga
{
constexpr float kPi = 3.14159265358979f;

// ----------------------------------------------------------------------------
enum Mode { LOOP = 0, ANALOG, STANDARD, TAPE, WARM, REVERSE, PLUSRV, SHIM, MOD, WARP, GLT, kNumModes };
enum StereoMode { STEREO_INDEPENDENT = 0, STEREO_WIDE, STEREO_PANNING };

inline const char* modeName (int m)
{
    static const char* n[] = { "LOOP", "ANALOG", "STANDARD", "TAPE", "WARM", "REVERSE", "+RV", "SHIM", "MOD", "WARP", "GLT" };
    return n[std::clamp (m, 0, kNumModes - 1)];
}

// Plages officielles du TIME (ms), sortie normale. Long = x2.
inline void modeRangeMs (int m, float& lo, float& hi)
{
    switch (m)
    {
        case REVERSE: lo = 300.0f; hi = 5000.0f; break;
        case SHIM:    lo = 200.0f; hi = 800.0f;  break;
        case GLT:     lo = 10.0f;  hi = 400.0f;  break;
        default:      lo = 20.0f;  hi = 800.0f;  break;
    }
}

// ----------------------------------------------------------------------------
struct OnePoleLP
{
    float a = 0.0f, z = 0.0f;
    void set (float fc, float fs) { a = std::exp (-2.0f * kPi * fc / fs); }
    float process (float x) { z = x + a * (z - x); return z; }
    void reset() { z = 0.0f; }
};

struct OnePoleHP
{
    float a = 0.0f, x1 = 0.0f, y1 = 0.0f;
    void set (float fc, float fs) { a = std::exp (-2.0f * kPi * fc / fs); }
    float process (float x) { float y = a * (y1 + x - x1); x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0.0f; }
};

// Biquad RBJ (LP / HP / peak)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void lowpass (float fc, float q, float fs)
    {
        fc = std::min (fc, 0.45f * fs);
        float w = 2 * kPi * fc / fs, c = std::cos (w), s = std::sin (w), al = s / (2 * q), a0 = 1 + al;
        b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    void highpass (float fc, float q, float fs)
    {
        float w = 2 * kPi * fc / fs, c = std::cos (w), s = std::sin (w), al = s / (2 * q), a0 = 1 + al;
        b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    void peak (float fc, float q, float gainDb, float fs)
    {
        float A = std::pow (10.0f, gainDb / 40.0f), w = 2 * kPi * fc / fs, c = std::cos (w), s = std::sin (w), al = s / (2 * q), a0 = 1 + al / A;
        b0 = (1 + al * A) / a0; b1 = -2 * c / a0; b2 = (1 - al * A) / a0; a1 = -2 * c / a0; a2 = (1 - al / A) / a0;
    }
    float process (float x) { float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
};

// Limiteur doux : transparent sous le genou, arrondi au-dessus (empeche l'emballement
// de l'auto-oscillation tout en la laissant exister, comme sur la pedale).
inline float softClip (float x, float knee = 0.8f)
{
    float ax = std::abs (x);
    if (ax <= knee) return x;
    float over = (ax - knee) / (1.0f - knee + 1e-6f);
    float y = knee + (1.0f - knee) * std::tanh (over);
    return x < 0 ? -y : y;
}

inline float tapeSat (float x, float drive)
{
    // saturation asymetrique legere facon bande
    float y = std::tanh (drive * (x + 0.08f * x * x)) / std::tanh (drive);
    return y;
}

struct Rng
{
    uint32_t s = 0x1234567u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xFFFFFF) / 8388608.0f - 1.0f; }
};

// Bruit lisse (derive aleatoire basse frequence)
struct SmoothNoise
{
    Rng rng; float cur = 0, target = 0, phase = 0, inc = 0;
    void set (float rateHz, float fs) { inc = rateHz / fs; }
    float process()
    {
        phase += inc;
        if (phase >= 1.0f) { phase -= 1.0f; cur = target; target = rng.next(); }
        float t = phase * phase * (3 - 2 * phase);
        return cur + (target - cur) * t;
    }
};

// ----------------------------------------------------------------------------
// Ligne de retard circulaire avec lecture Hermite
struct DelayLine
{
    std::vector<float> buf; int size = 0, w = 0;
    void init (int n) { size = n; buf.assign ((size_t) n, 0.0f); w = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; if (++w >= size) w = 0; }
    float at (int idx) const { idx %= size; if (idx < 0) idx += size; return buf[(size_t) idx]; }
    // delay en echantillons (>= 1), relatif a l'echantillon le plus recent ecrit
    float read (float d) const
    {
        d = std::clamp (d, 1.0f, (float) (size - 4));
        float rp = (float) w - 1.0f - d;
        int i = (int) std::floor (rp); float f = rp - (float) i;
        float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        float c0 = x0, c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }
};

// ----------------------------------------------------------------------------
// Pitch shifter granulaire (2 tetes, fenetres triangulaires) pour le SHIM (+1 octave)
struct PitchShifter
{
    DelayLine line; float phase = 0, grain = 2048, ratio = 2.0f;
    void init (float fs) { grain = 0.045f * fs; line.init ((int) (grain * 2 + 16)); phase = 0; }
    void clear() { line.clear(); phase = 0; }
    float process (float x)
    {
        line.push (x);
        float rate = (1.0f - ratio) / grain;         // negatif pour monter
        phase += rate; if (phase < 0) phase += 1.0f; if (phase >= 1) phase -= 1.0f;
        float p2 = phase + 0.5f; if (p2 >= 1) p2 -= 1.0f;
        float d1 = 1.0f + phase * grain, d2 = 1.0f + p2 * grain;
        float w1 = 1.0f - std::abs (2.0f * phase - 1.0f), w2 = 1.0f - std::abs (2.0f * p2 - 1.0f);
        return line.read (d1) * w1 + line.read (d2) * w2;
    }
};

// ----------------------------------------------------------------------------
// Petite reverb FDN 8 lignes (mode +RV). Le TIME de la pedale regle aussi la duree.
struct FDNReverb
{
    static constexpr int N = 8;
    std::array<DelayLine, N> d; std::array<float, N> len {}, g {}; std::array<OnePoleLP, N> damp;
    std::array<DelayLine, 4> ap; std::array<float, 4> apLen {};
    float fs = 48000, rt = 2.0f;
    void init (float sr)
    {
        fs = sr;
        const float ms[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.1f, 73.3f };
        for (int i = 0; i < N; ++i) { len[(size_t) i] = ms[i] * 0.001f * fs; d[(size_t) i].init ((int) (len[(size_t) i] + 8)); damp[(size_t) i].set (6000.0f, fs); }
        const float apms[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
        for (int i = 0; i < 4; ++i) { apLen[(size_t) i] = apms[i] * 0.001f * fs; ap[(size_t) i].init ((int) (apLen[(size_t) i] + 8)); }
        setRT (2.0f);
    }
    void setRT (float seconds)
    {
        rt = std::max (0.2f, seconds);
        for (int i = 0; i < N; ++i) g[(size_t) i] = std::pow (10.0f, -3.0f * len[(size_t) i] / (rt * fs));
    }
    void clear() { for (auto& x : d) x.clear(); for (auto& x : ap) x.clear(); for (auto& f : damp) f.reset(); }
    void process (float in, float& outL, float& outR)
    {
        // diffusion d'entree
        float x = in;
        for (int i = 0; i < 4; ++i)
        {
            float del = ap[(size_t) i].read (apLen[(size_t) i]);
            float v = x + 0.6f * del; ap[(size_t) i].push (v); x = del - 0.6f * v;
        }
        std::array<float, N> o {};
        float sum = 0;
        for (int i = 0; i < N; ++i) { o[(size_t) i] = damp[(size_t) i].process (d[(size_t) i].read (len[(size_t) i]) * g[(size_t) i]); sum += o[(size_t) i]; }
        float hh = sum * (2.0f / N);
        for (int i = 0; i < N; ++i) d[(size_t) i].push (o[(size_t) i] - hh + x * 0.5f);
        outL = (o[0] + o[2] + o[4] + o[6]) * 0.5f;
        outR = (o[1] + o[3] + o[5] + o[7]) * 0.5f;
    }
};

// ----------------------------------------------------------------------------
// Looper (mode LOOP) : 40 s stereo, overdub illimite
struct Looper
{
    enum State { EMPTY, RECORDING, PLAYING, OVERDUB, STOPPED };
    std::vector<float> L, R; int maxLen = 0, len = 0, pos = 0; State state = EMPTY;
    float fade = 0.0f;
    void init (float fs, float seconds) { maxLen = (int) (fs * seconds); L.assign ((size_t) maxLen, 0); R.assign ((size_t) maxLen, 0); clearAll(); }
    void clearAll() { len = pos = 0; state = EMPTY; }
    // footswitch : rec -> play -> overdub -> play ...
    void press()
    {
        switch (state)
        {
            case EMPTY:     len = 0; pos = 0; state = RECORDING; break;
            case RECORDING: len = std::max (pos, 1); pos = 0; state = PLAYING; break;
            case PLAYING:   state = OVERDUB; break;
            case OVERDUB:   state = PLAYING; break;
            case STOPPED:   pos = 0; state = PLAYING; break;
        }
    }
    void stop() { if (state == RECORDING) { len = std::max (pos, 1); } if (len > 0) { state = STOPPED; pos = 0; } else state = EMPTY; }
    // lecture seule (un autre mode d'effet est selectionne) : la boucle continue de tourner
    // sans etre touchee par l'effet ni par ce que l'on joue par-dessus
    void playOnly (float& outL, float& outR)
    {
        outL = outR = 0;
        if (state == RECORDING) { len = std::max (pos, 1); pos = 0; state = PLAYING; }
        if (state == OVERDUB) state = PLAYING;
        if (state != PLAYING || len <= 0) return;
        outL = L[(size_t) pos]; outR = R[(size_t) pos];
        if (++pos >= len) pos = 0;
    }
    bool isRunning() const { return state == PLAYING || state == OVERDUB || state == RECORDING; }

    void process (float inL, float inR, float& outL, float& outR)
    {
        outL = outR = 0;
        switch (state)
        {
            case RECORDING:
                L[(size_t) pos] = inL; R[(size_t) pos] = inR;
                if (++pos >= maxLen) { len = maxLen; pos = 0; state = PLAYING; }
                break;
            case PLAYING:
                outL = L[(size_t) pos]; outR = R[(size_t) pos];
                if (++pos >= len) pos = 0;
                break;
            case OVERDUB:
                outL = L[(size_t) pos]; outR = R[(size_t) pos];
                L[(size_t) pos] = softClip (outL + inL, 0.9f); R[(size_t) pos] = softClip (outR + inR, 0.9f);
                if (++pos >= len) pos = 0;
                break;
            default: break;
        }
    }
};

// ----------------------------------------------------------------------------
// Parametres temps reel transmis au moteur (deja "cuits" par le processeur)
struct EngineParams
{
    int   mode = STANDARD;
    float level = 0.5f;          // knob E.LEVEL 0..1
    float feedback = 0.4f;       // knob 0..1
    float timeKnob = 0.5f;       // knob 0..1 (sert aussi a la duree de reverb +RV)
    float delayMs = 300.0f;      // temps effectif (knob ou tap), avant Long
    bool  longMode = false;
    bool  dryKill = false;
    float mix = 0.5f;            // MIX : a midi = son direct + effet pleins (comme la DD-8)
    float loopLevel = 0.5f;      // volume de la boucle memorise (E.LEVEL regle en mode LOOP)
    int   stereo = STEREO_INDEPENDENT;
    bool  effectOn = true;
    bool  carryover = true;
    bool  holdFx = false;        // pied maintenu en WARP / GLT
    bool  twist = false;
};

// ----------------------------------------------------------------------------
class DelayEngine
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        const int maxSamples = (int) (fs * 20.6f) + 64;   // 10 s x2 (lecture reverse) + marge
        for (int c = 0; c < 2; ++c)
        {
            line[c].init (maxSamples);
            shifter[c].init (fs);
            glitchBuf[c].assign ((size_t) (fs * 0.85f), 0.0f);
            for (auto& a : diff[c]) a.init ((int) (0.012f * fs));
        }
        reverb.init (fs);
        looper.init (fs, 40.0f);
        tapeWow.set (0.55f, fs); tapeDrift.set (0.18f, fs);
        for (int c = 0; c < 2; ++c) { noiseLp[c].set (5000.0f, fs); }
        curDelay = targetDelay = 0.3f * fs;
        tapA = tapB = curDelay; xf = 1.0f;
        lastMode = -1;
        resetAll();
    }

    void resetAll()
    {
        for (int c = 0; c < 2; ++c)
        {
            line[c].clear(); shifter[c].clear();
            for (auto& b : loopF[c]) b.reset();
            for (auto& b : wetF[c]) b.reset();
            hpLoop[c].reset(); headBump[c].reset();
            for (auto& a : diff[c]) a.clear();
        }
        reverb.clear();
        revPhase = 0;
        glitchActive = false; glitchGain = 0;
    }

    Looper& getLooper() { return looper; }

    // traitement d'un bloc ; in/out peuvent etre les memes tableaux
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const EngineParams& p)
    {
        if (p.mode != lastMode) onModeChange (p.mode);

        for (int i = 0; i < n; ++i)
        {
            const float dryL = inL[i], dryR = inR[i];
            float wetL = 0, wetR = 0;

            // ---------------- LOOP ----------------
            if (p.mode == LOOP)
            {
                float lL, lR; looper.process (dryL, dryR, lL, lR);
                const float lv = levelGain (p.level);
                const float dg = (p.dryKill ? 0.0f : 1.0f) * std::min (1.0f, 2.0f * (1.0f - p.mix));
                const float wm = std::min (1.0f, 2.0f * p.mix);
                outL[i] = dryL * dg + lL * lv * wm; outR[i] = dryR * dg + lR * lv * wm;
                continue;
            }

            // ------------- gates on/off + carryover -------------
            const float inTarget  = p.effectOn ? 1.0f : 0.0f;
            const float outTarget = (p.effectOn || p.carryover) ? 1.0f : 0.0f;
            inGate  += (inTarget  - inGate)  * gateCoef;
            outGate += (outTarget - outGate) * gateCoef;
            if (! p.effectOn && ! p.carryover && outGate < 1e-4f && ! cleared) { resetAll(); cleared = true; }
            if (p.effectOn) cleared = false;
            modeFade = std::min (1.0f, modeFade + 1.0f / (0.03f * fs));

            // ------------- twist / warp enveloppes -------------
            updateTwist (p.twist);
            const float warpTarget = (p.mode == WARP && p.holdFx && p.effectOn) ? 1.0f : 0.0f;
            warpAmt += (warpTarget - warpAmt) * (warpTarget > warpAmt ? warpAtk : warpRel);

            // ------------- temps de retard -------------
            float ms = p.delayMs * (p.longMode ? 2.0f : 1.0f);
            float tgt = std::max (1.0f, ms * 0.001f * fs * twistMul);
            if (p.mode == REVERSE) tgt = std::max (tgt, 0.05f * fs);
            updateDelayTaps (tgt, p.mode);

            // ------------- feedback effectif -------------
            float fb = feedbackGain (p.feedback);
            if (p.mode == ANALOG || p.mode == WARM) fb *= 1.07f;   // compense la perte des filtres : l'oscillation reste possible
            if (p.mode == SHIM) fb *= 1.22f;
            if (p.mode == GLT) fb = 0.25f;                       // en GLT le knob = profondeur
            fb = fb + (0.97f - fb) * warpAmt;                    // WARP : feedback monte (nappe), sans emballement
            fb = fb + (1.10f - fb) * twistFb;                    // TWIST : auto-oscillation
            float lvl = levelGain (p.level) * (1.0f + 0.12f * warpAmt); // WARP : legere montee (~+1 dB)
            if (p.mode == REVERSE && p.level > 0.985f) lvl = 1.0f;       // manuel : effet seul, meme niveau que l'entree

            // ------------- entree du delay selon le mode stereo -------------
            const float gin = inGate;
            const float mono = 0.5f * (dryL + dryR) * gin;
            float xL = dryL * gin, xR = dryR * gin;

            // lectures
            float yL = readTap (0, p.mode), yR = readTap (1, p.mode);

            // coloration dans la boucle
            float fL = loopProcess (0, yL, p.mode);
            float fR = loopProcess (1, yR, p.mode);

            float wL, wR;
            switch (p.stereo)
            {
                case STEREO_PANNING:
                    wL = mono + fb * fR;  wR = fb * fL;  break;
                case STEREO_WIDE:
                    wL = mono + fb * (0.72f * fR + 0.28f * fL);
                    wR = 0.30f * mono + fb * (0.72f * fL + 0.28f * fR);
                    break;
                default:
                    wL = xL + fb * fL;  wR = xR + fb * fR;  break;
            }
            const float knee = (p.mode == ANALOG || p.mode == TAPE) ? 0.55f : 0.85f;
            line[0].push (softClip (wL, knee));
            line[1].push (softClip (wR, knee));

            // ------------- sortie humide par mode -------------
            wetL = wetOutput (0, yL, fL, p);
            wetR = wetOutput (1, yR, fR, p);

            if (p.mode == PLUSRV)
            {
                reverb.setRT (0.7f + 2.8f * p.timeKnob);
                float rL, rR; reverb.process (0.5f * (yL + yR), rL, rR);
                wetL = wetL * 0.85f + rL * 0.55f; wetR = wetR * 0.85f + rR * 0.55f;
            }

            if (p.mode == GLT) processGlitch (wetL, wetR, p);

            // ------------- WIDE : leger balancement -------------
            if (p.stereo == STEREO_WIDE)
            {
                float sL = 0.85f * wetL + 0.15f * wetR, sR = 0.85f * wetR + 0.15f * wetL;
                wetL = sL; wetR = sR;
            }

            // ------------- TWIST : sensation de rotation -------------
            if (twistRot > 1e-4f)
            {
                rotPhase += (0.6f + 5.0f * twistRamp) / fs; if (rotPhase >= 1) rotPhase -= 1;
                // panoramique a puissance constante qui tourne de plus en plus vite
                float th = 0.25f * kPi * (1.0f + 0.85f * std::sin (2 * kPi * rotPhase));
                float m = 0.5f * (wetL + wetR) * 1.41421356f;
                wetL = wetL * (1 - twistRot) + m * std::cos (th) * twistRot;
                wetR = wetR * (1 - twistRot) + m * std::sin (th) * twistRot;
            }

            float g = lvl * outGate * modeFade;
            {
                // limiteur de securite des effets maintenus (WARP / GLT) : niveau de reference pris avant l'appui,
                // le son humide ne peut pas depasser ~+2 dB de ce niveau (ni -12 dBFS si le delay etait silencieux)
                const float e = std::max (std::abs (wetL), std::abs (wetR)) * g;
                const bool holding = warpAmt > 0.02f || glitchActive || twistFb > 0.05f;
                if (! holding) { holdRef += (e - holdRef) * (e > holdRef ? 0.002f : 0.00002f); }
                env += (e - env) * (e > env ? 0.01f : 0.0002f);
                if (holding)
                {
                    const float ceilv = std::max (0.25f, holdRef * 1.26f);
                    const float want = env > ceilv ? ceilv / env : 1.0f;
                    holdGain += (want - holdGain) * (want < holdGain ? 0.02f : 0.0005f);
                }
                else holdGain += (1.0f - holdGain) * 0.0005f;
                g *= holdGain;
            }
            wetL = softClip (wetL * g, 0.85f); wetR = softClip (wetR * g, 0.85f);

            // REVERSE au max : effet seul a niveau unitaire
            float dryGain = (p.dryKill ? 0.0f : 1.0f) * std::min (1.0f, 2.0f * (1.0f - p.mix));
            const float wetMix = std::min (1.0f, 2.0f * p.mix);
            wetMixSm += (wetMix - wetMixSm) * gateCoef;
            wetL *= wetMixSm; wetR *= wetMixSm;
            if (p.mode == REVERSE && p.level > 0.985f) dryGain = 0.0f;
            dryGainSm += (dryGain - dryGainSm) * gateCoef;

            // boucle du looper ajoutee APRES l'effet : elle ne passe jamais dans le delay
            float lpL, lpR; looper.playOnly (lpL, lpR);
            const float llv = levelGain (p.loopLevel);
            outL[i] = dryL * dryGainSm + wetL + lpL * llv;
            outR[i] = dryR * dryGainSm + wetR + lpR * llv;
        }
    }

    // ------------------------------------------------------------------
    // Courbes des potards
    static float levelGain (float k)
    {
        // 3 h (~0.8 de la course) = meme niveau que le son direct
        float g = k / 0.8f; return g * g;
    }
    static float feedbackGain (float k) { return 1.06f * std::pow (k, 1.15f); }

    float getTwistAmount() const { return twistRamp; }
    float getWarpAmount() const { return warpAmt; }
    bool  isGlitching() const { return glitchActive; }

private:
    float fs = 48000.0f;
    DelayLine line[2];
    PitchShifter shifter[2];
    std::array<DelayLine, 2> diff[2];
    Biquad loopF[2][2], wetF[2][2];
    OnePoleHP hpLoop[2];
    Biquad headBump[2];
    OnePoleLP noiseLp[2];
    FDNReverb reverb;
    Looper looper;
    Rng rng;
    SmoothNoise tapeWow, tapeDrift;

    int lastMode = -1;
    float modeFade = 1.0f;
    float inGate = 1, outGate = 1, dryGainSm = 1, wetMixSm = 1, gateCoef = 0.002f;
    bool cleared = false;

    // temps
    float curDelay = 0, targetDelay = 0, tapA = 0, tapB = 0, xf = 1.0f;
    float lfoPhase = 0, flutPhase = 0, analogPhase = 0;

    // twist / warp
    bool twistHeld = false; float twistT = 0, twistMul = 1.0f, twistFb = 0, twistRot = 0, twistRamp = 0, rotPhase = 0;
    float warpAmt = 0, warpAtk = 0, warpRel = 0;
    float holdRef = 0.1f, env = 0, holdGain = 1.0f;

    // reverse
    float revPhase = 0, revLen = 0;

    // glitch
    std::vector<float> glitchBuf[2];
    bool glitchActive = false; float glitchGain = 0, gPos = 0, gRate = 1; int gLen = 1, gRepeat = 0, gRepeatsPerCapture = 1; bool gReverse = false;

    void onModeChange (int m)
    {
        lastMode = m;
        modeFade = 0.0f;
        resetAll();
        gateCoef = 1.0f - std::exp (-1.0f / (0.006f * fs));
        warpAtk = 1.0f - std::exp (-1.0f / (0.7f * fs));
        warpRel = 1.0f - std::exp (-1.0f / (0.9f * fs));
        for (int c = 0; c < 2; ++c)
        {
            hpLoop[c].set (m == ANALOG ? 90.0f : m == WARM ? 110.0f : 40.0f, fs);
            switch (m)
            {
                case ANALOG:   loopF[c][0].lowpass (3400.0f, 0.62f, fs); loopF[c][1].lowpass (4200.0f, 0.55f, fs); break;
                // TAPE : percussif et pas "use" (tests Delay Dude / Bonedo) -> perte d'aigus moderee
                case TAPE:     loopF[c][0].lowpass (6200.0f, 0.70f, fs); loopF[c][1].lowpass (10000.0f, 0.5f, fs); headBump[c].peak (110.0f, 0.9f, 1.5f, fs); break;
                // WARM : un STANDARD "moins brillant", rond et moderne
                case WARM:     loopF[c][0].lowpass (4300.0f, 0.60f, fs); loopF[c][1].lowpass (7000.0f, 0.5f, fs); break;
                case PLUSRV:   loopF[c][0].lowpass (8000.0f, 0.60f, fs); loopF[c][1].lowpass (14000.0f, 0.5f, fs); break;
                case SHIM:     loopF[c][0].lowpass (7000.0f, 0.60f, fs); loopF[c][1].highpass (180.0f, 0.6f, fs); break;
                case WARP:     loopF[c][0].lowpass (7500.0f, 0.60f, fs); loopF[c][1].lowpass (12000.0f, 0.5f, fs); break;
                case MOD:      loopF[c][0].lowpass (9500.0f, 0.60f, fs); loopF[c][1].lowpass (15000.0f, 0.5f, fs); break;
                default:       loopF[c][0].lowpass (11000.0f, 0.60f, fs); loopF[c][1].lowpass (16000.0f, 0.5f, fs); break;
            }
            // pre-filtre de sortie (anti-repliement facon BBD pour l'analog)
            // filtre de "couleur" applique des la 1re repetition (avant, WARM/MOD... sortaient brillantes au 1er echo)
            float tone = 13000.0f, hp = 25.0f;
            switch (m)
            {
                case ANALOG: tone = 5200.0f; hp = 60.0f; break;
                case TAPE:   tone = 7500.0f; hp = 45.0f; break;
                case WARM:   tone = 4800.0f; hp = 70.0f; break;
                case PLUSRV: tone = 9500.0f; break;
                case MOD:    tone = 10500.0f; break;
                case WARP:   tone = 8500.0f; break;
                case SHIM:   tone = 9000.0f; break;
                default: break;
            }
            wetF[c][0].lowpass (tone, 0.7f, fs); wetF[c][1].highpass (hp, 0.7f, fs);
            headBump[c].reset();
            if (m != TAPE) headBump[c].peak (110.0f, 0.9f, 0.0f, fs);
        }
        glitchActive = false;
    }

    void updateTwist (bool held)
    {
        const float dt = 1.0f / fs;
        if (held && ! twistHeld) { twistT = 0; }
        twistHeld = held;
        if (held)
        {
            twistT += dt;
            float m = std::pow (2.0f, -twistT / 1.1f);         // ~2,7 octaves de montee en ~4 s (mesure video officielle)
            twistMul = std::max (0.08f, m);
            twistFb += (1.0f - twistFb) * (dt / 0.35f);
            twistRot += (1.0f - twistRot) * (dt / 0.6f);
            twistRamp = std::min (1.0f, twistT / 4.0f);
        }
        else
        {
            twistMul += (1.0f - twistMul) * (dt / 0.6f);
            twistFb  += (0.0f - twistFb)  * (dt / 0.25f);
            twistRot += (0.0f - twistRot) * (dt / 1.2f);
            twistRamp += (0.0f - twistRamp) * (dt / 1.0f);
        }
    }

    bool glides (int mode) const { return mode == ANALOG || mode == TAPE || twistHeld || twistMul < 0.999f; }

    void updateDelayTaps (float tgt, int mode)
    {
        targetDelay = tgt;
        if (glides (mode))
        {
            // glissement continu (effet de pitch quand on tourne TIME), comme un BBD / une bande
            float tau = mode == TAPE ? 0.28f : mode == ANALOG ? 0.14f : 0.02f;
            if (twistHeld) tau = 0.004f;
            curDelay += (targetDelay - curDelay) * (1.0f / (tau * fs + 1.0f));
            tapA = tapB = curDelay; xf = 1.0f;
        }
        else
        {
            // numerique : changements par fondu enchaine entre deux tetes (pas de glissement)
            if (xf >= 1.0f)
            {
                if (std::abs (targetDelay - tapB) > 0.0015f * fs) { tapA = tapB; tapB = targetDelay; xf = 0.0f; }
            }
            else xf = std::min (1.0f, xf + 1.0f / (0.045f * fs));
            curDelay = tapB;
        }
    }

    float modOffset (int c, int mode)
    {
        // retourne un decalage de lecture en echantillons
        if (c == 0)
        {
            lfoPhase += (mode == WARP ? 0.35f : 1.5f) / fs; if (lfoPhase >= 1) lfoPhase -= 1;
            flutPhase += 4.8f / fs; if (flutPhase >= 1) flutPhase -= 1;
            analogPhase += 0.21f / fs; if (analogPhase >= 1) analogPhase -= 1;
        }
        switch (mode)
        {
            case MOD:
            {
                // LFO 1,5 Hz, ~1 ms (~+-15 cents), voies L/R en opposition de phase (mesure + test Leon Todd)
                float ph = lfoPhase + (c == 1 ? 0.5f : 0.0f);
                return (1.0f + std::sin (2 * kPi * ph)) * 0.0010f * fs;
            }
            case WARP:
            {
                float ph = lfoPhase + (c == 1 ? 0.25f : 0.0f);
                return (1.0f + std::sin (2 * kPi * ph)) * 0.0006f * fs;
            }
            case TAPE:
            {
                float wow = (c == 0 ? tapeWowL : tapeWowL);
                float flut = std::sin (2 * kPi * (flutPhase + (c ? 0.1f : 0.0f))) * 0.00005f * fs;
                return (1.2f + wow) * 0.0010f * fs + flut;
            }
            case ANALOG:
                return (1.0f + std::sin (2 * kPi * analogPhase)) * 0.00035f * fs;
            default:
                return 0.0f;
        }
    }
    float tapeWowL = 0;

    float readTap (int c, int mode)
    {
        if (c == 0 && mode == TAPE) tapeWowL = 0.7f * tapeWow.process() + 0.5f * tapeDrift.process();
        if (mode == REVERSE) return readReverse (c);
        float off = modOffset (c, mode);
        float wideOff = 0.0f;
        if (c == 1 && lastStereoWide) wideOff = 0.007f * fs;
        if (xf >= 1.0f) return line[c].read (tapB + off + wideOff);
        float a = line[c].read (tapA + off + wideOff), b = line[c].read (tapB + off + wideOff);
        float g = 0.5f - 0.5f * std::cos (kPi * xf);
        return a * (1 - g) + b * g;
    }
public:
    bool lastStereoWide = false;
private:

    // reverse : deux tetes qui relisent a l'envers le dernier segment (longueur = TIME)
    float readReverse (int c)
    {
        float S = std::max (64.0f, curDelay);
        if (c == 0)
        {
            revPhase += 1.0f / S; if (revPhase >= 1.0f) revPhase -= 1.0f;
        }
        float p1 = revPhase, p2 = revPhase + 0.5f; if (p2 >= 1) p2 -= 1;
        float d1 = 2.0f * p1 * S + 1.0f, d2 = 2.0f * p2 * S + 1.0f;
        float w1 = std::sin (kPi * p1), w2 = std::sin (kPi * p2);
        w1 *= w1; w2 *= w2;   // sin^2 : la somme vaut 1
        return line[c].read (d1) * w1 + line[c].read (d2) * w2;
    }

    float loopProcess (int c, float y, int mode)
    {
        float v = hpLoop[c].process (y);
        v = loopF[c][0].process (v);
        v = loopF[c][1].process (v);
        switch (mode)
        {
            case ANALOG:
            {
                v = std::tanh (1.25f * v) / 1.25f;
                // souffle "craquant" du BBD, module par le signal (compander)
                float env = std::min (1.0f, std::abs (v) * 6.0f);
                v += noiseLp[c].process (rng.next()) * (0.00025f + 0.0012f * env);
                break;
            }
            case TAPE:
                v = headBump[c].process (v);
                v = tapeSat (v, 1.25f);
                v += noiseLp[c].process (rng.next()) * 0.00008f;
                break;
            case SHIM:
            {
                float s = shifter[c].process (v);
                v = 0.72f * v + 0.70f * s;   // octave discrete (Bonedo) qui s'empile a chaque repetition
                // diffusion douce : evite le cote "cassant"
                for (int k = 0; k < 2; ++k)
                {
                    float len = (k == 0 ? 0.0071f : 0.0043f) * fs + (c ? 37.0f : 0.0f);
                    float del = diff[c][(size_t) k].read (len);
                    float u = v + 0.5f * del; diff[c][(size_t) k].push (u); v = del - 0.5f * u;
                }
                break;
            }
            default: break;
        }
        return v;
    }

    float wetOutput (int c, float y, float f, const EngineParams& p)
    {
        float w = (p.mode == SHIM) ? f : y;
        if (p.mode == ANALOG || p.mode == TAPE) w = 0.35f * y + 0.65f * f; // la 1re repetition est deja coloree
        w = wetF[c][0].process (w);
        w = wetF[c][1].process (w);
        return w;
    }

    // GLT : tranches repetees "mitraillette" tant que le pied est maintenu ; FEEDBACK = profondeur
    void processGlitch (float& wetL, float& wetR, const EngineParams& p)
    {
        const bool want = p.holdFx && p.effectOn;
        const float depth = p.feedback;
        if (want && ! glitchActive) { glitchActive = true; captureSlice (p); gRepeat = 0; }
        if (! want && glitchActive && glitchGain < 1e-3f) glitchActive = false;
        glitchGain += ((want ? 1.0f : 0.0f) - glitchGain) * (1.0f / (0.004f * fs));
        if (! glitchActive) return;

        // lecture de la tranche
        float rp = gReverse ? (float) gLen - 1.0f - gPos : gPos;
        int i0 = std::clamp ((int) rp, 0, gLen - 1), i1 = std::min (i0 + 1, gLen - 1); float fr = rp - (float) i0;
        float sL = glitchBuf[0][(size_t) i0] * (1 - fr) + glitchBuf[0][(size_t) i1] * fr;
        float sR = glitchBuf[1][(size_t) i0] * (1 - fr) + glitchBuf[1][(size_t) i1] * fr;
        // porte : plus de profondeur = repetitions plus hachees
        float duty = 0.92f - 0.55f * depth;
        float ph = gPos / (float) gLen;
        float edge = 0.004f * fs / (float) gLen;
        // a faible profondeur les trous ne sont qu'attenues (stutter doux), a fond : coupures franches
        const float floorLv = 0.55f * (1.0f - depth) * (1.0f - depth);
        float gate = 1.0f;
        if (ph > duty) gate = 0.0f;
        else if (ph < edge) gate = ph / edge;
        else if (ph > duty - edge) gate = (duty - ph) / edge;
        gate = floorLv + (1.0f - floorLv) * gate;
        sL *= gate; sR *= gate;

        gPos += gRate;
        if (gPos >= (float) gLen)
        {
            gPos = 0; ++gRepeat;
            // la meme sequence courte boucle tant que le pied est maintenu (Bonedo) ;
            // sa longueur suit TIME / le tempo
        }
        const float g = glitchGain;
        const float lv = 0.85f;   // tranche glitch au niveau du delay, pas plus fort
        wetL = wetL * (1 - g) + sL * g * lv;
        wetR = wetR * (1 - g) + sR * g * lv;
    }

    void captureSlice (const EngineParams& p)
    {
        float ms = std::clamp (p.delayMs * (p.longMode ? 2.0f : 1.0f), 10.0f, 800.0f);
        gLen = std::clamp ((int) (ms * 0.001f * fs), 16, (int) glitchBuf[0].size());
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < gLen; ++k)
                glitchBuf[c][(size_t) k] = line[c].read ((float) (gLen - k));
        gPos = 0; gRate = 1; gReverse = false;
        gRepeatsPerCapture = 1 << 30;
    }
};

} // namespace wiga
