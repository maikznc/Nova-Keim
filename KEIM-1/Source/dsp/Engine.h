// KEIM-1 Klangerzeugung
// Reines C++17 ohne JUCE und ohne Speicheranforderungen im Audio-Thread.
// Derselbe Code soll später auf dem Daisy Seed laufen (dort mit festen Puffern im SDRAM).
#pragma once

#include "Params.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace keim {

constexpr float kPi = 3.14159265358979323846f;
constexpr int   kCtrl  = 16;   // Kontrollrate: Modulation alle 16 Samples
constexpr int   kPoly  = 8;    // gleichzeitig klingende Stimmen
constexpr int   kSlots = 12;   // + Reserve, damit gestohlene Stimmen sauber ausblenden

inline float mtof (float note) { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }

// ---------- Oszillator (PolyBLEP, wenig Aliasing) ----------
inline float polyBlep (double t, double dt)
{
    if (t < dt)       { t /= dt;               return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt;    return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}

struct Osc
{
    double phase = 0.0;
    inline float tick (int wave, double inc)
    {
        const double t = phase;
        float y;
        switch (wave)
        {
            case 0:  y = (float) (2.0 * t - 1.0) - polyBlep (t, inc); break;            // Säge
            case 1: {                                                                     // Rechteck
                y = t < 0.5 ? 1.0f : -1.0f;
                y += polyBlep (t, inc);
                double t2 = t + 0.5; if (t2 >= 1.0) t2 -= 1.0;
                y -= polyBlep (t2, inc);
                break;
            }
            case 2:  y = (float) (t < 0.25 ? 4.0 * t : (t < 0.75 ? 2.0 - 4.0 * t : 4.0 * t - 4.0)); break; // Dreieck
            default: y = std::sin (2.0f * kPi * (float) t); break;                       // Sinus
        }
        phase += inc;
        if (phase >= 1.0) phase -= 1.0;
        return y;
    }
};

// ---------- State-Variable-Filter (TPT, Zavalishin/Simper) ----------
struct SVF
{
    float ic1 = 0, ic2 = 0;
    void reset() { ic1 = ic2 = 0; }
    struct Coef { float a1 = 1, a2 = 0, a3 = 0, k = 1.4f; };
    static Coef make (float g, float k)
    {
        Coef c; c.k = k;
        c.a1 = 1.0f / (1.0f + g * (g + k)); c.a2 = g * c.a1; c.a3 = g * c.a2;
        return c;
    }
    // type: 0 Tiefpass, 1 Hochpass, 2 Bandpass (Spitze 0 dB)
    inline float tick (float v0, const Coef& c, int type)
    {
        const float v3 = v0 - ic2;
        const float v1 = c.a1 * ic1 + c.a2 * v3;
        const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        if (type == 0) return v2;
        if (type == 1) return v0 - c.k * v1 - v2;
        return c.k * v1;
    }
};

// ---------- Hüllkurve (gleiche Kurven wie im Browser) ----------
struct Env
{
    enum Stage { Idle, Attack, Decay, Release };
    Stage stage = Idle;
    float v = 0, peak = 1, sus = 0.7f, inc = 0, decCoef = 0, relCoef = 0;

    void trigger (float pk, float susLevel, float A, float D, float sr, bool fromCurrent)
    {
        if (! fromCurrent) v = 0;
        peak = pk; sus = susLevel;
        inc = (peak - v) / std::max (1.0f, A * sr);
        decCoef = std::exp (-1.0f / (std::max (D, 0.004f) / 4.0f * sr));
        stage = Attack;
    }
    void release (float R, float sr) { relCoef = std::exp (-1.0f / (std::max (R / 5.0f, 0.002f) * sr)); stage = Release; }
    void kill (float sr)             { relCoef = std::exp (-1.0f / (0.005f * sr)); stage = Release; }

    inline float tick()
    {
        switch (stage)
        {
            case Attack:
                v += inc;
                if ((inc >= 0 && v >= peak) || (inc < 0 && v <= peak)) { v = peak; stage = Decay; }
                break;
            case Decay:   v = sus + (v - sus) * decCoef; break;
            case Release: v *= relCoef; if (v < 2.0e-4f) { v = 0; stage = Idle; } break;
            case Idle:    break;
        }
        return v;
    }
};

// ---------- Feedback-Delay-Network-Hall ----------
class Reverb
{
public:
    static constexpr int N = 8;

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        static const float lineMs[N] = { 31.7f, 37.3f, 41.9f, 47.1f, 53.3f, 59.9f, 67.1f, 73.7f };
        static const float apMs[4]   = { 4.77f, 3.59f, 12.73f, 9.31f };
        for (int i = 0; i < N; ++i)
        {
            len[i] = std::max (2, (int) (lineMs[i] * 0.001f * sr));
            line[i].assign ((size_t) len[i], 0.0f); pos[i] = 0; lp[i] = 0;
        }
        for (int i = 0; i < 4; ++i)
        {
            apLen[i] = std::max (2, (int) (apMs[i] * 0.001f * sr));
            ap[i].assign ((size_t) apLen[i], 0.0f); apPos[i] = 0;
        }
        preLen = std::max (2, (int) (0.010f * sr));
        pre.assign ((size_t) preLen, 0.0f); prePos = 0;
        damp = 1.0f - std::exp (-2.0f * kPi * 5200.0f / sr);
        setDecay (2.0f);
    }

    void setDecay (float rt60)
    {
        rt60 = std::max (0.1f, rt60);
        for (int i = 0; i < N; ++i)
            g[i] = std::pow (10.0f, -3.0f * (float) len[i] / (rt60 * sr));
    }

    inline void tick (float in, float& outL, float& outR)
    {
        // Vorverzögerung
        float x = pre[(size_t) prePos]; pre[(size_t) prePos] = in; if (++prePos >= preLen) prePos = 0;
        // Diffusion
        for (int i = 0; i < 4; ++i)
        {
            float& d = ap[i][(size_t) apPos[i]];
            const float y = d - 0.62f * x;
            d = x + 0.62f * y;
            x = y;
            if (++apPos[i] >= apLen[i]) apPos[i] = 0;
        }
        float y[N]; float sum = 0;
        for (int i = 0; i < N; ++i)
        {
            const float r = line[i][(size_t) pos[i]];
            lp[i] += damp * (r - lp[i]);
            y[i] = lp[i]; sum += y[i];
        }
        const float s = sum * (2.0f / N);  // Householder-Matrix
        for (int i = 0; i < N; ++i)
        {
            line[i][(size_t) pos[i]] = (y[i] - s) * g[i] + ((i & 1) ? -x : x);
            if (++pos[i] >= len[i]) pos[i] = 0;
        }
        outL = (y[0] - y[2] + y[4] - y[6]) * kOut;
        outR = (y[1] - y[3] + y[5] - y[7]) * kOut;
    }

    void reset()
    {
        for (int i = 0; i < N; ++i) { std::fill (line[i].begin(), line[i].end(), 0.0f); lp[i] = 0; }
        for (auto& a : ap) std::fill (a.begin(), a.end(), 0.0f);
        std::fill (pre.begin(), pre.end(), 0.0f);
    }

    static constexpr float kOut = 0.245f;  // so eingestellt, dass der Hall etwa so laut ist wie im Browser

private:
    float sr = 48000, damp = 0.5f;
    std::vector<float> line[N]; int len[N] {}, pos[N] {}; float g[N] {}, lp[N] {};
    std::vector<float> ap[4]; int apLen[4] {}, apPos[4] {};
    std::vector<float> pre; int preLen = 2, prePos = 0;
};

