#include "SynthEngine.h"
#include <cmath>

juce::StringArray instrumentNames()
{
    return { "Suitcase Electric Piano", "Concert Grand", "Jazz Organ", "Vibraphone", "Velvet Pad" };
}

//==============================================================================
namespace
{
struct SineTable
{
    static constexpr int size = 4096;
    float t[size + 1];
    SineTable() { for (int i = 0; i <= size; ++i) t[i] = (float) std::sin (juce::MathConstants<double>::twoPi * i / size); }
    inline float operator() (double cycles) const noexcept
    {
        cycles -= std::floor (cycles);
        const double x = cycles * size;
        const int i = (int) x;
        const float f = (float) (x - i);
        return t[i] + f * (t[i + 1] - t[i]);
    }
};
const SineTable sineTable;

inline float softClip (float x) noexcept { return x / (1.0f + std::abs (x) * 0.6f); }

inline float polyBlep (double t, double dt) noexcept
{
    if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}
}

//==============================================================================
void JazzVoice::setupAdditive (const double* ratios, const float* amps, const float* tauFast, const float* tauSlow, float fastMix, int n)
{
    numPartials = 0;
    for (int i = 0; i < n && numPartials < maxPartials; ++i)
    {
        const double f = freq * ratios[i];
        if (f >= sr * 0.45 || amps[i] <= 0.0f) continue;
        Partial& p = partials[numPartials++];
        const double w = juce::MathConstants<double>::twoPi * f / sr;
        p.cw = std::cos (w); p.sw = std::sin (w);
        const double ph = rng.nextDouble() * juce::MathConstants<double>::twoPi * 0.05;   // tiny phase scatter
        p.c = std::cos (ph); p.s = std::sin (ph);
        p.aFast = amps[i] * fastMix;
        p.aSlow = amps[i] * (1.0f - fastMix);
        p.dFast = tauFast[i] > 0 ? (float) std::exp (-1.0 / (tauFast[i] * sr)) : 1.0f;
        p.dSlow = tauSlow[i] > 0 ? (float) std::exp (-1.0 / (tauSlow[i] * sr)) : 1.0f;
    }
}

