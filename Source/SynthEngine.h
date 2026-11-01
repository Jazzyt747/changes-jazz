#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

/** Built-in instruments. All sounds are synthesized in real time (no samples). */
enum class Instrument : int { ElectricPiano = 0, Grand, Organ, Vibes, Pad, Num };

juce::StringArray instrumentNames();

/** Parameters shared by every voice of one synth. Written by the processor once per block. */
struct SynthShared
{
    std::atomic<int> instrument { 0 };
    std::atomic<float> tone { 0.5f };      // 0..1 brightness
    std::atomic<float> release { 0.35f };  // 0..1 -> release time
};

class JazzVoice : public juce::SynthesiserVoice
{
public:
    explicit JazzVoice (SynthShared& s) : shared (s) {}

    bool canPlaySound (juce::SynthesiserSound*) override { return true; }
    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    SynthShared& shared;

    static constexpr int maxPartials = 24;
    struct Partial { double c = 1, s = 0, cw = 1, sw = 0; float aFast = 0, aSlow = 0, dFast = 1, dSlow = 1; };

    Instrument inst = Instrument::ElectricPiano;
    Partial partials[maxPartials];
    int numPartials = 0;

    double freq = 440, sr = 44100;
    float vel = 0.8f, panL = 0.7f, panR = 0.7f;
    float env = 0, envTarget = 1, attackInc = 0, releaseMul = 0.999f;
    bool releasing = false;
    float tone = 0.5f;
    float level = 1.0f;

    // FM / organ / pad state
    double phC = 0, phM = 0, phBell = 0, incC = 0, incM = 0, incBell = 0;
    float fmIndex = 0, fmDecay = 0.9999f, fmFloor = 0, bellAmp = 0, bellDecay = 0.99f, ampDecay = 0.99999f;
    float noiseAmp = 0, noiseDecay = 0.99f, noiseLp = 0, noiseCoef = 0.5f;
    double padPh[3] {}; double padInc[3] {}; float svfLow = 0, svfBand = 0, padCut = 0.1f;
    juce::Random rng;

    void setupAdditive (const double* ratios, const float* amps, const float* tauFast, const float* tauSlow, float fastMix, int n);
    float renderSample();
};

class JazzSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class JazzSynth : public juce::Synthesiser
{
public:
    JazzSynth();
    SynthShared shared;
};
