// Rendert alle Werkssounds als WAV und prüft Pegel, Spitzen und ungültige Werte.
// Läuft ohne JUCE:   ./keim_render [ausgabeordner]
#include "../Source/dsp/Engine.h"
#include "../Source/dsp/Presets.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
 #include <xmmintrin.h>
#endif

using namespace keim;

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 2 * 4;
    auto u32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    u32 (16); u16 (3); u16 (2); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 8); u16 (8); u16 (32);
    std::fwrite ("data", 1, 4, f); u32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i) { std::fwrite (&L[i], 4, 1, f); std::fwrite (&R[i], 4, 1, f); }
    std::fclose (f);
}

struct Stats { double rms = 0; float peak = 0; bool bad = false; };
static Stats measure (const std::vector<float>& L, const std::vector<float>& R, size_t a, size_t b)
{
    Stats s; double acc = 0;
    b = std::min (b, L.size());
    for (size_t i = a; i < b; ++i)
    {
        if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) s.bad = true;
        acc += 0.5 * ((double) L[i] * L[i] + (double) R[i] * R[i]);
        s.peak = std::max (s.peak, std::max (std::fabs (L[i]), std::fabs (R[i])));
    }
    s.rms = b > a ? std::sqrt (acc / (double) (b - a)) : 0;
    return s;
}

struct Ev { double t; int type; int note; float val; }; // 0 an, 1 aus, 2 Parameter

static void render (Engine& e, const std::vector<Ev>& evs, double seconds, int sr, std::vector<float>& L, std::vector<float>& R)
{
    const size_t total = (size_t) (seconds * sr);
    L.assign (total, 0); R.assign (total, 0);
    size_t pos = 0, ei = 0;
    while (pos < total)
    {
        size_t next = std::min (total, pos + 256);
        if (ei < evs.size()) next = std::min (next, (size_t) (evs[ei].t * sr));
        if (next > pos) { e.process (&L[pos], &R[pos], (int) (next - pos)); pos = next; }
        while (ei < evs.size() && (size_t) (evs[ei].t * sr) <= pos)
        {
            const auto& ev = evs[ei++];
            if (ev.type == 0) e.noteOn (ev.note, ev.val);
            else if (ev.type == 1) e.noteOff (ev.note);
            else e.setParam (ev.note, ev.val);
        }
    }
}

static void applyPatch (Engine& e, const Patch& p) { for (int i = 0; i < kNumParams; ++i) e.setParam (i, p[i]); }