void JazzVoice::startNote (int midiNote, float velocity, juce::SynthesiserSound*, int)
{
    sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
    freq = juce::MidiMessage::getMidiNoteInHertz (midiNote);
    vel = juce::jlimit (0.05f, 1.0f, velocity);
    tone = shared.tone.load();
    inst = (Instrument) juce::jlimit (0, (int) Instrument::Num - 1, shared.instrument.load());

    const float p = juce::jlimit (-1.0f, 1.0f, (float) (midiNote - 62) / 40.0f) * 0.55f;
    panL = std::cos ((p + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
    panR = std::sin ((p + 1.0f) * juce::MathConstants<float>::pi * 0.25f);

    releasing = false;
    env = 0.0f;
    numPartials = 0;
    noiseAmp = 0.0f; noiseLp = 0.0f;
    phC = phM = phBell = 0.0;
    svfLow = svfBand = 0.0f;
    ampDecay = 1.0f;

    const float rel = 0.05f + std::pow (shared.release.load(), 2.0f) * 3.0f;
    float attack = 0.002f;
    float relTime = rel;
    const double lowRef = 261.63 / freq;

    switch (inst)
    {
        case Instrument::ElectricPiano:
        {
            incC = freq / sr; incM = freq / sr;
            const double bellRatio = freq * 7.0 < sr * 0.4 ? 7.0 : 3.0;
            incBell = freq * bellRatio / sr;
            fmIndex = (0.7f + 2.6f * vel) * (0.45f + tone * 0.9f);
            fmFloor = (0.15f + 0.35f * vel) * (0.4f + tone);
            fmDecay = (float) std::exp (-1.0 / (0.30 * sr));
            bellAmp = 0.10f * vel * vel * (0.4f + tone);
            bellDecay = (float) std::exp (-1.0 / (0.035 * sr));
            const double tau = 2.4 * std::pow (lowRef, 0.45) + 0.5;
            ampDecay = (float) std::exp (-1.0 / (tau * sr));
            noiseAmp = 0.0f;
            attack = 0.0015f;
            break;
        }
        case Instrument::Grand:
        {
            double ratios[maxPartials]; float amps[maxPartials], tf[maxPartials], ts[maxPartials];
            const double B = 0.00008 + 0.0004 * juce::jlimit (0.0, 1.0, (midiNote - 36) / 60.0);
            const float bright = 0.25f + 0.55f * vel + 0.35f * tone;
            const float tau1 = (float) juce::jlimit (0.6, 14.0, 7.5 * std::pow (lowRef, 0.7));
            int n = 0;
            for (int h = 1; h <= 12; ++h)
            {
                const double r = h * std::sqrt (1.0 + B * h * h);
                const float a = (1.0f / std::pow ((float) h, 1.15f)) * std::exp (-(h - 1) * (1.15f - bright) * 0.45f) * 0.5f;
                const float slow = tau1 / (1.0f + 0.32f * (h - 1));
                for (int s = 0; s < 2; ++s)
                {
                    ratios[n] = r * (s == 0 ? 0.99962 : 1.00038);
                    amps[n] = a; tf[n] = slow * 0.11f; ts[n] = slow; ++n;
                }
            }
            setupAdditive (ratios, amps, tf, ts, 0.62f, n);
            noiseAmp = 0.10f * vel * (0.5f + tone);
            noiseDecay = (float) std::exp (-1.0 / (0.007 * sr));
            noiseCoef = 0.15f + 0.6f * vel;
            attack = 0.0008f;
            relTime = juce::jmin (rel, 0.08f + rel * 0.6f);
            break;
        }
        case Instrument::Organ:
        {
            const double ratios[] = { 0.5, 1.5, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0, 3.0 };
            const float t = tone;
            float amps[] = { 0.9f, 0.85f, 1.0f, 0.55f * t, 0.35f * t, 0.25f * t, 0.0f, 0.0f, 0.22f * t, 0.55f };
            float tf[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.22f };
            float ts[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.22f };
            for (auto& a : amps) a *= 0.33f;
            setupAdditive (ratios, amps, tf, ts, 0.0f, 10);
            noiseAmp = 0.06f; noiseDecay = (float) std::exp (-1.0 / (0.003 * sr)); noiseCoef = 0.7f;
            attack = 0.004f;
            relTime = 0.03f + rel * 0.25f;
            break;
        }
        case Instrument::Vibes:
        {
            const double ratios[] = { 1.0, 3.98, 9.95 };
            const float tau1 = (float) juce::jlimit (1.5, 6.0, 4.5 * std::pow (lowRef, 0.35));
            float amps[] = { 1.0f, 0.4f * (0.35f + tone), 0.18f * (0.25f + tone) };
            float ts[] = { tau1, 0.9f, 0.28f };
            float tf[] = { tau1 * 0.15f, 0.15f, 0.06f };
            for (auto& a : amps) a *= 0.55f;
            setupAdditive (ratios, amps, tf, ts, 0.2f, 3);
            noiseAmp = 0.05f * vel; noiseDecay = (float) std::exp (-1.0 / (0.002 * sr)); noiseCoef = 0.5f;
            attack = 0.001f;
            break;
        }
        case Instrument::Pad:
        {
            const double cents[3] = { -8.0, 0.0, 7.0 };
            for (int i = 0; i < 3; ++i) { padInc[i] = freq * std::pow (2.0, cents[i] / 1200.0) / sr; padPh[i] = rng.nextDouble(); }
            const double fc = juce::jmin (sr * 0.4, 280.0 + 4800.0 * std::pow (tone, 1.5) + 900.0 * vel);
            padCut = (float) (2.0 * std::sin (juce::MathConstants<double>::pi * fc / sr));
            attack = 0.35f;
            relTime = rel * 2.0f + 0.2f;
            break;
        }
        default: break;
    }

    attackInc = 1.0f / (float) juce::jmax (1.0, attack * sr);
    releaseMul = (float) std::exp (-1.0 / (juce::jmax (0.01f, relTime) * 0.35 * sr));
    level = 1.0f;
}

void JazzVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff) releasing = true;
    else { clearCurrentNote(); env = 0.0f; }
}

