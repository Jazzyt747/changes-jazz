#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace theory;

juce::StringArray compNames()
{
    return { "Sustain (whole)", "Charleston", "2 & 4 stabs", "Push (anticipation)", "Freddie Green 4s", "Ballad (half notes)" };
}

int compHits (int pattern, int beats, float swing, double* starts, double* ends, int maxHits)
{
    int n = 0;
    auto add = [&] (double s, double e) { if (n < maxHits && s < e) { starts[n] = s; ends[n] = e; ++n; } };
    auto sw = [&] (double x) { const double f = x - std::floor (x); return std::abs (f - 0.5) < 1.0e-6 ? std::floor (x) + swing : x; };

    for (int bar = 0; bar < beats; bar += 4)
    {
        const double b = bar;
        const double len = juce::jmin (4, beats - bar);
        const double barEnd = b + len;
        switch (pattern)
        {
            case 1: // Charleston
                if (len < 2) { add (b, barEnd - 0.04); break; }
                add (b, b + 0.8); add (b + sw (1.5), barEnd - 0.04); break;
            case 2: // 2 & 4
                if (len < 2) { add (b, b + 0.6); break; }
                add (b + 1, b + 1.6); if (len >= 4) add (b + 3, b + 3.6); break;
            case 3: // push
                if (len < 3) { add (b, barEnd - 0.04); break; }
                add (b, b + 1.3); add (b + sw (2.5), barEnd - 0.04); break;
            case 4: // Freddie Green
                for (int k = 0; k < (int) len; ++k) add (b + k, b + k + 0.55); break;
            case 5: // ballad halves
                add (b, b + juce::jmin (2.0, len) - 0.04); if (len > 2) add (b + 2, barEnd - 0.04); break;
            default:
                add (b, barEnd - 0.02); break;
        }
    }
    if (n == 0) add (0, beats - 0.02);
    return n;
}

//==============================================================================
ChangesProcessor::ChangesProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CHANGES", createLayout())
{
    pads = palettes::levelDefault (Level::Intermediate, false);
    syncPadsToAudio();
    syncProgressionToAudio();
    for (auto* id : { &ids::level, &ids::minor, &ids::lockPalette })
        apvts.addParameterListener (*id, this);
    startTimerHz (20);
}

ChangesProcessor::~ChangesProcessor()
{
    stopTimer();
    for (auto* id : { &ids::level, &ids::minor, &ids::lockPalette })
        apvts.removeParameterListener (*id, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout ChangesProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto choice = [&] (const String& id, const String& name, const StringArray& items, int def)
    { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto boolean = [&] (const String& id, const String& name, bool def)
    { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto real = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, const String& label)
    { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withLabel (label))); };

    choice (ids::key, "Key", keyNames(), 0);
    boolean (ids::minor, "Minor Key", false);
    choice (ids::level, "Theory Level", levelNames(), 1);
    choice (ids::style, "Voicing Style", styleNames(), 0);
    boolean (ids::bass, "Bass Note", true);
    boolean (ids::ext, "Extensions", true);
    boolean (ids::alt, "Alterations", false);
    boolean (ids::vl, "Voice Leading", true);
    boolean (ids::reharm, "Auto Reharm", false);
    boolean (ids::latch, "Latch", false);
    boolean (ids::lockPalette, "Lock Palette", false);
    boolean (ids::suggest, "Suggest Next", true);
    choice (ids::registerShift, "Chord Register", { "Low", "Middle", "High" }, 1);
    choice (ids::zone, "Chord Zone", { "C1 (MIDI 24)", "C2 (MIDI 36)", "C3 (MIDI 48)" }, 1);
    boolean (ids::melSnap, "Melody Snap", true);
    choice (ids::melMode, "Melody Scale", melodyModeNames(), 1);
    boolean (ids::whiteMap, "White-Key Map", false);
    choice (ids::harmonize, "Harmonize", harmonizeNames(), 0);
    choice (ids::melOctave, "Melody Octave", { "-2 oct", "-1 oct", "0 oct", "+1 oct", "+2 oct" }, 2);
    boolean (ids::showScale, "Show Scale", true);
    real (ids::strum, "Strum", NormalisableRange<float> (0.0f, 250.0f, 0.1f, 0.5f), 22.0f, "ms");
    choice (ids::strumDir, "Strum Direction", { "Up", "Down", "Alternate" }, 0);
    real (ids::humanize, "Humanize", NormalisableRange<float> (0.0f, 1.0f), 0.3f, "");
    real (ids::dynamics, "Dynamics", NormalisableRange<float> (0.0f, 1.0f), 0.75f, "");
    choice (ids::lhInst, "Chord Sound", instrumentNames(), 0);
    choice (ids::rhInst, "Melody Sound", instrumentNames(), 3);
    real (ids::tone, "Tone", NormalisableRange<float> (0.0f, 1.0f), 0.5f, "");
    real (ids::release, "Release", NormalisableRange<float> (0.0f, 1.0f), 0.35f, "");
    real (ids::chorus, "Chorus", NormalisableRange<float> (0.0f, 1.0f), 0.25f, "");
    real (ids::reverb, "Reverb", NormalisableRange<float> (0.0f, 1.0f), 0.3f, "");
    real (ids::motion, "Tremolo", NormalisableRange<float> (0.0f, 1.0f), 0.12f, "");
    real (ids::chordVol, "Chord Volume", NormalisableRange<float> (-60.0f, 6.0f, 0.1f, 2.5f), -2.0f, "dB");
    real (ids::melVol, "Melody Volume", NormalisableRange<float> (-60.0f, 6.0f, 0.1f, 2.5f), -2.0f, "dB");
    real (ids::masterVol, "Master Volume", NormalisableRange<float> (-60.0f, 6.0f, 0.1f, 2.5f), -3.0f, "dB");
    boolean (ids::internal, "Internal Sound", true);
    boolean (ids::midiOut, "MIDI Out", true);
    real (ids::bpm, "Tempo", NormalisableRange<float> (40.0f, 280.0f, 1.0f), 120.0f, "BPM");
    choice (ids::beats, "Beats per Chord", { "1 beat", "2 beats", "3 beats", "4 beats", "8 beats" }, 3);
    choice (ids::comp, "Comp Pattern", compNames(), 0);
    real (ids::swing, "Swing", NormalisableRange<float> (0.5f, 0.75f, 0.01f), 0.62f, "");
    return l;
}