int main (int argc, char** argv)
{
#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
    _mm_setcsr (_mm_getcsr() | 0x8040);  // Denormals aus
#endif
    const std::string out = argc > 1 ? argv[1] : ".";
    const int sr = 48000;
    int failures = 0;

    // Hall-Kalibrierung: Verstärkung für weißes Rauschen
    {
        Reverb rv; rv.prepare ((float) sr); rv.setDecay (2.2f * 0.8f);
        uint32_t s = 1; double inAcc = 0, outAcc = 0;
        for (int i = 0; i < sr * 4; ++i)
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            const float x = (float) (int32_t) s / 2147483648.0f * 0.1f;
            float l, r; rv.tick (x, l, r);
            if (i > sr) { inAcc += x * x; outAcc += 0.5 * (l * l + r * r); }
        }
        std::printf ("Hall: Rausch-Verstaerkung %.3f (Browser ca. 0.37)\n\n", std::sqrt (outAcc / inAcc));
    }

    std::printf ("%-15s %-17s %-17s %-10s\n", "Sound", "Einzelton rms/pk", "Akkord rms/pk", "Ausklang");
    for (int p = 0; p < numFactoryPresets(); ++p)
    {
        Engine e; e.prepare (sr);
        applyPatch (e, factoryPatch (p));
        e.reset();
        const bool pad = p == 5;
        const double tChord = pad ? 1.4 : 0.6;
        std::vector<Ev> ev = { { 0.0, 0, 48, 0.85f }, { tChord, 0, 52, 0.85f }, { tChord, 0, 55, 0.85f }, { tChord, 0, 60, 0.85f },
                               { tChord + 0.6, 1, 48, 0 }, { tChord + 0.6, 1, 52, 0 }, { tChord + 0.6, 1, 55, 0 }, { tChord + 0.6, 1, 60, 0 } };
        std::vector<float> L, R;
        const double len = tChord + 0.6 + 6.0;
        render (e, ev, len, sr, L, R);
        const auto one = measure (L, R, (size_t) ((pad ? 1.1 : 0.12) * sr), (size_t) ((pad ? 1.35 : 0.35) * sr));
        const auto chord = measure (L, R, (size_t) ((tChord + 0.15) * sr), (size_t) ((tChord + 0.55) * sr));
        const auto tail = measure (L, R, L.size() - (size_t) (0.3 * sr), L.size());
        const auto all = measure (L, R, 0, L.size());
        const char* name = factoryPresets()[(size_t) p].name;
        std::printf ("%-15s %.3f / %.3f    %.3f / %.3f    %.5f %s\n", name, one.rms, one.peak, chord.rms, chord.peak, tail.rms,
                     all.bad ? "UNGUELTIG" : "");
        if (all.bad || one.rms < 0.02 || all.peak > 1.01f || tail.rms > 0.01) ++failures;
        std::string fn = out + "/" + name + ".wav";
        for (auto& c : fn) if (c == ' ') c = '_';
        writeWav (fn, L, R, sr);
    }

    // Arpeggiator mit Mutation
    {
        Engine e; e.prepare (sr); applyPatch (e, factoryPatch (6)); e.reset();
        e.setParam (pArpOn, 1); e.setParam (pArpMut, 0.6f); e.setParam (pSync, 0); e.setParam (pBpm, 120);
        std::vector<Ev> ev = { { 0.05, 0, 60, 0.9f }, { 0.05, 0, 63, 0.9f }, { 0.05, 0, 67, 0.9f }, { 3.0, 1, 60, 0 }, { 3.0, 1, 63, 0 }, { 3.0, 1, 67, 0 } };
        // Schritte zählen: in 128-Sample-Häppchen rendern und Schrittwechsel beobachten
        std::vector<float> L ((size_t) (5.0 * sr)), R (L.size());
        int steps = 0, last = -1; size_t pos = 0; size_t ei = 0;
        while (pos < L.size())
        {
            while (ei < ev.size() && (size_t) (ev[ei].t * sr) <= pos) { if (ev[ei].type == 0) e.noteOn (ev[ei].note, ev[ei].val); else e.noteOff (ev[ei].note); ++ei; }
            const int n = (int) std::min<size_t> (128, L.size() - pos);
            e.process (&L[pos], &R[pos], n); pos += (size_t) n;
            const int st = e.arpStep();
            if (st != last && st >= 0 && pos < (size_t) (3.0 * sr)) ++steps;
            last = st;
        }
        const auto st = measure (L, R, 0, L.size());
        std::printf ("\nArp (1/16 bei 120 BPM): %d Schritte in 2,95 s (erwartet 24), Spitze %.3f, Stimmen am Ende %d %s\n", steps, st.peak, e.activeVoices(), st.bad ? "UNGUELTIG" : "");
        if (st.bad || steps < 23 || steps > 25) ++failures;
        writeWav (out + "/Arp_Glass_Pluck.wav", L, R, sr);
    }

    // Glide im Legato-Modus
    {
        Engine e; e.prepare (sr); applyPatch (e, factoryPatch (0)); e.reset();
        std::vector<Ev> ev = { { 0.0, 0, 36, 0.9f }, { 0.25, 0, 48, 0.9f }, { 0.4, 1, 36, 0 }, { 0.6, 0, 43, 0.9f }, { 0.7, 1, 48, 0 }, { 1.0, 1, 43, 0 } };
        std::vector<float> L, R; render (e, ev, 2.0, sr, L, R);
        const auto st = measure (L, R, 0, L.size());
        std::printf ("Acid-Glide: Spitze %.3f, Stimmen danach %d %s\n", st.peak, e.activeVoices(), st.bad ? "UNGUELTIG" : "");
        if (st.bad) ++failures;
        writeWav (out + "/Glide_Acid.wav", L, R, sr);
    }

    // Morph-Mitte aus vier Ecken
    {
        std::array<Patch, 4> c;
        const auto dc = defaultCorners();
        for (int i = 0; i < 4; ++i) c[(size_t) i] = factoryPatch (dc[(size_t) i]);
        const Patch m = morphPatches (c, 0.5f, 0.5f, defaultPatch());
        Engine e; e.prepare (sr); applyPatch (e, m); e.reset();
        std::vector<Ev> ev = { { 0.0, 0, 48, 0.85f }, { 0.0, 0, 55, 0.85f }, { 1.5, 1, 48, 0 }, { 1.5, 1, 55, 0 } };
        std::vector<float> L, R; render (e, ev, 4.0, sr, L, R);
        const auto st = measure (L, R, (size_t) (0.3 * sr), (size_t) (1.4 * sr));
        std::printf ("Morph-Mitte: rms %.3f, Cutoff %.0f Hz %s\n", st.rms, m[pCutoff], st.bad ? "UNGUELTIG" : "");
        writeWav (out + "/Morph_Mitte.wav", L, R, sr);
    }

    // Stimmenklau: 12 Töne gleichzeitig
    {
        Engine e; e.prepare (sr); applyPatch (e, factoryPatch (5)); e.reset();
        std::vector<Ev> ev; for (int i = 0; i < 12; ++i) ev.push_back ({ 0.01 * i, 0, 48 + i * 2, 0.8f });
        std::vector<float> L, R; render (e, ev, 2.0, sr, L, R);
        const auto st = measure (L, R, 0, L.size());
        std::printf ("12 Töne auf 8 Stimmen: aktiv %d, Spitze %.3f %s\n", e.activeVoices(), st.peak, st.bad ? "UNGUELTIG" : "");
        if (st.bad || e.activeVoices() > kPoly) ++failures;
    }

    std::printf ("\n%s\n", failures ? "FEHLER gefunden" : "Alles in Ordnung");
    return failures ? 1 : 0;
}
