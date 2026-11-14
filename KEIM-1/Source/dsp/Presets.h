// KEIM-1 Werkssounds und Zufallsgenerator
#pragma once

#include "Params.h"
#include <array>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

namespace keim {

struct Patch
{
    std::array<float, kNumParams> v {};
    float& operator[] (int id)       { return v[(size_t) id]; }
    float  operator[] (int id) const { return v[(size_t) id]; }
};

inline Patch defaultPatch()
{
    Patch p;
    for (int i = 0; i < kNumParams; ++i) p[i] = spec (i).def;
    return p;
}

struct PresetDef
{
    const char* name;
    std::vector<std::pair<int, float>> values;
};

// Wellen: 0 Säge, 1 Rechteck, 2 Dreieck, 3 Sinus · Spielweise: 0 Poly, 1 Mono, 2 Legato
// Delay-Zeit: 0 1/16, 1 1/8, 2 1/8 punktiert, 3 1/4
inline const std::vector<PresetDef>& factoryPresets()
{
    static const std::vector<PresetDef> list = {
        { "Acid Bass", { {pOsc1Wave,0},{pOsc2Wave,1},{pMix,.15f},{pDetune,0},{pSub,.3f},{pSpread,0},{pCutoff,320},{pRes,.78f},{pFenv,.65f},{pKtrack,.3f},
                         {pFA,.002f},{pFD,.22f},{pFS,.05f},{pFR,.15f},{pAA,.002f},{pAD,.3f},{pAS,.8f},{pAR,.08f},{pMode,2},{pGlide,.07f},{pDrive,.45f},
                         {pDMix,.12f},{pDFb,.3f},{pRMix,.08f},{pRSize,1.2f} } },
        { "Sub Bass", { {pOsc1Wave,3},{pOsc2Wave,2},{pMix,.3f},{pSemi,0},{pDetune,0},{pSub,.4f},{pSpread,0},{pCutoff,900},{pRes,.05f},{pFenv,.1f},{pKtrack,.2f},
                        {pAA,.004f},{pAD,.5f},{pAS,.85f},{pAR,.12f},{pMode,1},{pGlide,.03f},{pDrive,.15f},{pDMix,0},{pRMix,.03f} } },
        { "Reese Bass", { {pOsc1Wave,0},{pOsc2Wave,0},{pMix,.5f},{pDetune,28},{pSub,.35f},{pSpread,.5f},{pCutoff,650},{pRes,.25f},{pFenv,.15f},{pKtrack,.3f},
                          {pFA,.01f},{pFD,.6f},{pFS,.5f},{pFR,.3f},{pAA,.005f},{pAD,1},{pAS,.9f},{pAR,.2f},{pMode,1},{pGlide,.04f},{pLfoRate,.3f},
                          {pLfoFilter,.15f},{pDrive,.3f},{pChorus,.45f},{pDMix,0},{pRMix,.1f},{pRSize,1.5f} } },
        { "Supersaw Lead", { {pOsc1Wave,0},{pOsc2Wave,0},{pMix,.5f},{pDetune,22},{pSub,.1f},{pSpread,.8f},{pCutoff,5200},{pRes,.15f},{pFenv,.25f},{pKtrack,.6f},
                             {pFA,.005f},{pFD,.4f},{pFS,.4f},{pFR,.3f},{pAA,.01f},{pAD,.3f},{pAS,.85f},{pAR,.35f},{pMode,0},{pLfoRate,5.5f},{pLfoPitch,.1f},
                             {pChorus,.4f},{pDMix,.28f},{pDFb,.4f},{pDDiv,2},{pRMix,.3f},{pRSize,2.8f} } },
        { "Square Lead", { {pOsc1Wave,1},{pOsc2Wave,1},{pMix,.35f},{pSemi,12},{pDetune,6},{pSpread,.3f},{pCutoff,2800},{pRes,.3f},{pFenv,.35f},
                           {pFA,.005f},{pFD,.25f},{pFS,.4f},{pAA,.005f},{pAD,.2f},{pAS,.9f},{pAR,.2f},{pMode,2},{pGlide,.12f},{pLfoRate,5.2f},{pLfoPitch,.18f},
                           {pDrive,.2f},{pDMix,.25f},{pDFb,.45f},{pDDiv,3},{pRMix,.2f} } },
        { "Warm Pad", { {pOsc1Wave,0},{pOsc2Wave,2},{pMix,.45f},{pDetune,14},{pSub,.15f},{pSpread,.9f},{pCutoff,1400},{pRes,.12f},{pFenv,.2f},{pKtrack,.5f},
                        {pFA,1.2f},{pFD,2},{pFS,.6f},{pFR,2.5f},{pAA,1.1f},{pAD,1.5f},{pAS,.85f},{pAR,2.8f},{pMode,0},{pLfoRate,.35f},{pLfoFilter,.25f},
                        {pChorus,.7f},{pDMix,.18f},{pDFb,.4f},{pDDiv,3},{pRMix,.55f},{pRSize,4.5f} } },
        { "Glass Pluck", { {pOsc1Wave,2},{pOsc2Wave,3},{pMix,.45f},{pSemi,19},{pDetune,4},{pSpread,.5f},{pCutoff,1800},{pRes,.35f},{pFenv,.55f},{pKtrack,.7f},
                           {pFA,.001f},{pFD,.25f},{pFS,0},{pFR,.3f},{pAA,.001f},{pAD,.55f},{pAS,0},{pAR,.5f},{pMode,0},{pChorus,.2f},{pDMix,.35f},
                           {pDFb,.5f},{pDDiv,2},{pRMix,.35f},{pRSize,3} } },
        { "E-Keys", { {pOsc1Wave,2},{pOsc2Wave,1},{pMix,.3f},{pSemi,12},{pDetune,5},{pCutoff,2200},{pRes,.1f},{pFenv,.3f},{pFA,.002f},{pFD,.6f},{pFS,.2f},
                      {pFR,.4f},{pAA,.003f},{pAD,1.4f},{pAS,.35f},{pAR,.4f},{pMode,0},{pLfoRate,4.5f},{pChorus,.55f},{pDMix,.1f},{pRMix,.22f} } },
        { "Brass", { {pOsc1Wave,0},{pOsc2Wave,0},{pMix,.5f},{pDetune,10},{pCutoff,700},{pRes,.1f},{pFenv,.6f},{pKtrack,.5f},{pFA,.08f},{pFD,.5f},{pFS,.5f},
                     {pFR,.25f},{pAA,.03f},{pAD,.4f},{pAS,.85f},{pAR,.25f},{pMode,0},{pLfoRate,5},{pLfoPitch,.08f},{pChorus,.2f},{pRMix,.25f} } },
    };
    return list;
}

inline int numFactoryPresets() { return (int) factoryPresets().size(); }

// Kompletter Sound eines Werkspresets (nur Klang-Parameter sind relevant)
inline Patch factoryPatch (int index)
{
    Patch p = defaultPatch();
    const auto& list = factoryPresets();
    if (index < 0 || index >= (int) list.size()) return p;
    for (const auto& kv : list[(size_t) index].values) p[kv.first] = kv.second;
    return p;
}

inline int findFactoryPreset (const std::string& name)
{
    const auto& list = factoryPresets();
    for (size_t i = 0; i < list.size(); ++i) if (name == list[i].name) return (int) i;
    return -1;
}

// Standard-Ecken des Morph-Pads: oben links, oben rechts, unten links, unten rechts
inline std::array<int, 4> defaultCorners() { return { 0, 3, 5, 6 }; }

// Bilineare Gewichte für Pad-Position x,y (0..1, y=0 oben)
inline std::array<float, 4> morphWeights (float x, float y)
{
    return { (1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y };
}

// Überblendet vier Sounds: stufenlose Parameter in Reglerstellung, Auswahlen von der stärksten Ecke
inline Patch morphPatches (const std::array<Patch, 4>& corners, float x, float y, const Patch& base)
{
    const auto w = morphWeights (x, y);
    int best = 0;
    for (int i = 1; i < 4; ++i) if (w[(size_t) i] > w[(size_t) best]) best = i;
    Patch out = base;
    for (int id = 0; id < kNumParams; ++id)
    {
        if (! isMorphParam (id)) continue;
        if (spec (id).kind == Kind::Choice) { out[id] = corners[(size_t) best][id]; continue; }
        float n = 0;
        for (size_t i = 0; i < 4; ++i) n += w[i] * toNorm (id, corners[i][id]);
        out[id] = fromNorm (id, n);
    }
    return out;
}

// ---------- Würfeln ----------
struct Rng
{
    uint32_t s;
    explicit Rng (uint32_t seed) : s (seed ? seed : 0x9E3779B9u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
    float range (float a, float b) { return a + uni() * (b - a); }
    template <typename T> T pick (std::initializer_list<T> l) { return *(l.begin() + (size_t) (uni() * (float) l.size()) % l.size()); }
};

inline Patch randomPatch (uint32_t seed, const Patch& base, std::string* kindName = nullptr)
{
    Rng r (seed);
    Patch p = base;
    const int kind = (int) (r.uni() * 4.0f) % 4;  // 0 Bass, 1 Lead, 2 Pluck, 3 Pad
    auto expRange = [&] (float a, float b) { return std::exp (r.range (std::log (a), std::log (b))); };

    p[pOsc1Wave] = (float) (r.next() % 4); p[pOsc2Wave] = (float) (r.next() % 4);
    p[pMix] = r.range (.1f, .9f);  p[pSemi] = r.pick ({ 0.f, 0.f, 0.f, 12.f, -12.f, 7.f, 19.f, 5.f });
    p[pDetune] = r.range (0, 30);  p[pSpread] = r.uni();
    p[pNoise] = r.uni() < .2f ? r.range (.05f, .3f) : 0.f;
    p[pFilterType] = r.uni() < .8f ? 0.f : (float) (1 + r.next() % 2);
    p[pCutoff] = expRange (300, 7000); p[pRes] = r.range (0, .65f); p[pFenv] = r.range (-.2f, .8f); p[pKtrack] = r.range (.2f, .8f);
    p[pFA] = r.range (.001f, .05f); p[pFD] = r.range (.1f, 1); p[pFS] = r.range (0, .6f); p[pFR] = r.range (.1f, .8f);
    p[pAA] = .005f; p[pAD] = .4f; p[pAS] = .7f; p[pAR] = .3f;
    p[pLfoRate] = r.range (.2f, 7); p[pLfoFilter] = r.uni() < .4f ? r.range (0, .4f) : 0.f; p[pLfoPitch] = r.uni() < .3f ? r.range (0, .2f) : 0.f;
    p[pDrive] = r.range (0, .4f); p[pChorus] = r.range (0, .6f); p[pDMix] = r.range (0, .35f); p[pDFb] = r.range (.2f, .6f);
    p[pDDiv] = (float) (r.next() % 4); p[pRMix] = r.range (.05f, .45f); p[pRSize] = r.range (1, 4);
    p[pMode] = 0; p[pGlide] = 0; p[pSub] = r.range (0, .3f);

    switch (kind)
    {
        case 0:
            p[pSemi] = r.pick ({ 0.f, -12.f, 12.f, 7.f }); p[pSub] = r.range (.2f, .6f); p[pMode] = (float) (1 + r.next() % 2);
            p[pGlide] = r.pick ({ 0.f, .04f, .08f }); p[pAA] = r.range (.002f, .01f); p[pAD] = r.range (.2f, .8f); p[pAS] = r.range (.5f, 1);
            p[pAR] = r.range (.05f, .25f); p[pCutoff] = expRange (150, 1500); p[pFD] = r.range (.1f, .5f); p[pFS] = r.range (0, .4f);
            p[pDrive] = r.range (.1f, .6f); p[pRMix] = r.range (0, .15f); p[pDMix] = r.range (0, .15f); p[pFilterType] = 0;
            break;
        case 1:
            p[pMode] = r.uni() < .5f ? 0.f : 2.f; p[pGlide] = r.range (0, .1f); p[pAA] = r.range (.002f, .03f); p[pAD] = r.range (.2f, 1);
            p[pAS] = r.range (.6f, 1); p[pAR] = r.range (.1f, .5f); p[pDMix] = r.range (.15f, .4f); p[pLfoPitch] = r.range (0, .25f);
            break;
        case 2:
            p[pAA] = r.range (.001f, .005f); p[pAD] = r.range (.2f, .9f); p[pAS] = r.range (0, .15f); p[pAR] = r.range (.2f, .7f);
            p[pFA] = .001f; p[pFD] = r.range (.08f, .4f); p[pFS] = r.range (0, .2f); p[pFenv] = r.range (.3f, .8f); p[pDMix] = r.range (.2f, .45f);
            break;
        default:
            p[pAA] = r.range (.4f, 1.6f); p[pAD] = r.range (.8f, 2); p[pAS] = r.range (.6f, .9f); p[pAR] = r.range (1.2f, 3.5f);
            p[pFA] = r.range (.3f, 1.5f); p[pFD] = r.range (.8f, 2); p[pChorus] = r.range (.4f, .8f); p[pRMix] = r.range (.4f, .7f);
            p[pRSize] = r.range (3, 5.5f); p[pLfoFilter] = r.range (0, .4f); p[pLfoRate] = r.range (.1f, 1);
            break;
    }
    for (int id = 0; id < kNumParams; ++id) p[id] = snap (id, p[id]);
    static const char* kinds[] = { "Bass", "Lead", "Pluck", "Pad" };
    if (kindName) *kindName = kinds[kind];
    return p;
}

} // namespace keim