//==============================================================================
static float rawValue (const juce::AudioProcessorValueTreeState& s, const juce::String& id)
{
    if (auto* p = s.getRawParameterValue (id)) return p->load();
    return 0.0f;
}

Options ChangesProcessor::currentOptions() const
{
    Options o;
    o.keyTonic      = (int) rawValue (apvts, ids::key);
    o.minorKey      = rawValue (apvts, ids::minor) > 0.5f;
    o.level         = (Level) (int) rawValue (apvts, ids::level);
    o.style         = (Style) (int) rawValue (apvts, ids::style);
    o.bassNote      = rawValue (apvts, ids::bass) > 0.5f;
    o.extensions    = rawValue (apvts, ids::ext) > 0.5f;
    o.alterations   = rawValue (apvts, ids::alt) > 0.5f;
    o.voiceLeading  = rawValue (apvts, ids::vl) > 0.5f;
    o.autoReharm    = rawValue (apvts, ids::reharm) > 0.5f;
    o.registerShift = (int) rawValue (apvts, ids::registerShift) - 1;
    return o;
}

MelodyMode ChangesProcessor::melodyMode() const { return (MelodyMode) (int) rawValue (apvts, ids::melMode); }
int ChangesProcessor::chordZoneStart() const { return 24 + 12 * (int) rawValue (apvts, ids::zone); }

//==============================================================================
void ChangesProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    lhSynth.setCurrentPlaybackSampleRate (sampleRate);
    rhSynth.setCurrentPlaybackSampleRate (sampleRate);
    lhBuf.setSize (2, samplesPerBlock * 2);
    rhBuf.setSize (2, samplesPerBlock * 2);
    mixBuf.setSize (2, samplesPerBlock * 2);
    reverb.setSampleRate (sampleRate);
    chorus.prepare ({ sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock * 2), 2 });
    chorus.setRate (0.6f); chorus.setDepth (0.22f); chorus.setCentreDelay (8.0f); chorus.setFeedback (0.0f);
    for (auto* g : { &chordGain, &melGain, &masterGain }) g->reset (sampleRate, 0.03);
    chordGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (rawValue (apvts, ids::chordVol), -60.0f));
    melGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (rawValue (apvts, ids::melVol), -60.0f));
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (rawValue (apvts, ids::masterVol), -60.0f));
    keyboardState.reset();
}

bool ChangesProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

//==============================================================================
void ChangesProcessor::schedule (juce::int64 time, int note, float vel, bool on, int bus)
{
    if (note < 0 || note > 127) return;
    if (numEvents >= (int) events.size())
    {
        if (on) return;           // never drop a note-off: overwrite the oldest note-on instead
        for (int i = 0; i < numEvents; ++i) if (events[(size_t) i].on) { events[(size_t) i] = { time, note, vel, on, bus }; return; }
        return;
    }
    events[(size_t) numEvents++] = { time, note, vel, on, bus };
}

void ChangesProcessor::setSoundingBit (std::atomic<uint32_t>* bits, int note, bool on)
{
    const uint32_t mask = 1u << (note & 31);
    if (on) bits[note >> 5].fetch_or (mask); else bits[note >> 5].fetch_and (~mask);
}

