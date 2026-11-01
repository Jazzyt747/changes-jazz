// Offline test harness: renders audio for every instrument, checks the progression
// player, MIDI output, state save/restore and takes GUI snapshots.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>
#include <chrono>

using namespace juce;
static int failures = 0;
#define EXPECT(c, msg) do { if (! (c)) { ++failures; std::cout << "FAIL: " << msg << std::endl; } } while (0)

struct Ev { double t; int note; float vel; bool on; };

static void setP (ChangesProcessor& p, const String& id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

/** Renders `seconds` of audio feeding events; returns the audio and collects MIDI out. */
static AudioBuffer<float> render (ChangesProcessor& p, std::vector<Ev> evs, double seconds, std::vector<MidiMessage>* midiOut, double* cpuRatio = nullptr)
{
    const double sr = 48000; const int bs = 512;
    const int total = (int) (seconds * sr);
    AudioBuffer<float> out (2, total); out.clear();
    std::sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b) { return a.t < b.t; });
    size_t next = 0;
    double cpu = 0;
    for (int pos = 0; pos < total; pos += bs)
    {
        const int n = jmin (bs, total - pos);
        AudioBuffer<float> buf (2, n); buf.clear();
        MidiBuffer midi;
        while (next < evs.size() && (int) (evs[next].t * sr) < pos + n)
        {
            const int off = jlimit (0, n - 1, (int) (evs[next].t * sr) - pos);
            midi.addEvent (evs[next].on ? MidiMessage::noteOn (1, evs[next].note, evs[next].vel) : MidiMessage::noteOff (1, evs[next].note), off);
            ++next;
        }
        auto t0 = std::chrono::high_resolution_clock::now();
        p.processBlock (buf, midi);
        cpu += std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, n);
        if (midiOut) for (const auto m : midi) midiOut->push_back (m.getMessage());
    }
    if (cpuRatio) *cpuRatio = cpu / seconds;
    return out;
}

static void writeWav (const AudioBuffer<float>& b, const File& f)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), 48000, 2, 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

static void stats (const AudioBuffer<float>& b, float& peak, float& rms, bool& finite)
{
    peak = 0; double s = 0; finite = true;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float x = b.getSample (c, i);
            if (! std::isfinite (x)) finite = false;
            peak = jmax (peak, std::abs (x)); s += x * x;
        }
    rms = (float) std::sqrt (s / (b.getNumSamples() * b.getNumChannels()));
}

static void runLoop (int ms) { MessageManager::getInstance()->runDispatchLoopUntil (ms); }

