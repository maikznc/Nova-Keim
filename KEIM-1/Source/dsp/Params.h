// KEIM-1 Parametertabelle
// Reines C++17 ohne JUCE: dieselbe Tabelle nutzt später auch die Daisy-Firmware.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>

namespace keim {

enum ParamId : int {
    pOsc1Wave, pOsc2Wave, pMix, pSemi, pDetune, pSub, pNoise, pSpread,
    pFilterType, pCutoff, pRes, pFenv, pKtrack, pFA, pFD, pFS, pFR,
    pAA, pAD, pAS, pAR, pMode, pGlide, pLevel,
    pLfoRate, pLfoFilter, pLfoPitch,
    pDrive, pChorus, pRMix, pRSize, pDDiv, pDMix, pDFb,
    pArpOn, pHold, pArpMode, pArpRate, pArpOct, pBpm, pArpGate, pArpMut, pSync,
    kNumParams
};

enum class Kind { Float, Choice, Bool };
enum class Curve { Lin, Exp, Pow };
enum class Unit { None, Percent, Bipolar, Hz, LfoHz, Seconds, Glide, Cents, Semis, Bpm, Feedback };

struct ParamSpec {
    const char* id;    // feste ID für Presets, DAW-Automation und gespeicherte Songs
    const char* name;  // Anzeigename (UTF-8)
    Kind kind;
    float min, max, def;
    Curve curve;
    float step;        // 0 = stufenlos
    Unit unit;
    bool morph;        // Teil eines Sounds (wird beim Morphen überblendet)
    bool bipolar;      // Regler-Bogen startet in der Mitte
    const char* const* choices;
    int numChoices;
};

// Auswahllisten
inline const char* const kWaveNames[]   = { "Säge", "Rechteck", "Dreieck", "Sinus" };
inline const char* const kFilterNames[] = { "Tiefpass", "Hochpass", "Bandpass" };
inline const char* const kModeNames[]   = { "Poly", "Mono", "Legato" };
inline const char* const kDivNames[]    = { "1/16", "1/8", "1/8.", "1/4" };
inline const char* const kArpModeNames[]= { "Hoch", "Runter", "Auf-Ab", "Zufall" };
inline const char* const kArpRateNames[]= { "1/8", "1/16", "1/8 T", "1/32" };
inline const char* const kArpOctNames[] = { "1 Okt", "2 Okt", "3 Okt" };

namespace detail {
inline ParamSpec F (const char* id, const char* name, float mn, float mx, float df, Curve c, Unit u,
                    bool morph = true, float step = 0.0f, bool bip = false)
{
    return { id, name, Kind::Float, mn, mx, df, c, step, u, morph, bip, nullptr, 0 };
}
template <std::size_t N>
inline ParamSpec C (const char* id, const char* name, const char* const (&names)[N], int df, bool morph = true)
{
    return { id, name, Kind::Choice, 0.0f, float (N - 1), float (df), Curve::Lin, 1.0f, Unit::None, morph, false, names, (int) N };
}
inline ParamSpec B (const char* id, const char* name, bool df)
{
    return { id, name, Kind::Bool, 0.0f, 1.0f, df ? 1.0f : 0.0f, Curve::Lin, 1.0f, Unit::None, false, false, nullptr, 0 };
}
} // namespace detail

inline const ParamSpec& spec (int id)
{
    using namespace detail;
    static const ParamSpec table[kNumParams] = {
        C ("osc1Wave",   "Osz 1 Welle",  kWaveNames, 0),
        C ("osc2Wave",   "Osz 2 Welle",  kWaveNames, 0),
        F ("mix",        "Mix 1↔2",      0.0f, 1.0f, 0.5f,   Curve::Lin, Unit::Percent),
        F ("semi",       "Osz 2 Ton",    -24.f, 24.f, 0.0f,  Curve::Lin, Unit::Semis, true, 1.0f),
        F ("detune",     "Detune",       0.0f, 50.f, 8.0f,   Curve::Lin, Unit::Cents),
        F ("sub",        "Sub",          0.0f, 1.0f, 0.0f,   Curve::Lin, Unit::Percent),
        F ("noise",      "Rauschen",     0.0f, 1.0f, 0.0f,   Curve::Lin, Unit::Percent),
        F ("spread",     "Breite",       0.0f, 1.0f, 0.3f,   Curve::Lin, Unit::Percent),
        C ("filterType", "Filtertyp",    kFilterNames, 0),
        F ("cutoff",     "Cutoff",       30.f, 16000.f, 2400.f, Curve::Exp, Unit::Hz),
        F ("res",        "Resonanz",     0.0f, 1.0f, 0.2f,   Curve::Lin, Unit::Percent),
        F ("fenv",       "Env-Menge",    -1.f, 1.0f, 0.3f,   Curve::Lin, Unit::Bipolar, true, 0.0f, true),
        F ("ktrack",     "Keytrack",     0.0f, 1.0f, 0.5f,   Curve::Lin, Unit::Percent),
        F ("fA",         "F-Attack",     0.001f, 5.f, 0.005f, Curve::Exp, Unit::Seconds),
        F ("fD",         "F-Decay",      0.01f, 5.f, 0.3f,   Curve::Exp, Unit::Seconds),
        F ("fS",         "F-Sustain",    0.0f, 1.0f, 0.3f,   Curve::Lin, Unit::Percent),
        F ("fR",         "F-Release",    0.01f, 8.f, 0.4f,   Curve::Exp, Unit::Seconds),
        F ("aA",         "Attack",       0.001f, 5.f, 0.005f, Curve::Exp, Unit::Seconds),
        F ("aD",         "Decay",        0.01f, 5.f, 0.4f,   Curve::Exp, Unit::Seconds),
        F ("aS",         "Sustain",      0.0f, 1.0f, 0.7f,   Curve::Lin, Unit::Percent),
        F ("aR",         "Release",      0.01f, 8.f, 0.3f,   Curve::Exp, Unit::Seconds),
        C ("mode",       "Spielweise",   kModeNames, 0),
        F ("glide",      "Glide",        0.0f, 1.0f, 0.0f,   Curve::Pow, Unit::Glide),
        F ("level",      "Lautstärke",   0.0f, 1.0f, 0.75f,  Curve::Lin, Unit::Percent, false),
        F ("lfoRate",    "LFO Rate",     0.05f, 20.f, 4.0f,  Curve::Exp, Unit::LfoHz),
        F ("lfoFilter",  "LFO → Filter", 0.0f, 1.0f, 0.0f,   Curve::Lin, Unit::Percent),
        F ("lfoPitch",   "LFO → Ton",    0.0f, 1.0f, 0.0f,   Curve::Lin, Unit::Percent),
        F ("drive",      "Drive",        0.0f, 1.0f, 0.0f,   Curve::Lin, Unit::Percent),
        F ("chorus",     "Chorus",       0.0f, 1.0f, 0.0f,   Curve::Lin, Unit::Percent),
        F ("rMix",       "Hall",         0.0f, 1.0f, 0.2f,   Curve::Lin, Unit::Percent),
        F ("rSize",      "Hallgröße",    0.3f, 6.f, 2.2f,    Curve::Exp, Unit::Seconds),
        C ("dDiv",       "Delay-Zeit",   kDivNames, 2),
        F ("dMix",       "Delay",        0.0f, 1.0f, 0.15f,  Curve::Lin, Unit::Percent),
        F ("dFb",        "Feedback",     0.0f, 0.9f, 0.35f,  Curve::Lin, Unit::Feedback),
        B ("arpOn",      "Arp an",       false),
        B ("hold",       "Halten",       false),
        C ("arpMode",    "Arp-Richtung", kArpModeNames, 0, false),
        C ("arpRate",    "Arp-Raster",   kArpRateNames, 1, false),
        C ("arpOct",     "Arp-Oktaven",  kArpOctNames, 0, false),
        F ("bpm",        "Tempo",        60.f, 180.f, 120.f, Curve::Lin, Unit::Bpm, false, 1.0f),
        F ("arpGate",    "Gate",         0.05f, 1.0f, 0.55f, Curve::Lin, Unit::Percent, false),
        F ("arpMut",     "Mutation",     0.0f, 1.0f, 0.0f,   Curve::Lin, Unit::Percent, false),
        B ("sync",       "Host-Tempo",   true),
    };
    return table[(size_t) std::clamp (id, 0, kNumParams - 1)];
}

inline float clamp01 (float x) { return std::min (1.0f, std::max (0.0f, x)); }

// Wert -> Reglerstellung 0..1 (gleiche Kurven wie im Browser-Prototyp)
inline float toNorm (int id, float v)
{
    const auto& s = spec (id);
    if (s.kind != Kind::Float)
        return s.max > s.min ? clamp01 ((v - s.min) / (s.max - s.min)) : 0.0f;
    if (s.curve == Curve::Exp)
        return clamp01 (std::log (std::max (v, s.min) / s.min) / std::log (s.max / s.min));
    const float l = (v - s.min) / (s.max - s.min);
    return clamp01 (s.curve == Curve::Pow ? std::sqrt (std::max (0.0f, l)) : l);
}

inline float snap (int id, float v)
{
    const auto& s = spec (id);
    if (s.step > 0.0f)
        v = std::round (v / s.step) * s.step;
    return std::min (s.max, std::max (s.min, v));
}

// Reglerstellung 0..1 -> Wert
inline float fromNorm (int id, float n)
{
    const auto& s = spec (id);
    n = clamp01 (n);
    float v;
    if (s.curve == Curve::Exp)       v = s.min * std::pow (s.max / s.min, n);
    else if (s.curve == Curve::Pow)  v = s.min + (s.max - s.min) * n * n;
    else                             v = s.min + (s.max - s.min) * n;
    return snap (id, v);
}

inline std::string fmtNum (float x, int decimals)
{
    char buf[32];
    std::snprintf (buf, sizeof (buf), "%.*f", decimals, (double) x);
    for (char* c = buf; *c; ++c) if (*c == '.') *c = ',';   // deutsches Komma
    return buf;
}

inline std::string formatSeconds (float v)
{
    if (v < 1.0f) return std::to_string ((int) std::lround (v * 1000.0f)) + " ms";
    return fmtNum (v, v < 10.0f ? 2 : 1) + " s";
}

// Anzeige eines Werts, z. B. "2,4 kHz", "+7 HT", "350 ms"
inline std::string formatValue (int id, float v)
{
    const auto& s = spec (id);
    if (s.kind == Kind::Bool)   return v >= 0.5f ? "an" : "aus";
    if (s.kind == Kind::Choice) return s.choices[std::clamp ((int) std::lround (v), 0, s.numChoices - 1)];

    switch (s.unit)
    {
        case Unit::Percent:  return std::to_string ((int) std::lround (v * 100.0f)) + " %";
        case Unit::Feedback: return std::to_string ((int) std::lround (v / 0.9f * 100.0f)) + " %";
        case Unit::Bipolar: {
            const int p = (int) std::lround (std::fabs (v) * 100.0f);
            return std::string (p == 0 ? "" : (v > 0 ? "+" : "−")) + std::to_string (p) + " %";
        }
        case Unit::Hz:
            if (v < 1000.0f) return std::to_string ((int) std::lround (v)) + " Hz";
            return fmtNum (v / 1000.0f, v < 10000.0f ? 1 : 0) + " kHz";
        case Unit::LfoHz:   return fmtNum (v, v < 1.0f ? 2 : 1) + " Hz";
        case Unit::Seconds: return formatSeconds (v);
        case Unit::Glide:   return v < 0.002f ? "aus" : formatSeconds (v);
        case Unit::Cents:   return std::to_string ((int) std::lround (v)) + " ct";
        case Unit::Semis: {
            const int st = (int) std::lround (v);
            return std::string (st > 0 ? "+" : (st < 0 ? "−" : "")) + std::to_string (std::abs (st)) + " HT";
        }
        case Unit::Bpm:     return std::to_string ((int) std::lround (v)) + " BPM";
        case Unit::None:    break;
    }
    return fmtNum (v, 2);
}

// Text -> Wert (für Werteingabe im Host); akzeptiert "2,4 kHz", "350 ms", "45 %" ...
inline float parseValue (int id, const std::string& text)
{
    const auto& s = spec (id);
    std::string t = text;
    for (auto& c : t) if (c == ',') c = '.';
    if (s.kind == Kind::Choice)
    {
        for (int i = 0; i < s.numChoices; ++i)
            if (text == s.choices[i]) return (float) i;
    }
    float v = (float) std::atof (t.c_str());
    if (s.kind != Kind::Float) return snap (id, v);
    const bool hasK = t.find ('k') != std::string::npos;
    const bool hasMs = t.find ("ms") != std::string::npos;
    switch (s.unit)
    {
        case Unit::Percent: case Unit::Bipolar: v /= 100.0f; break;
        case Unit::Feedback: v = v / 100.0f * 0.9f; break;
        case Unit::Hz: if (hasK) v *= 1000.0f; break;
        case Unit::Seconds: case Unit::Glide: if (hasMs) v /= 1000.0f; break;
        case Unit::None: case Unit::LfoHz: case Unit::Cents: case Unit::Semis: case Unit::Bpm: break;
    }
    return snap (id, v);
}

inline bool isMorphParam (int id) { const auto& s = spec (id); return s.morph && s.kind != Kind::Bool; }

} // namespace keim