void ChangesProcessor::dispatchEvents (int numSamples, juce::MidiBuffer& lhMidi, juce::MidiBuffer& rhMidi, juce::MidiBuffer& outMidi)
{
    const juce::int64 end = now + numSamples;
    Event due[1024]; int nd = 0;
    for (int i = 0; i < numEvents;)
    {
        if (events[(size_t) i].time < end) { due[nd++] = events[(size_t) i]; events[(size_t) i] = events[(size_t) --numEvents]; }
        else ++i;
    }
    std::sort (due, due + nd, [] (const Event& a, const Event& b) { return a.time != b.time ? a.time < b.time : (! a.on && b.on); });

    for (int i = 0; i < nd; ++i)
    {
        const Event& e = due[i];
        const int pos = (int) juce::jlimit<juce::int64> (0, numSamples - 1, e.time - now);
        auto& synthMidi = e.bus == 0 ? lhMidi : rhMidi;
        const int ch = e.bus == 0 ? 1 : 2;
        int& count = soundCount[e.bus][e.note];
        auto* bits = e.bus == 0 ? soundingChord : soundingMelody;

        if (e.on)
        {
            const auto v = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (e.vel * 127.0f));
            if (count > 0)
            {
                synthMidi.addEvent (juce::MidiMessage::noteOff (ch, e.note), pos);
                outMidi.addEvent (juce::MidiMessage::noteOff (ch, e.note), pos);
            }
            ++count;
            synthMidi.addEvent (juce::MidiMessage::noteOn (ch, e.note, v), pos);
            outMidi.addEvent (juce::MidiMessage::noteOn (ch, e.note, v), pos);
            setSoundingBit (bits, e.note, true);
        }
        else if (count > 0)
        {
            if (--count == 0)
            {
                synthMidi.addEvent (juce::MidiMessage::noteOff (ch, e.note), pos);
                outMidi.addEvent (juce::MidiMessage::noteOff (ch, e.note), pos);
                setSoundingBit (bits, e.note, false);
            }
        }
    }
}

float ChangesProcessor::shapeVelocity (float v)
{
    const float dyn = rawValue (apvts, ids::dynamics);
    return juce::jlimit (0.05f, 1.0f, 0.72f * (1.0f - dyn) + v * dyn);
}

//==============================================================================
void ChangesProcessor::handleChordOn (const ChordSpec& spec, int pad, float vel, int offset, bool fromSeq)
{
    const Options opt = currentOptions();
    ResolvedChord rc = resolve (spec, opt);
    Voicing v = buildVoicing (rc, spec, opt, prevVoicing);
    addVoicedTensions (rc, v);

    currentChord = rc;
    currentVoicing = v;
    prevVoicing = v; prevVoicing.bass = -1;
    haveChord = true;

    currentPad.store (pad);
    currentSpec.store (spec.pack());
    chordSerial.fetch_add (1);

    if (! fromSeq && recording.load())
    {
        ChordSpec rec = spec;
        static const int beatTable[5] = { 1, 2, 3, 4, 8 };
        rec.beats = beatTable[juce::jlimit (0, 4, (int) rawValue (apvts, ids::beats))];
        const auto scope = recFifo.write (1);
        if (scope.blockSize1 > 0) recBuffer[(size_t) scope.startIndex1] = rec.pack();
    }

    if (! fromSeq)
    {
        soundCurrentChord (offset, vel, 0);
        activePad = pad;
    }
}

void ChangesProcessor::soundCurrentChord (int offset, float vel, double)
{
    releaseChord (offset);
    if (! haveChord) return;

    int order[16]; int n = 0;
    if (currentVoicing.bass >= 0) order[n++] = currentVoicing.bass;
    for (int i = 0; i < currentVoicing.count && n < 16; ++i) order[n++] = currentVoicing.notes[i];

    const int dir = (int) rawValue (apvts, ids::strumDir);
    bool down = dir == 1;
    if (dir == 2) { strumFlip = ! strumFlip; down = strumFlip; }

    const float strumMs = rawValue (apvts, ids::strum);
    const float human = rawValue (apvts, ids::humanize);
    const float baseVel = shapeVelocity (vel);

    for (int k = 0; k < n; ++k)
    {
        const int idx = down ? n - 1 - k : k;
        const int note = order[idx];
        double t = n > 1 ? strumMs * (double) k / (double) (n - 1) : 0.0;
        t += human * rng.nextDouble() * 14.0;                       // late-ish human timing
        const juce::int64 when = now + offset + (juce::int64) (t * 0.001 * sr);

        float v = baseVel;
        if (idx == n - 1) v *= 1.06f;                               // top voice sings a little
        else if (idx == 0 && currentVoicing.bass >= 0) v *= 0.92f;  // bass a touch softer
        else v *= 0.94f;
        v *= 1.0f + human * (rng.nextFloat() - 0.5f) * 0.25f;
        schedule (when, note, juce::jlimit (0.05f, 1.0f, v), true, 0);

        if (numHeldChordNotes < 16) { heldChordNotes[numHeldChordNotes] = note; heldOnTime[numHeldChordNotes] = when; ++numHeldChordNotes; }
    }
}

