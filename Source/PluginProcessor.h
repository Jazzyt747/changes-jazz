#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Theory.h"
#include "Palettes.h"
#include "SynthEngine.h"

namespace ids
{
    #define P(name) static const juce::String name { #name };
    P(key) P(minor) P(level) P(style) P(bass) P(ext) P(alt) P(vl) P(reharm) P(latch) P(lockPalette) P(suggest)
    P(registerShift) P(zone) P(melSnap) P(melMode) P(whiteMap) P(harmonize) P(melOctave) P(showScale)
    P(strum) P(strumDir) P(humanize) P(dynamics) P(lhInst) P(rhInst) P(tone) P(release) P(chorus) P(reverb) P(motion)
    P(chordVol) P(melVol) P(masterVol) P(internal) P(midiOut) P(bpm) P(beats) P(comp) P(swing)
    #undef P
}

/** Comping rhythms used by the progression player. */
juce::StringArray compNames();

class ChangesProcessor : public juce::AudioProcessor,
                         public juce::ChangeBroadcaster,
                         private juce::AudioProcessorValueTreeState::Listener,
                         private juce::AsyncUpdater,
                         private juce::Timer
{
public:
    ChangesProcessor();
    ~ChangesProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Changes"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;     // GUI / computer keyboard input

    theory::Options currentOptions() const;
    theory::MelodyMode melodyMode() const;
    int chordZoneStart() const;

    // ---- palette (message thread)
    theory::ChordSpec getPad (int i) const;
    void setPad (int i, const theory::ChordSpec&);
    void swapPads (int a, int b);
    void copyPad (int from, int to);
    void loadPreset (int index);
    int getPresetIndex() const { return presetIndex; }
    juce::String padDisplayName (int i) const;
    bool savePaletteToFile (const juce::File&) const;
    bool loadPaletteFromFile (const juce::File&);

    // ---- progression (message thread)
    const std::vector<theory::ChordSpec>& getProgression() const { return progression; }
    void progressionAdd (const theory::ChordSpec&, int insertAt = -1);
    void progressionRemove (int i);
    void progressionMove (int from, int to);
    void progressionSetBeats (int i, int beats);
    void progressionClear();
    juce::MidiFile renderProgressionMidi() const;
    juce::File writeProgressionMidiTemp() const;

    // ---- transport
    std::atomic<bool> recording { false };
    void setPlaying (bool);
    bool isPlaying() const { return playing.load(); }
    std::atomic<int> playStep { -1 };
    void panic() { panicRequested = true; }

    // ---- GUI feedback (written by audio thread)
    std::atomic<int> currentPad { -1 };
    std::atomic<uint32_t> currentSpec { 0 };
    std::atomic<int> chordSerial { 0 };
    std::atomic<uint32_t> soundingChord[4] {}, soundingMelody[4] {};
    std::atomic<float> meterL { 0 }, meterR { 0 }, rmsL { 0 }, rmsR { 0 }, meterChord { 0 }, meterMelody { 0 };
    std::atomic<float> hostBpm { 0 };
    bool isNoteSounding (const std::atomic<uint32_t>* bits, int note) const noexcept
    {
        return note >= 0 && note < 128 && (bits[note >> 5].load() & (1u << (note & 31))) != 0;
    }

    /** Called when an audition from the editor should be heard (pad click etc). */
    void auditionPad (int pad, bool on, float velocity = 0.8f);
    /** Audition an arbitrary chord (e.g. a progression chip). */
    void auditionSpec (const theory::ChordSpec& s, bool on)
    {
        if (on) { auditionValue.store (s.pack()); auditionOnReq = true; }
        else auditionOffReq = true;
    }

private:
    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String& id, float) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;
    void syncPadsToAudio();
    void syncProgressionToAudio();

    // ---- audio thread helpers
    struct Event { juce::int64 time; int note; float vel; bool on; int bus; };
    void schedule (juce::int64 time, int note, float vel, bool on, int bus);
    void dispatchEvents (int numSamples, juce::MidiBuffer& lhMidi, juce::MidiBuffer& rhMidi, juce::MidiBuffer& outMidi);
    void handleChordOn (const theory::ChordSpec& spec, int pad, float vel, int offset, bool fromSeq);
    void soundCurrentChord (int offset, float vel, double lengthSamples);
    void releaseChord (int offset);
    void handleMelodyOn (int note, float vel, int offset);
    void handleMelodyOff (int note, int offset);
    void runSequencer (int numSamples);
    void setSoundingBit (std::atomic<uint32_t>* bits, int note, bool on);
    float shapeVelocity (float v);

    JazzSynth lhSynth, rhSynth;
    juce::Reverb reverb;
    juce::dsp::Chorus<float> chorus;
    juce::AudioBuffer<float> lhBuf, rhBuf, mixBuf;
    juce::SmoothedValue<float> chordGain, melGain, masterGain;
    double sr = 44100.0;
    double lfoPhase = 0.0;
    juce::Random rng;

    // palette
    std::array<std::atomic<uint32_t>, palettes::numPads> padsAudio;
    palettes::Palette pads;
    int presetIndex = 0;
    int paletteLevel = 1, paletteMinor = 0;   // what the current palette was generated for
    std::atomic<bool> paletteRegenPending { false };

    // progression
    std::vector<theory::ChordSpec> progression;
    std::array<std::atomic<uint32_t>, 64> progAudio;
    std::atomic<int> progLen { 0 };

    // recorder
    juce::AbstractFifo recFifo { 256 };
    std::array<uint32_t, 256> recBuffer {};

    // scheduler
    std::array<Event, 1024> events {};
    int numEvents = 0;
    juce::int64 now = 0;
    int soundCount[3][128] {};

    // chord state
    theory::Voicing prevVoicing;
    theory::Voicing currentVoicing;
    theory::ResolvedChord currentChord;
    bool haveChord = false;
    int heldChordNotes[16] {}; int numHeldChordNotes = 0;
    juce::int64 heldOnTime[16] {};
    int activePad = -1;
    bool strumFlip = false;

    // melody state
    int melodyMap[128][6] {};
    int melodyMapCount[128] {};

    // sequencer
    std::atomic<bool> playing { false }, playRequested { false }, stopRequested { false };
    std::atomic<bool> panicRequested { false };
    std::atomic<uint32_t> auditionValue { 0 };
    std::atomic<bool> auditionOnReq { false }, auditionOffReq { false };
    double seqBeat = 0.0;
    int seqIndex = 0;
    double stepStartBeat = 0.0;
    struct Hit { double start, end; };
    Hit stepHits[8]; int numStepHits = 0; int nextHit = 0; bool hitSounding = false; int hitIndex = 0;
    double seqSamplePos = 0.0;
    void beginStep (int index, double startBeat, int sampleOffset);

    juce::CriticalSection progressionLock;   // message thread only; audio reads atomics

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChangesProcessor)
};

/** Comp-pattern hits within one chord of `beats` length (shared by player & MIDI export). */
int compHits (int pattern, int beats, float swing, double* starts, double* ends, int maxHits);