static void snapshot (Component& c, const File& f)
{
    Image img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    f.deleteFile();
    FileOutputStream os (f);
    PNGImageFormat png; png.writeImageToStream (img, os);
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? String (argv[1]) : File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    // ---- 1. every instrument: ii-V-I-vi with melody
    const char* instNames[] = { "ep", "grand", "organ", "vibes", "pad" };
    for (int inst = 0; inst < 5; ++inst)
    {
        ChangesProcessor p;
        p.setPlayConfigDetails (0, 2, 48000, 512);
        p.prepareToPlay (48000, 512);
        setP (p, ids::lhInst, (float) inst); setP (p, ids::rhInst, (float) inst);
        std::vector<Ev> ev;
        const int zone = 36;
        int padsSeq[] = { 2, 7, 0, 9 };
        for (int i = 0; i < 4; ++i) { ev.push_back ({ i * 1.2, zone + padsSeq[i], 0.8f, true }); ev.push_back ({ i * 1.2 + 1.1, zone + padsSeq[i], 0.f, false }); }
        int mel[] = { 74, 72, 71, 69, 67, 76, 77, 79 };
        for (int i = 0; i < 8; ++i) { ev.push_back ({ 0.1 + i * 0.6, 60 + (mel[i] - 60), 0.7f, true }); ev.push_back ({ 0.1 + i * 0.6 + 0.5, 60 + (mel[i] - 60), 0.f, false }); }
        std::vector<MidiMessage> mo;
        double cpu = 0;
        auto audio = render (p, ev, 6.5, &mo, &cpu);
        float peak, rms; bool finite; stats (audio, peak, rms, finite);
        std::cout << "instrument " << instNames[inst] << ": peak " << peak << " rms " << rms << " cpu " << String (cpu * 100.0, 2) << "% of realtime" << std::endl;
        EXPECT (finite, "NaN/inf in " << instNames[inst]);
        EXPECT (peak > 0.02f, "too quiet " << instNames[inst]);
        EXPECT (peak <= 1.0f, "clipping " << instNames[inst]);
        EXPECT (cpu < 0.25, "too slow " << instNames[inst]);
        writeWav (audio, outDir.getChildFile (String ("demo_") + instNames[inst] + ".wav"));
        int ons = 0; for (auto& m : mo) if (m.isNoteOn()) ++ons;
        EXPECT (ons >= 4 * 4 + 8, "midi out note-ons " << ons);
    }

    // ---- 2. voicings by level, as heard through MIDI out
    for (int level = 0; level < 4; ++level)
    {
        ChangesProcessor p; p.setPlayConfigDetails (0, 2, 48000, 512); p.prepareToPlay (48000, 512);
        setP (p, ids::level, (float) level);
        setP (p, ids::strum, 0.0f); setP (p, ids::humanize, 0.0f);
        if (level == 3) setP (p, ids::alt, 1.0f);
        runLoop (50);                       // let the palette regenerate for the level
        std::vector<Ev> ev; const int zone = 36; int seq[] = { 2, 7, 0 };
        for (int i = 0; i < 3; ++i) { ev.push_back ({ i * 0.5, zone + seq[i], 0.8f, true }); ev.push_back ({ i * 0.5 + 0.45, zone + seq[i], 0.f, false }); }
        std::vector<MidiMessage> mo; render (p, ev, 1.6, &mo);
        String line = theory::levelName ((theory::Level) level) + ": ";
        int chord = 0; double lastT = -1;
        for (auto& m : mo) if (m.isNoteOn() && m.getChannel() == 1) { (void) lastT; line << MidiMessage::getMidiNoteName (m.getNoteNumber(), false, true, 4) << " "; }
        ignoreUnused (chord);
        std::cout << line << "   [" << p.padDisplayName (2) << " | " << p.padDisplayName (7) << " | " << p.padDisplayName (0) << "]" << std::endl;
    }

    // ---- 3. progression player + MIDI out + comp patterns
    {
        ChangesProcessor p; p.setPlayConfigDetails (0, 2, 48000, 512); p.prepareToPlay (48000, 512);
        for (int pad : { 2, 7, 0, 9 }) { auto s = p.getPad (pad); s.beats = 4; p.progressionAdd (s); }
        setP (p, ids::comp, 1.0f);    // Charleston
        setP (p, ids::bpm, 120.0f);
        p.setPlaying (true);
        std::vector<MidiMessage> mo;
        auto audio = render (p, {}, 8.2, &mo);
        int ons = 0; for (auto& m : mo) if (m.isNoteOn()) ++ons;
        float peak, rms; bool finite; stats (audio, peak, rms, finite);
        std::cout << "sequencer: " << ons << " note-ons in 8 s, peak " << peak << std::endl;
        EXPECT (ons >= 4 * 2 * 4, "sequencer produced too few notes: " << ons);
        EXPECT (finite && peak > 0.02f, "sequencer audio");
        writeWav (audio, outDir.getChildFile ("demo_progression_charleston.wav"));

        // MIDI file export
        auto mf = p.renderProgressionMidi();
        File mid = outDir.getChildFile ("progression.mid"); mid.deleteFile();
        { FileOutputStream os (mid); mf.writeTo (os); }
        EXPECT (mid.getSize() > 100, "midi export");
        p.setPlaying (false);
        render (p, {}, 0.1, nullptr);
        EXPECT (! p.isPlaying(), "stop");

        // ---- 4. state round trip
        MemoryBlock mb; p.getStateInformation (mb);
        ChangesProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        EXPECT (q.getProgression().size() == 4, "state progression");
        for (int i = 0; i < 24; ++i) EXPECT (q.getPad (i) == p.getPad (i), "state pad " << i);
    }

    // ---- 5. palette edit operations
    {
        ChangesProcessor p;
        auto a = p.getPad (0), b = p.getPad (7);
        p.swapPads (0, 7); EXPECT (p.getPad (0) == b && p.getPad (7) == a, "swap");
        for (int i = 0; i < palettes::presetNames().size(); ++i) { p.loadPreset (i); EXPECT (p.getPresetIndex() == i, "preset"); }
        File pal = outDir.getChildFile ("test.chgpal");
        EXPECT (p.savePaletteToFile (pal), "save palette");
        ChangesProcessor q; EXPECT (q.loadPaletteFromFile (pal), "load palette");
        for (int i = 0; i < 24; ++i) EXPECT (q.getPad (i) == p.getPad (i), "palette file pad " << i);
    }

    // ---- 6. GUI snapshots
    {
        PropertiesFile::Options o; o.applicationName = "Changes"; o.filenameSuffix = ".settings"; o.folderName = "JazzyT Audio" + String (File::getSeparatorString()) + "Changes";
        int textSize = 0;
        auto setSeen = [&] (bool seen) { PropertiesFile s (o); s.setValue ("seenQuickStart", seen); s.setValue ("textSize", textSize); s.saveIfNeeded(); };

        auto shoot = [&] (int level, bool minor, int key, std::initializer_list<int> padsToPlay, const String& name, bool quickStart, int preset = 0)
        {
            setSeen (! quickStart);
            ChangesProcessor p; p.setPlayConfigDetails (0, 2, 48000, 512); p.prepareToPlay (48000, 512);
            setP (p, ids::level, (float) level); setP (p, ids::minor, minor ? 1.0f : 0.0f); setP (p, ids::key, (float) key);
            runLoop (30);
            if (preset > 0) p.loadPreset (preset);
            for (int pad : { 2, 7, 0, 9, 3 }) { auto s = p.getPad (pad); p.progressionAdd (s); }
            std::vector<Ev> ev;
            double t = 0;
            for (int pad : padsToPlay) { ev.push_back ({ t, 36 + pad, 0.85f, true }); t += 0.3; }
            ev.push_back ({ t + 0.05, 74, 0.8f, true });
            render (p, ev, t + 0.4, nullptr);
            std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
            ed->setSize (1280, 860);
            // feed some audio so meters show
            render (p, {}, 0.15, nullptr);
            runLoop (250);
            render (p, {}, 0.1, nullptr);
            runLoop (100);
            snapshot (*ed, outDir.getChildFile (name));
            std::cout << "snapshot " << name << std::endl;
        };
        for (int ts = 0; ts < 3; ++ts)
        {
            textSize = ts;
            shoot (1, false, 0, { 2 }, "gui_text" + String (ts) + ".png", false);
            shoot (1, false, 0, {}, "gui_quickstart_text" + String (ts) + ".png", true);
        }
        textSize = 1;
        shoot (1, false, 0, { 2 }, "gui_intermediate.png", false);
        shoot (3, false, 5, { 7 }, "gui_conservatory.png", false);
        shoot (0, true, 9, { 0 }, "gui_beginner_minor.png", false);
        shoot (2, false, 10, { 4 }, "gui_advanced_coltrane.png", false, 4);
        shoot (1, false, 0, {}, "gui_quickstart.png", true);
        textSize = 1; setSeen (false);
    }

    std::cout << (failures == 0 ? "ALL RENDER TESTS PASSED" : "RENDER TEST FAILURES: " + String (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