void ChangesProcessor::releaseChord (int offset)
{
    for (int i = 0; i < numHeldChordNotes; ++i)
        schedule (juce::jmax (now + offset, heldOnTime[i] + 1), heldChordNotes[i], 0.0f, false, 0);
    numHeldChordNotes = 0;
}

void ChangesProcessor::handleMelodyOn (int note, float vel, int offset)
{
    const Options opt = currentOptions();
    ResolvedChord rc = currentChord;
    if (! haveChord)
    {
        ChordSpec s; s.rootOffset = 0; s.quality = opt.minorKey ? Quality::Min6 : Quality::Maj7;
        rc = resolve (s, opt);
    }

    const int octave = (int) rawValue (apvts, ids::melOctave) - 2;
    int out = note + octave * 12;

    if (rawValue (apvts, ids::melSnap) > 0.5f)
    {
        const uint16_t mask = melodyMask (melodyMode(), rc, opt);
        if (rawValue (apvts, ids::whiteMap) > 0.5f)
        {
            const int m = mapWhiteKey (out, mask, opt.keyTonic);
            out = m >= 0 ? m : snapToMask (out, chordToneMask (rc));
        }
        else out = snapToMask (out, mask);
    }
    out = juce::jlimit (0, 127, out);

    int notes[6]; int n = 0;
    notes[n++] = out;
    n += harmonize ((Harmonize) (int) rawValue (apvts, ids::harmonize), out, rc, opt, notes + 1, 5);

    // if the same key is still held (shouldn't happen) release it first
    handleMelodyOff (note, offset);

    const float human = rawValue (apvts, ids::humanize);
    const float v = shapeVelocity (vel);
    for (int i = 0; i < n; ++i)
    {
        const juce::int64 when = now + offset + (juce::int64) (human * rng.nextDouble() * 6.0 * 0.001 * sr);
        schedule (when, notes[i], juce::jlimit (0.05f, 1.0f, i == 0 ? v : v * 0.8f), true, 1);
        melodyMap[note][i] = notes[i];
    }
    melodyMapCount[note] = n;
}

void ChangesProcessor::handleMelodyOff (int note, int offset)
{
    for (int i = 0; i < melodyMapCount[note]; ++i)
        schedule (now + offset + (juce::int64) (0.008 * sr), melodyMap[note][i], 0.0f, false, 1);
    melodyMapCount[note] = 0;
}

//==============================================================================
void ChangesProcessor::setPlaying (bool shouldPlay)
{
    if (shouldPlay) playRequested = true;
    else stopRequested = true;
}

void ChangesProcessor::beginStep (int index, double startBeat, int sampleOffset)
{
    const int len = progLen.load();
    if (len <= 0) return;
    seqIndex = ((index % len) + len) % len;
    const ChordSpec spec = ChordSpec::unpack (progAudio[(size_t) seqIndex].load());
    stepStartBeat = startBeat;
    double s[8], e[8];
    numStepHits = compHits ((int) rawValue (apvts, ids::comp), spec.beats, rawValue (apvts, ids::swing), s, e, 8);
    for (int i = 0; i < numStepHits; ++i) stepHits[i] = { s[i], e[i] };
    nextHit = 0; hitSounding = false;
    handleChordOn (spec, -1, 0.75f, sampleOffset, true);
    playStep.store (seqIndex);
}

void ChangesProcessor::runSequencer (int numSamples)
{
    if (stopRequested.exchange (false) && playing.load())
    {
        releaseChord (0);
        playing = false;
        playStep.store (-1);
    }
    if (playRequested.exchange (false) && progLen.load() > 0)
    {
        playing = true;
        seqBeat = 0.0;
        beginStep (0, 0.0, 0);
    }
    if (! playing.load()) return;
    if (progLen.load() <= 0) { releaseChord (0); playing = false; playStep.store (-1); return; }

    double bpm = rawValue (apvts, ids::bpm);
    if (hostBpm.load() > 1.0f) bpm = hostBpm.load();
    const double bps = bpm / (60.0 * sr);
    const double blockEnd = seqBeat + numSamples * bps;

    for (int guard = 0; guard < 64; ++guard)
    {
        const ChordSpec spec = ChordSpec::unpack (progAudio[(size_t) seqIndex].load());
        const double stepEnd = stepStartBeat + spec.beats;
        const double hitEnd  = hitSounding ? stepStartBeat + stepHits[hitIndex].end : 1.0e12;
        const double hitStart = nextHit < numStepHits ? stepStartBeat + stepHits[nextHit].start : 1.0e12;
        const double e = juce::jmin (stepEnd, hitEnd, hitStart);
        if (e >= blockEnd) break;
        const int offset = juce::jlimit (0, numSamples - 1, (int) ((e - seqBeat) / bps));

        if (hitSounding && hitEnd <= stepEnd && hitEnd <= hitStart)
        {
            releaseChord (offset); hitSounding = false;
        }
        else if (stepEnd <= hitStart)
        {
            if (hitSounding) { releaseChord (offset); hitSounding = false; }
            beginStep (seqIndex + 1, stepEnd, offset);
        }
        else
        {
            const float accent = nextHit == 0 ? 0.78f : 0.70f;
            soundCurrentChord (offset, accent, 0);
            hitSounding = true; hitIndex = nextHit; ++nextHit;
        }
    }
    seqBeat = blockEnd;
}