// ---------- Engine ----------
class Engine
{
public:
    Engine() { for (int i = 0; i < kNumParams; ++i) target[(size_t) i] = cur[(size_t) i] = spec (i).def; }

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        smoothCoef = 1.0f - std::exp (-(float) kCtrl / (0.015f * sr));
        delayCoef  = 1.0f - std::exp (-1.0f / (0.05f * sr));
        chorusBuf.assign ((size_t) (0.05f * sr) + 8, 0.0f);
        dlyL.assign ((size_t) (2.0f * sr) + 8, 0.0f);
        dlyR.assign ((size_t) (2.0f * sr) + 8, 0.0f);
        reverb.prepare (sr);
        reverb.setDecay (target[pRSize] * 0.8f);
        compAtt = std::exp (-1.0f / (0.0015f * sr));
        compRel = std::exp (-1.0f / (0.2f * sr));
        toneCoef = SVF::make (std::tan (kPi * std::min (3800.0f, 0.45f * sr) / sr), 1.0f / 1.12f);
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v = Voice();
        std::fill (chorusBuf.begin(), chorusBuf.end(), 0.0f);
        std::fill (dlyL.begin(), dlyL.end(), 0.0f);
        std::fill (dlyR.begin(), dlyR.end(), 0.0f);
        reverb.reset(); tone.reset();
        for (auto& k : keyDown) k = false;
        for (auto& k : sustained) k = false;
        nDown = 0; nHeld = 0; monoCount = 0; monoSlot = -1; lastNote = -1; arpRunning = false;
        for (auto& p : pending) p.used = false;
        ctrlLeft = 0; grSm = 0;
        dTime = delayTarget() * sr;
        for (int i = 0; i < kNumParams; ++i) cur[(size_t) i] = target[(size_t) i];
    }

    // ----- Parameter (aus dem Audio-Thread, jeden Block) -----
    void setParam (int id, float v)
    {
        const float old = target[(size_t) id];
        if (! (old < v || old > v)) return;  // unverändert
        target[(size_t) id] = v;
        if (spec (id).kind != Kind::Float) cur[(size_t) id] = v;
        switch (id)
        {
            case pMode:  if ((int) old != (int) v) releaseAllVoices(); break;
            case pRSize: reverbDirty = true; break;
            case pArpOn: {
                const bool on = v >= 0.5f;
                if (on == (old >= 0.5f)) break;
                if (on) { releaseAllVoices(); nHeld = 0; for (int n = 0; n < 128; ++n) if (keyDown[n]) arpAddRaw (n); fresh = false; arpRunning = false; }
                else    { stopArp(); for (int n = 0; n < 128; ++n) if (keyDown[n]) voiceNoteOn (n, 0.85f); }
                break;
            }
            case pHold:
                if (v < 0.5f && old >= 0.5f)
                {
                    int w = 0;
                    for (int i = 0; i < nHeld; ++i) if (keyDown[held[i]]) held[w++] = held[i];
                    nHeld = w;
                }
                break;
            default: break;
        }
    }
    float param (int id) const { return target[(size_t) id]; }

    // ----- Noten -----
    void noteOn (int note, float vel)
    {
        if (note < 0 || note > 127) return;
        if (vel <= 0.0f) { noteOff (note); return; }
        if (! keyDown[note]) { keyDown[note] = true; ++nDown; }
        sustained[note] = false;
        if (arpOn()) { arpAdd (note); return; }
        voiceNoteOn (note, vel);
    }
    void noteOff (int note)
    {
        if (note < 0 || note > 127) return;
        if (keyDown[note]) { keyDown[note] = false; --nDown; }
        if (arpOn()) { arpRemove (note); return; }
        if (sustainPedal && mode() == 0) { sustained[note] = true; return; }
        voiceNoteOff (note);
    }
    void setSustain (bool on)
    {
        sustainPedal = on;
        if (on) return;
        for (int n = 0; n < 128; ++n)
            if (sustained[n]) { sustained[n] = false; if (! keyDown[n]) voiceNoteOff (n); }
    }
    void pitchBend (float semis) { bend = semis; }
    void modWheel (float v)      { mod = std::min (1.0f, std::max (0.0f, v)); }
    void setHostTempo (double bpm, bool valid) { hostBpm = (float) bpm; hostValid = valid && bpm > 20.0; }
    void allNotesOff()
    {
        stopArp();
        for (auto& v : voices) if (v.active) killVoice (v);
        for (int n = 0; n < 128; ++n) { keyDown[n] = false; sustained[n] = false; }
        nDown = 0; monoCount = 0; monoSlot = -1;
    }

    // ----- Rendern -----
    void process (float* L, float* R, int n)
    {
        int i = 0;
        while (i < n)
        {
            if (ctrlLeft == 0) { controlTick(); ctrlLeft = kCtrl; }
            const int m = std::min (ctrlLeft, n - i);
            render (L + i, R + i, m);
            i += m; ctrlLeft -= m;
        }
    }

    int activeVoices() const { return voiceCount.load (std::memory_order_relaxed); }
    int arpStep() const      { return arpStepIdx.load (std::memory_order_relaxed); }
    float tempo() const      { return (target[pSync] >= 0.5f && hostValid) ? hostBpm : target[pBpm]; }