float JazzVoice::renderSample()
{
    float out = 0.0f;
    switch (inst)
    {
        case Instrument::ElectricPiano:
        {
            const float mod = sineTable (phM);
            const float car = sineTable (phC + (fmIndex * mod) / juce::MathConstants<float>::twoPi);
            const float bell = sineTable (phBell) * bellAmp;
            phC += incC; phM += incM; phBell += incBell;
            if (phC > 1.0) phC -= 1.0; if (phM > 1.0) phM -= 1.0; if (phBell > 1.0) phBell -= 1.0;
            fmIndex = fmFloor + (fmIndex - fmFloor) * fmDecay;
            bellAmp *= bellDecay;
            level *= ampDecay;
            out = softClip ((car * (0.35f + 0.65f * vel) + bell) * (1.0f + vel * 0.8f)) * level * 0.24f;
            break;
        }
        case Instrument::Grand: case Instrument::Organ: case Instrument::Vibes:
        {
            for (int i = 0; i < numPartials; ++i)
            {
                Partial& p = partials[i];
                out += (float) p.s * (p.aFast + p.aSlow);
                const double c = p.c * p.cw - p.s * p.sw;
                p.s = p.s * p.cw + p.c * p.sw;
                p.c = c;
                p.aFast *= p.dFast; p.aSlow *= p.dSlow;
            }
            const float gain = inst == Instrument::Grand ? 0.22f : (inst == Instrument::Organ ? 0.22f : 0.30f);
            out *= gain * (inst == Instrument::Organ ? 1.0f : (0.3f + 0.7f * vel));
            break;
        }
        case Instrument::Pad:
        {
            float saw = 0.0f;
            for (int i = 0; i < 3; ++i)
            {
                double& ph = padPh[i];
                saw += (float) (2.0 * ph - 1.0) - polyBlep (ph, padInc[i]);
                ph += padInc[i]; if (ph >= 1.0) ph -= 1.0;
            }
            saw *= 0.33f;
            // 2-pole state variable low-pass
            svfLow += padCut * svfBand;
            const float high = saw - svfLow - 0.7f * svfBand;
            svfBand += padCut * high;
            out = svfLow * 0.22f * (0.5f + 0.5f * vel);
            break;
        }
        default: break;
    }

    if (noiseAmp > 1.0e-5f)
    {
        const float n = rng.nextFloat() * 2.0f - 1.0f;
        noiseLp += noiseCoef * (n - noiseLp);
        out += noiseLp * noiseAmp;
        noiseAmp *= noiseDecay;
    }
    return out;
}

void JazzVoice::renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (! isVoiceActive()) return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        if (releasing) env *= releaseMul;
        else if (env < 1.0f) env = juce::jmin (1.0f, env + attackInc);

        const float s = renderSample() * env;
        L[startSample + i] += s * panL;
        if (R) R[startSample + i] += s * panR;
    }

    // keep the recursive oscillators on the unit circle
    for (int i = 0; i < numPartials; ++i)
    {
        Partial& p = partials[i];
        const double r = std::sqrt (p.c * p.c + p.s * p.s);
        if (r > 0) { p.c /= r; p.s /= r; }
    }

    // stop silent voices
    float amp = 0.0f;
    if (inst == Instrument::ElectricPiano) amp = level;
    else if (inst == Instrument::Pad) amp = 1.0f;
    else for (int i = 0; i < numPartials; ++i) amp += partials[i].aFast + partials[i].aSlow;

    if ((releasing && env < 1.0e-4f) || amp * env < 1.0e-5f)
    {
        clearCurrentNote();
        env = 0.0f;
    }
}

//==============================================================================
JazzSynth::JazzSynth()
{
    for (int i = 0; i < 24; ++i) addVoice (new JazzVoice (shared));
    addSound (new JazzSound());
    setNoteStealingEnabled (true);
}