//==============================================================================
void ChangesProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch) buffer.clear (ch, 0, numSamples);
    buffer.clear();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) hostBpm.store ((float) *b);

    lhSynth.shared.instrument = (int) rawValue (apvts, ids::lhInst);
    rhSynth.shared.instrument = (int) rawValue (apvts, ids::rhInst);
    for (auto* s : { &lhSynth, &rhSynth })
    {
        s->shared.tone = rawValue (apvts, ids::tone);
        s->shared.release = rawValue (apvts, ids::release);
    }

    juce::MidiBuffer lhMidi, rhMidi, outMidi;

    if (panicRequested.exchange (false))
    {
        numEvents = 0; numHeldChordNotes = 0; activePad = -1;
        for (int b = 0; b < 2; ++b)
            for (int n = 0; n < 128; ++n)
                if (soundCount[b][n] > 0) { soundCount[b][n] = 0; outMidi.addEvent (juce::MidiMessage::noteOff (b + 1, n), 0); }
        for (auto& w : soundingChord) w = 0;
        for (auto& w : soundingMelody) w = 0;
        for (auto& c : melodyMapCount) c = 0;
        lhSynth.allNotesOff (0, false); rhSynth.allNotesOff (0, false);
        outMidi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        outMidi.addEvent (juce::MidiMessage::allNotesOff (2), 0);
        if (playing.load()) { playing = false; playStep.store (-1); }
    }

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    if (auditionOffReq.exchange (false) && activePad == -2) { releaseChord (0); activePad = -1; }
    if (auditionOnReq.exchange (false))
    {
        handleChordOn (ChordSpec::unpack (auditionValue.load()), -2, 0.8f, 0, true);
        soundCurrentChord (0, 0.8f, 0);
        activePad = -2;
    }

    const int zone = chordZoneStart();
    const bool latch = rawValue (apvts, ids::latch) > 0.5f;

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int pos = meta.samplePosition;
        if (m.isNoteOn())
        {
            const int n = m.getNoteNumber();
            if (n >= zone && n < zone + palettes::numPads)
            {
                const int pad = n - zone;
                const ChordSpec spec = ChordSpec::unpack (padsAudio[(size_t) pad].load());
                handleChordOn (spec, pad, m.getFloatVelocity(), pos, false);
            }
            else if (n >= zone + palettes::numPads) handleMelodyOn (n, m.getFloatVelocity(), pos);
        }
        else if (m.isNoteOff())
        {
            const int n = m.getNoteNumber();
            if (n >= zone && n < zone + palettes::numPads)
            {
                if (! latch && n - zone == activePad) { releaseChord (pos); activePad = -1; }
            }
            else if (n >= zone + palettes::numPads) handleMelodyOff (n, pos);
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            panicRequested = true;
        }
        else if (m.isController() || m.isPitchWheel() || m.isChannelPressure())
        {
            if (m.isSustainPedalOn() || m.isSustainPedalOff())
            {
                lhMidi.addEvent (m, pos); rhMidi.addEvent (m, pos);
            }
            outMidi.addEvent (m, pos);
        }
    }

    runSequencer (numSamples);
    dispatchEvents (numSamples, lhMidi, rhMidi, outMidi);

    // ---- render
    if (lhBuf.getNumSamples() < numSamples) { lhBuf.setSize (2, numSamples, false, false, true); rhBuf.setSize (2, numSamples, false, false, true); }
    lhBuf.clear(); rhBuf.clear();
    lhSynth.renderNextBlock (lhBuf, lhMidi, 0, numSamples);
    rhSynth.renderNextBlock (rhBuf, rhMidi, 0, numSamples);

    chordGain.setTargetValue (juce::Decibels::decibelsToGain (rawValue (apvts, ids::chordVol), -60.0f));
    melGain.setTargetValue (juce::Decibels::decibelsToGain (rawValue (apvts, ids::melVol), -60.0f));
    masterGain.setTargetValue (juce::Decibels::decibelsToGain (rawValue (apvts, ids::masterVol), -60.0f));

    const bool internal = rawValue (apvts, ids::internal) > 0.5f;
    const float motion = rawValue (apvts, ids::motion);
    const double lfoInc = (2.0 + motion * 4.5) / sr;

    float peakC = 0, peakM = 0;
    if (mixBuf.getNumSamples() < numSamples) mixBuf.setSize (2, numSamples, false, false, true);
    juce::AudioBuffer<float> mix (mixBuf.getArrayOfWritePointers(), 2, numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        const float cg = chordGain.getNextValue(), mg = melGain.getNextValue();
        float cl = lhBuf.getSample (0, i) * cg, cr = lhBuf.getSample (1, i) * cg;
        float ml = rhBuf.getSample (0, i) * mg, mr = rhBuf.getSample (1, i) * mg;
        peakC = juce::jmax (peakC, std::abs (cl), std::abs (cr));
        peakM = juce::jmax (peakM, std::abs (ml), std::abs (mr));

        // stereo tremolo / auto-pan (suitcase style)
        const float lfo = (float) std::sin (lfoPhase * juce::MathConstants<double>::twoPi);
        lfoPhase += lfoInc; if (lfoPhase >= 1.0) lfoPhase -= 1.0;
        const float d = motion * 0.85f;
        cl *= 1.0f - d * (0.5f + 0.5f * lfo);
        cr *= 1.0f - d * (0.5f - 0.5f * lfo);
        ml *= 1.0f - d * 0.6f * (0.5f + 0.5f * lfo);
        mr *= 1.0f - d * 0.6f * (0.5f - 0.5f * lfo);

        mix.setSample (0, i, cl + ml);
        mix.setSample (1, i, cr + mr);
    }

    const float chorusAmt = rawValue (apvts, ids::chorus);
    if (chorusAmt > 0.001f)
    {
        chorus.setMix (chorusAmt * 0.55f);
        juce::dsp::AudioBlock<float> block (mix);
        chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    const float rv = rawValue (apvts, ids::reverb);
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.55f + rv * 0.35f; rp.damping = 0.5f; rp.width = 1.0f;
    rp.wetLevel = rv * 0.42f; rp.dryLevel = 1.0f - rv * 0.3f; rp.freezeMode = 0.0f;
    reverb.setParameters (rp);
    if (rv > 0.001f) reverb.processStereo (mix.getWritePointer (0), mix.getWritePointer (1), numSamples);

    float pl = 0, pr = 0; double sl = 0, sr2 = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = masterGain.getNextValue();
        float l = mix.getSample (0, i) * g, r = mix.getSample (1, i) * g;
        // gentle safety limiter
        auto limit = [] (float x) { const float a = std::abs (x); return a < 0.9f ? x : std::copysign (0.9f + 0.1f * std::tanh ((a - 0.9f) * 10.0f), x); };
        l = limit (l); r = limit (r);
        pl = juce::jmax (pl, std::abs (l)); pr = juce::jmax (pr, std::abs (r));
        sl += l * l; sr2 += r * r;
        if (internal)
        {
            if (buffer.getNumChannels() > 1) { buffer.setSample (0, i, l); buffer.setSample (1, i, r); }
            else buffer.setSample (0, i, 0.5f * (l + r));
        }
    }

    auto maxStore = [] (std::atomic<float>& a, float v) { float cur = a.load(); while (v > cur && ! a.compare_exchange_weak (cur, v)) {} };
    const float scale = internal ? 1.0f : 0.0f;
    maxStore (meterL, pl * scale); maxStore (meterR, pr * scale);
    maxStore (rmsL, (float) std::sqrt (sl / juce::jmax (1, numSamples)) * scale);
    maxStore (rmsR, (float) std::sqrt (sr2 / juce::jmax (1, numSamples)) * scale);
    maxStore (meterChord, peakC * scale); maxStore (meterMelody, peakM * scale);

    if (rawValue (apvts, ids::midiOut) > 0.5f) midi.swapWith (outMidi);
    else midi.clear();

    now += numSamples;
}