private:
    struct Voice
    {
        bool active = false, released = false, killed = false;
        int note = 60; float vel = 0.85f; uint32_t age = 0;
        float curNote = 60, glideStep = 0;
        int releaseIn = -1;
        Osc o1, o2, os; uint32_t rng = 22222;
        SVF l1, l2, r1, r2; SVF::Coef c1, c2;
        Env amp, fenv;
        double inc1 = 0, inc2 = 0, incS = 0;
    };
    struct Pending { int note = 0; float vel = 0; int delay = 0; int gate = 0; bool used = false; };

    // Parameter
    std::array<float, kNumParams> target {}, cur {};
    float sr = 48000, smoothCoef = 0.1f, delayCoef = 0.001f;
    bool reverbDirty = false;
    float bend = 0, mod = 0, hostBpm = 120; bool hostValid = false;

    int mode() const  { return (int) target[pMode]; }
    bool arpOn() const { return target[pArpOn] >= 0.5f; }

    // Stimmen
    std::array<Voice, kSlots> voices {};
    uint32_t ageCounter = 0;
    int monoStack[16] {}; int monoCount = 0; int monoSlot = -1; int lastNote = -1;
    bool keyDown[128] {}; bool sustained[128] {}; int nDown = 0; bool sustainPedal = false;
    std::atomic<int> voiceCount { 0 }, arpStepIdx { -1 };

    // Arp
    int held[16] {}; int nHeld = 0; bool fresh = false; bool arpRunning = false;
    int arpIdx = 0; uint32_t stepCount = 0; double toNext = 0;
    std::array<Pending, 8> pending {};
    uint32_t rng = 0x1234567u;
    float rand01() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) (rng >> 8) * (1.0f / 16777216.0f); }

    // Modulation & Kontrollwerte
    int ctrlLeft = 0;
    float lfoPhase = 0, lfoVal = 0;
    float g1L = 0, g1R = 0, g2L = 0, g2R = 0, gSub = 0, gNoise = 0, busGain = 1;
    float driveK = 1, driveNorm = 1; bool driveOn = false;
    float chWet = 0, chDry = 1, dSend = 0, dFb = 0, rSend = 0;

    // Effekte
    std::vector<float> chorusBuf; int chPos = 0; float chPh1 = 0, chPh2 = 0.37f;
    std::vector<float> dlyL, dlyR; int dPos = 0; float dTime = 0.3f;
    SVF tone; SVF::Coef toneCoef;
    Reverb reverb;
    float compAtt = 0.99f, compRel = 0.999f, grSm = 0;

    // ---------- Stimmenverwaltung ----------
    int liveCount() const { int c = 0; for (auto& v : voices) if (v.active && ! v.killed) ++c; return c; }

    int allocSlot()
    {
        if (liveCount() >= kPoly)
        {
            int victim = -1;
            for (int pass = 0; pass < 2 && victim < 0; ++pass)
                for (int i = 0; i < kSlots; ++i)
                {
                    auto& v = voices[(size_t) i];
                    if (! v.active || v.killed || (pass == 0 && ! v.released)) continue;
                    if (victim < 0 || v.age < voices[(size_t) victim].age) victim = i;
                }
            if (victim >= 0) killVoice (voices[(size_t) victim]);
        }
        for (int i = 0; i < kSlots; ++i) if (! voices[(size_t) i].active) return i;
        int oldest = 0;
        for (int i = 1; i < kSlots; ++i) if (voices[(size_t) i].age < voices[(size_t) oldest].age) oldest = i;
        return oldest;
    }

    void startVoice (int slot, int note, float vel, int fromNote)
    {
        auto& v = voices[(size_t) slot];
        v = Voice();
        v.active = true; v.note = note; v.vel = vel; v.age = ++ageCounter;
        v.rng = 0x9E3779B9u ^ (uint32_t) (note * 7919 + (int) ageCounter * 104729);
        v.o1.phase = v.o2.phase = 0.5; v.os.phase = 0.0;
        const float glide = target[pGlide];
        if (fromNote >= 0 && glide > 0.002f) { v.curNote = (float) fromNote; v.glideStep = (float) (note - fromNote) / (glide * sr); }
        else                                 { v.curNote = (float) note; v.glideStep = 0; }
        v.amp.trigger (vel, vel * target[pAS], target[pAA], target[pAD], sr, false);
        v.fenv.trigger (1.0f, target[pFS], target[pFA], target[pFD], sr, false);
        updateVoice (v, 0);
    }
    void setVoiceNote (Voice& v, int note)
    {
        const float glide = target[pGlide];
        v.note = note;
        if (glide > 0.002f) v.glideStep = ((float) note - v.curNote) / (glide * sr);
        else { v.curNote = (float) note; v.glideStep = 0; }
    }
    void retrigger (Voice& v, float vel)
    {
        v.released = false;
        v.amp.trigger (vel, vel * target[pAS], target[pAA], target[pAD], sr, true);
        v.fenv.trigger (1.0f, target[pFS], target[pFA], target[pFD], sr, true);
    }
    void releaseVoice (Voice& v)
    {
        if (v.released) return;
        v.released = true;
        v.amp.release (target[pAR], sr);
        v.fenv.release (target[pFR], sr);
    }
    void killVoice (Voice& v) { v.released = true; v.killed = true; v.amp.kill (sr); }

    int findHeld (int note) const
    {
        for (int i = 0; i < kSlots; ++i)
        {
            const auto& v = voices[(size_t) i];
            if (v.active && ! v.released && v.note == note && v.releaseIn < 0) return i;
        }
        return -1;
    }

    void voiceNoteOn (int note, float vel)
    {
        if (mode() == 0)
        {
            const int old = findHeld (note);
            if (old >= 0) releaseVoice (voices[(size_t) old]);
            startVoice (allocSlot(), note, vel, -1);
        }
        else
        {
            int w = 0;
            for (int i = 0; i < monoCount; ++i) if (monoStack[i] != note) monoStack[w++] = monoStack[i];
            monoCount = w;
            if (monoCount < 16) monoStack[monoCount++] = note;
            Voice* mv = monoSlot >= 0 ? &voices[(size_t) monoSlot] : nullptr;
            if (mv && mv->active && ! mv->released)
            {
                setVoiceNote (*mv, note);
                if (mode() == 1) retrigger (*mv, vel);
            }
            else
            {
                const int from = (target[pGlide] > 0.002f && lastNote >= 0) ? lastNote : -1;
                if (mv && mv->active) killVoice (*mv);
                monoSlot = allocSlot();
                startVoice (monoSlot, note, vel, from);
            }
        }
        lastNote = note;
    }

    void voiceNoteOff (int note)
    {
        if (mode() == 0)
        {
            const int s = findHeld (note);
            if (s >= 0) releaseVoice (voices[(size_t) s]);
            return;
        }
        int idx = -1;
        for (int i = 0; i < monoCount; ++i) if (monoStack[i] == note) idx = i;
        if (idx < 0) return;
        for (int i = idx; i < monoCount - 1; ++i) monoStack[i] = monoStack[i + 1];
        --monoCount;
        Voice* mv = monoSlot >= 0 ? &voices[(size_t) monoSlot] : nullptr;
        if (! mv || ! mv->active || mv->released) return;
        if (mv->note == note)
        {
            if (monoCount > 0)
            {
                const int n = monoStack[monoCount - 1];
                setVoiceNote (*mv, n); lastNote = n;
                if (mode() == 1) retrigger (*mv, 0.8f);
            }
            else releaseVoice (*mv);
        }
    }

    void releaseAllVoices()
    {
        for (auto& v : voices) if (v.active) releaseVoice (v);
        monoCount = 0; monoSlot = -1;
    }

    // Arp-Note mit festem Gate
    void playStep (int note, float vel, int gateSamples)
    {
        if (mode() == 0)
        {
            const int s = allocSlot();
            startVoice (s, note, vel, -1);
            voices[(size_t) s].releaseIn = std::max (1, gateSamples);
        }
        else
        {
            const int from = (target[pGlide] > 0.002f && lastNote >= 0) ? lastNote : -1;
            if (monoSlot >= 0 && voices[(size_t) monoSlot].active) killVoice (voices[(size_t) monoSlot]);
            monoSlot = allocSlot();
            startVoice (monoSlot, note, vel, from);
            voices[(size_t) monoSlot].releaseIn = std::max (1, gateSamples);
        }
        lastNote = note;
    }

    // ---------- Arpeggiator ----------
    void arpAddRaw (int n)
    {
        for (int i = 0; i < nHeld; ++i) if (held[i] == n) return;
        if (nHeld < 16) held[nHeld++] = n;
    }
    void arpAdd (int n)
    {
        if (target[pHold] >= 0.5f && fresh) { nHeld = 0; fresh = false; }
        arpAddRaw (n);
    }
    void arpRemove (int n)
    {
        if (target[pHold] >= 0.5f) { if (nDown == 0) fresh = true; return; }
        int w = 0;
        for (int i = 0; i < nHeld; ++i) if (held[i] != n) held[w++] = held[i];
        nHeld = w;
    }
    void stopArp()
    {
        nHeld = 0; fresh = false; arpRunning = false;
        for (auto& p : pending) p.used = false;
        arpStepIdx.store (-1, std::memory_order_relaxed);
    }

    int buildSeq (int* out) const
    {
        int base[16]; int nb = nHeld;
        for (int i = 0; i < nb; ++i) base[i] = held[i];
        for (int i = 1; i < nb; ++i) for (int j = i; j > 0 && base[j - 1] > base[j]; --j) std::swap (base[j - 1], base[j]);
        const int octs = (int) target[pArpOct] + 1;
        int n = 0;
        for (int o = 0; o < octs; ++o) for (int i = 0; i < nb; ++i) out[n++] = base[i] + 12 * o;
        const int m = (int) target[pArpMode];
        if (m == 1) for (int i = 0; i < n / 2; ++i) std::swap (out[i], out[n - 1 - i]);
        if (m == 2 && n > 2) { const int k = n; for (int i = k - 2; i >= 1; --i) out[n++] = out[i]; }
        return n;
    }

    void arpPlay (double stepSamples)
    {
        int seq[100];
        const int n = buildSeq (seq);
        if (n == 0) return;
        int note = (int) target[pArpMode] == 3 ? seq[(int) (rand01() * (float) n) % n] : seq[arpIdx % n];
        ++arpIdx;
        arpStepIdx.store ((int) (stepCount++ % 8), std::memory_order_relaxed);
        const float m = target[pArpMut], r = rand01();
        const int gate = (int) (stepSamples * target[pArpGate]);
        if (r < m * 0.2f) return;                       // Pause
        bool ratchet = false;
        if (r < m * 0.4f)       note += 12;             // Oktave hoch
        else if (r < m * 0.5f)  note -= 12;             // Oktave runter
        else if (r < m * 0.65f) ratchet = true;         // Doppelschlag
        note = std::min (115, std::max (12, note));
        if (ratchet)
        {
            const int g = (int) (std::min ((double) gate, stepSamples / 2) * 0.9);
            playStep (note, 0.8f, g);
            for (auto& p : pending)
                if (! p.used) { p = { note, 0.62f, (int) (stepSamples / 2), g, true }; break; }
        }
        else playStep (note, 0.85f, gate);
    }

    void arpControl (int n)
    {
        for (auto& p : pending)
            if (p.used && (p.delay -= n) <= 0) { p.used = false; playStep (p.note, p.vel, p.gate); }
        if (! arpOn() || nHeld == 0) { if (arpRunning) { arpRunning = false; arpStepIdx.store (-1); } return; }
        static const double rateBeats[] = { 0.5, 0.25, 1.0 / 3.0, 0.125 };
        const double stepSamples = 60.0 / std::max (20.0f, tempo()) * rateBeats[std::min (3, (int) target[pArpRate])] * sr;
        if (! arpRunning) { arpRunning = true; toNext = 0; arpIdx = 0; }
        while (toNext <= 0) { arpPlay (stepSamples); toNext += stepSamples; }
        toNext -= n;
    }

    // ---------- Kontrollrate ----------
    float delayTarget() const
    {
        static const float div[] = { 0.25f, 0.5f, 0.75f, 1.0f };
        return std::min (1.9f, 60.0f / std::max (20.0f, tempo()) * div[std::min (3, (int) target[pDDiv])]);
    }

    void updateVoice (Voice& v, int n)
    {
        if (std::fabs (v.glideStep) > 0.0f)
        {
            v.curNote += v.glideStep * (float) n;
            if ((v.glideStep > 0 && v.curNote >= (float) v.note) || (v.glideStep < 0 && v.curNote <= (float) v.note))
            { v.curNote = (float) v.note; v.glideStep = 0; }
        }
        const float lp = std::max (cur[pLfoPitch], mod);
        const float pitchMod = bend + lp * lp * 80.0f * lfoVal / 100.0f;
        const float base = v.curNote + pitchMod;
        const float det = cur[pDetune] / 200.0f;
        const float nyq = 0.48f;
        v.inc1 = std::min (nyq, mtof (base - det) / sr);
        v.inc2 = std::min (nyq, mtof (base + cur[pSemi] + det) / sr);
        v.incS = std::min (nyq, mtof (base) * 0.5f / sr);

        for (int i = 0; i < n; ++i) v.fenv.tick();
        const float baseCut = std::min (20000.0f, std::max (20.0f, cur[pCutoff] * std::exp2 (((float) v.note - 60.0f) / 12.0f * cur[pKtrack])));
        const float cents = cur[pFenv] * 7200.0f * v.fenv.v + cur[pLfoFilter] * 2400.0f * lfoVal;
        const float fc = std::min (0.45f * sr, std::max (20.0f, baseCut * std::exp2 (cents / 1200.0f)));
        const float g = std::tan (kPi * fc / sr);
        const int type = (int) target[pFilterType];
        if (type == 2) { v.c1 = SVF::make (g, 1.0f / 0.8f); v.c2 = SVF::make (g, 1.0f / (0.8f + cur[pRes] * 14.0f)); }
        else           { v.c1 = SVF::make (g, 1.0f / 0.708f); v.c2 = SVF::make (g, 1.0f / std::pow (10.0f, (-1.0f + cur[pRes] * 23.0f) / 20.0f)); }
    }

    void controlTick()
    {
        for (int i = 0; i < kNumParams; ++i)
            if (spec (i).kind == Kind::Float) cur[(size_t) i] += (target[(size_t) i] - cur[(size_t) i]) * smoothCoef;
        if (reverbDirty) { reverb.setDecay (target[pRSize] * 0.8f); reverbDirty = false; }

        lfoPhase += cur[pLfoRate] * (float) kCtrl / sr;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        lfoVal = std::sin (2.0f * kPi * lfoPhase);

        arpControl (kCtrl);

        int count = 0;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            if (v.releaseIn > 0 && (v.releaseIn -= kCtrl) <= 0) { v.releaseIn = 0; releaseVoice (v); }
            updateVoice (v, kCtrl);
            if (! v.killed) ++count;
        }
        voiceCount.store (std::min (count, kPoly), std::memory_order_relaxed);

        // Mischung (Equal-Power wie StereoPanner im Browser)
        const float L = 0.3f, mix = cur[pMix], sp = cur[pSpread] * 0.85f;
        const float a = std::cos (mix * kPi / 2) * L, b = std::sin (mix * kPi / 2) * L;
        const float x1 = (-sp + 1.0f) * 0.5f, x2 = (sp + 1.0f) * 0.5f;
        g1L = a * std::cos (x1 * kPi / 2); g1R = a * std::sin (x1 * kPi / 2);
        g2L = b * std::cos (x2 * kPi / 2); g2R = b * std::sin (x2 * kPi / 2);
        gSub = cur[pSub] * L; gNoise = cur[pNoise] * 0.22f;
        busGain = cur[pLevel] * 1.6f;
        driveOn = cur[pDrive] >= 0.01f;
        driveK = 1.0f + cur[pDrive] * 24.0f; driveNorm = 1.0f / std::sqrt (driveK);
        chWet = cur[pChorus] * 0.75f; chDry = 1.0f - cur[pChorus] * 0.3f;
        dSend = cur[pDMix] * 0.8f; dFb = cur[pDFb]; rSend = cur[pRMix] * 0.9f;
    }

    // ---------- Audio ----------
    inline float readFrac (const std::vector<float>& buf, int writePos, float delaySamples) const
    {
        const int size = (int) buf.size();
        float rp = (float) writePos - delaySamples;
        while (rp < 0) rp += (float) size;
        const int i0 = (int) rp; const float fr = rp - (float) i0;
        const int i1 = (i0 + 1) % size;
        return buf[(size_t) (i0 % size)] * (1.0f - fr) + buf[(size_t) i1] * fr;
    }

    void render (float* outL, float* outR, int m)
    {
        const int w1 = (int) target[pOsc1Wave], w2 = (int) target[pOsc2Wave], ft = std::min (2, (int) target[pFilterType]);
        const int ftype = ft == 0 ? 0 : (ft == 1 ? 1 : 2);
        const bool useNoise = gNoise > 0.0001f;
        const float dTarget = delayTarget() * sr;

        for (int s = 0; s < m; ++s)
        {
            float sumL = 0, sumR = 0;
            for (int vi = 0; vi < kSlots; ++vi)
            {
                auto& v = voices[(size_t) vi];
                if (! v.active) continue;
                const float a = v.amp.tick();
                if (v.amp.stage == Env::Idle && v.released)
                {
                    v.active = false;
                    if (monoSlot == vi) monoSlot = -1;
                    continue;
                }
                const float s1 = v.o1.tick (w1, v.inc1);
                const float s2 = v.o2.tick (w2, v.inc2);
                const float ss = v.os.tick (1, v.incS);
                float nz = 0;
                if (useNoise) { v.rng ^= v.rng << 13; v.rng ^= v.rng >> 17; v.rng ^= v.rng << 5; nz = (float) (int32_t) v.rng * (1.0f / 2147483648.0f); }
                const float common = ss * gSub + nz * gNoise;
                const float inL = s1 * g1L + s2 * g2L + common;
                const float inR = s1 * g1R + s2 * g2R + common;
                const float yL = v.l2.tick (v.l1.tick (inL, v.c1, ftype), v.c2, ftype);
                const float yR = v.r2.tick (v.r1.tick (inR, v.c1, ftype), v.c2, ftype);
                sumL += yL * a; sumR += yR * a;
            }

            // Bus + Drive
            float l = sumL * busGain, r = sumR * busGain;
            if (driveOn) { l = std::tanh (driveK * l) * driveNorm; r = std::tanh (driveK * r) * driveNorm; }

            // Chorus: zwei modulierte Verzögerungen, links und rechts betont
            const float mono = (l + r) * 0.5f;
            chorusBuf[(size_t) chPos] = mono;
            chPh1 += 0.55f / sr; if (chPh1 >= 1) chPh1 -= 1;
            chPh2 += 0.77f / sr; if (chPh2 >= 1) chPh2 -= 1;
            const float c1 = readFrac (chorusBuf, chPos, (0.013f + 0.0025f * std::sin (2 * kPi * chPh1)) * sr);
            const float c2 = readFrac (chorusBuf, chPos, (0.019f + 0.0025f * std::sin (2 * kPi * chPh2)) * sr);
            if (++chPos >= (int) chorusBuf.size()) chPos = 0;
            float cl = l * chDry + (c1 * 1.891f + c2 * 0.454f) * chWet;
            float cr = r * chDry + (c1 * 0.454f + c2 * 1.891f) * chWet;

            // Ping-Pong-Delay
            dTime += (dTarget - dTime) * delayCoef;
            const float dl = readFrac (dlyL, dPos, dTime);
            const float dr = readFrac (dlyR, dPos, dTime);
            const float din = tone.tick ((cl + cr) * 0.5f * dSend + dr * dFb, toneCoef, 0);
            dlyL[(size_t) dPos] = din;
            dlyR[(size_t) dPos] = dl;
            if (++dPos >= (int) dlyL.size()) dPos = 0;
            float fl = cl + dl, fr = cr + dr;

            // Hall
            float rl, rr;
            reverb.tick ((fl + fr) * 0.5f * rSend, rl, rr);
            float ml = (fl + rl) * 0.9f, mr = (fr + rr) * 0.9f;

            // Kompressor (Schwelle -12 dB, 6:1, weiches Knie, +6 dB Aufholverstärkung wie im Browser)
            const float peak = std::max (std::fabs (ml), std::fabs (mr));
            const float xdb = 20.0f * std::log10 (peak + 1.0e-9f);
            const float T = -12.0f, W = 8.0f, Rt = 6.0f;
            float ydb;
            if (2.0f * (xdb - T) < -W)              ydb = xdb;
            else if (2.0f * std::fabs (xdb - T) <= W) { const float d = xdb - T + W / 2; ydb = xdb + (1.0f / Rt - 1.0f) * d * d / (2.0f * W); }
            else                                     ydb = T + (xdb - T) / Rt;
            const float gr = ydb - xdb;
            grSm = gr < grSm ? compAtt * grSm + (1 - compAtt) * gr : compRel * grSm + (1 - compRel) * gr;
            const float gain = std::pow (10.0f, grSm / 20.0f) * 2.0f;
            ml *= gain; mr *= gain;

            outL[s] = softClip (ml);
            outR[s] = softClip (mr);
        }
    }

    static inline float softClip (float x)
    {
        if (! std::isfinite (x)) return 0.0f;
        const float a = std::fabs (x);
        if (a <= 0.9f) return x;
        const float y = 0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f);
        return x < 0 ? -y : y;
    }
};

} // namespace keim