//==============================================================================
void ChangesProcessor::auditionPad (int pad, bool on, float velocity)
{
    const int note = chordZoneStart() + pad;
    if (on) keyboardState.noteOn (1, note, velocity);
    else keyboardState.noteOff (1, note, 0.0f);
}

//==============================================================================
// Palette
void ChangesProcessor::syncPadsToAudio()
{
    for (int i = 0; i < palettes::numPads; ++i) padsAudio[(size_t) i].store (pads[(size_t) i].pack());
}

ChordSpec ChangesProcessor::getPad (int i) const { return pads[(size_t) juce::jlimit (0, palettes::numPads - 1, i)]; }

void ChangesProcessor::setPad (int i, const ChordSpec& s)
{
    if (i < 0 || i >= palettes::numPads) return;
    pads[(size_t) i] = s; pads[(size_t) i].valid = true;
    syncPadsToAudio(); sendChangeMessage();
}

void ChangesProcessor::swapPads (int a, int b)
{
    if (a == b || a < 0 || b < 0 || a >= palettes::numPads || b >= palettes::numPads) return;
    std::swap (pads[(size_t) a], pads[(size_t) b]);
    syncPadsToAudio(); sendChangeMessage();
}

void ChangesProcessor::copyPad (int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= palettes::numPads || to >= palettes::numPads) return;
    pads[(size_t) to] = pads[(size_t) from];
    syncPadsToAudio(); sendChangeMessage();
}

void ChangesProcessor::loadPreset (int index)
{
    presetIndex = juce::jlimit (0, palettes::presetNames().size() - 1, index);
    const Options o = currentOptions();
    pads = palettes::preset (presetIndex, o.level, o.minorKey);
    paletteLevel = (int) o.level; paletteMinor = o.minorKey ? 1 : 0;
    syncPadsToAudio(); sendChangeMessage();
}

juce::String ChangesProcessor::padDisplayName (int i) const
{
    const Options o = currentOptions();
    const ChordSpec s = getPad (i);
    ResolvedChord rc = resolve (s, o);
    const Voicing v = buildVoicing (rc, s, o, {});
    addVoicedTensions (rc, v);
    return chordName (rc, o);
}

bool ChangesProcessor::savePaletteToFile (const juce::File& f) const
{
    juce::XmlElement x ("ChangesPalette");
    x.setAttribute ("version", 1);
    x.setAttribute ("preset", presetIndex);
    for (int i = 0; i < palettes::numPads; ++i)
    {
        auto* p = x.createNewChildElement ("Pad");
        p->setAttribute ("index", i);
        p->setAttribute ("spec", (int) pads[(size_t) i].pack());
    }
    return x.writeTo (f);
}

bool ChangesProcessor::loadPaletteFromFile (const juce::File& f)
{
    auto x = juce::XmlDocument::parse (f);
    if (x == nullptr || ! x->hasTagName ("ChangesPalette")) return false;
    for (auto* p : x->getChildWithTagNameIterator ("Pad"))
    {
        const int i = p->getIntAttribute ("index", -1);
        if (i >= 0 && i < palettes::numPads) pads[(size_t) i] = ChordSpec::unpack ((uint32_t) p->getIntAttribute ("spec"));
    }
    presetIndex = x->getIntAttribute ("preset", 0);
    syncPadsToAudio(); sendChangeMessage();
    return true;
}

void ChangesProcessor::parameterChanged (const juce::String&, float)
{
    paletteRegenPending = true;
    triggerAsyncUpdate();
}

void ChangesProcessor::handleAsyncUpdate()
{
    if (! paletteRegenPending.exchange (false)) return;
    const Options o = currentOptions();
    if (rawValue (apvts, ids::lockPalette) > 0.5f) { sendChangeMessage(); return; }
    if ((int) o.level == paletteLevel && (o.minorKey ? 1 : 0) == paletteMinor) { sendChangeMessage(); return; }
    pads = palettes::preset (presetIndex, o.level, o.minorKey);
    paletteLevel = (int) o.level; paletteMinor = o.minorKey ? 1 : 0;
    syncPadsToAudio(); sendChangeMessage();
}

void ChangesProcessor::timerCallback()
{
    const int ready = recFifo.getNumReady();
    if (ready > 0)
    {
        const auto scope = recFifo.read (ready);
        for (int i = 0; i < scope.blockSize1; ++i) progression.push_back (ChordSpec::unpack (recBuffer[(size_t) (scope.startIndex1 + i)]));
        for (int i = 0; i < scope.blockSize2; ++i) progression.push_back (ChordSpec::unpack (recBuffer[(size_t) (scope.startIndex2 + i)]));
        while (progression.size() > 64) progression.erase (progression.begin());
        syncProgressionToAudio();
        sendChangeMessage();
    }
}

//==============================================================================
// Progression
void ChangesProcessor::syncProgressionToAudio()
{
    const int n = (int) juce::jmin<size_t> (progression.size(), progAudio.size());
    for (int i = 0; i < n; ++i) progAudio[(size_t) i].store (progression[(size_t) i].pack());
    progLen.store (n);
}

void ChangesProcessor::progressionAdd (const ChordSpec& s, int insertAt)
{
    if (progression.size() >= 64) return;
    if (insertAt < 0 || insertAt >= (int) progression.size()) progression.push_back (s);
    else progression.insert (progression.begin() + insertAt, s);
    syncProgressionToAudio(); sendChangeMessage();
}

void ChangesProcessor::progressionRemove (int i)
{
    if (i < 0 || i >= (int) progression.size()) return;
    progression.erase (progression.begin() + i);
    syncProgressionToAudio(); sendChangeMessage();
}

void ChangesProcessor::progressionMove (int from, int to)
{
    const int n = (int) progression.size();
    if (from < 0 || from >= n) return;
    to = juce::jlimit (0, n - 1, to);
    if (from == to) return;
    auto s = progression[(size_t) from];
    progression.erase (progression.begin() + from);
    progression.insert (progression.begin() + to, s);
    syncProgressionToAudio(); sendChangeMessage();
}

void ChangesProcessor::progressionSetBeats (int i, int beats)
{
    if (i < 0 || i >= (int) progression.size()) return;
    progression[(size_t) i].beats = juce::jlimit (1, 15, beats);
    syncProgressionToAudio(); sendChangeMessage();
}

void ChangesProcessor::progressionClear()
{
    progression.clear();
    syncProgressionToAudio(); sendChangeMessage();
}

juce::MidiFile ChangesProcessor::renderProgressionMidi() const
{
    constexpr int ppq = 960;
    const Options o = currentOptions();
    const double bpm = rawValue (apvts, ids::bpm);
    const int pattern = (int) rawValue (apvts, ids::comp);
    const float swing = rawValue (apvts, ids::swing);
    const float strumMs = rawValue (apvts, ids::strum);
    juce::Random r (7);

    juce::MidiMessageSequence seq;
    auto meta = juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / bpm)); meta.setTimeStamp (0); seq.addEvent (meta);
    auto ts = juce::MidiMessage::timeSignatureMetaEvent (4, 4); ts.setTimeStamp (0); seq.addEvent (ts);
    auto name = juce::MidiMessage::textMetaEvent (3, "Changes progression"); name.setTimeStamp (0); seq.addEvent (name);

    Voicing prev;
    double beat = 0;
    for (const auto& spec : progression)
    {
        ResolvedChord rc = resolve (spec, o);
        Voicing v = buildVoicing (rc, spec, o, prev);
        prev = v; prev.bass = -1;

        int notes[16]; int n = 0;
        if (v.bass >= 0) notes[n++] = v.bass;
        for (int i = 0; i < v.count; ++i) notes[n++] = v.notes[i];

        double s[8], e[8];
        const int hits = compHits (pattern, spec.beats, swing, s, e, 8);
        for (int h = 0; h < hits; ++h)
        {
            for (int k = 0; k < n; ++k)
            {
                const double strumBeats = n > 1 ? (strumMs * 0.001 * bpm / 60.0) * k / (n - 1) : 0.0;
                const double on = (beat + s[h] + strumBeats) * ppq;
                const double off = (beat + e[h]) * ppq;
                const int vel = juce::jlimit (30, 120, 88 + (k == n - 1 ? 8 : 0) - (h > 0 ? 6 : 0) + r.nextInt (7) - 3);
                auto mOn = juce::MidiMessage::noteOn (1, notes[k], (juce::uint8) vel); mOn.setTimeStamp (on);
                auto mOff = juce::MidiMessage::noteOff (1, notes[k]); mOff.setTimeStamp (juce::jmax (on + 1, off));
                seq.addEvent (mOn); seq.addEvent (mOff);
            }
        }
        beat += spec.beats;
    }
    auto endTrack = juce::MidiMessage::endOfTrack(); endTrack.setTimeStamp (beat * ppq); seq.addEvent (endTrack);
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    mf.addTrack (seq);
    return mf;
}

juce::File ChangesProcessor::writeProgressionMidiTemp() const
{
    const Options o = currentOptions();
    juce::String nm = "Changes " + noteName (o.keyTonic, o.keyTonic, o.minorKey) + (o.minorKey ? "m" : "") + " progression.mid";
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (nm);
    f.deleteFile();
    juce::FileOutputStream os (f);
    if (os.openedOk()) { renderProgressionMidi().writeTo (os); os.flush(); }
    return f;
}

//==============================================================================
void ChangesProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("PALETTE"), nullptr);
    state.removeChild (state.getChildWithName ("PROGRESSION"), nullptr);

    juce::ValueTree pal ("PALETTE");
    pal.setProperty ("preset", presetIndex, nullptr);
    for (int i = 0; i < palettes::numPads; ++i) pal.setProperty ("p" + juce::String (i), (int) pads[(size_t) i].pack(), nullptr);
    state.appendChild (pal, nullptr);

    juce::ValueTree prog ("PROGRESSION");
    for (auto& s : progression) { juce::ValueTree c ("CHORD"); c.setProperty ("spec", (int) s.pack(), nullptr); prog.appendChild (c, nullptr); }
    state.appendChild (prog, nullptr);

    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ChangesProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType())) return;

    auto pal = state.getChildWithName ("PALETTE");
    auto prog = state.getChildWithName ("PROGRESSION");
    state.removeChild (pal, nullptr);
    state.removeChild (prog, nullptr);
    apvts.replaceState (state);

    if (pal.isValid())
    {
        presetIndex = pal.getProperty ("preset", 0);
        for (int i = 0; i < palettes::numPads; ++i)
            if (pal.hasProperty ("p" + juce::String (i)))
                pads[(size_t) i] = ChordSpec::unpack ((uint32_t) (int) pal.getProperty ("p" + juce::String (i)));
        const Options o = currentOptions();
        paletteLevel = (int) o.level; paletteMinor = o.minorKey ? 1 : 0;
    }
    progression.clear();
    if (prog.isValid())
        for (auto c : prog) progression.push_back (ChordSpec::unpack ((uint32_t) (int) c.getProperty ("spec")));

    syncPadsToAudio();
    syncProgressionToAudio();
    sendChangeMessage();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChangesProcessor(); }
