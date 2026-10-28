// Rendu d'extraits audio et d'une capture de l'interface (pour vérifier sans DAW).
// Usage : SequencesRender <dossier de sortie>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

struct PlayHead final : public juce::AudioPlayHead
{
    double bpm = 190.0, ppq = 0.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        info.setPpqPosition (ppq);
        info.setIsPlaying (true);
        return info;
    }
};

static void set (SequencesAudioProcessor& p, const juce::String& id, float v)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (v));
}

/** Notes : {note, début en noires, durée en noires}. */
struct Note { int note; double start, length; };

static void renderTo (SequencesAudioProcessor& p, const juce::File& file, double beats, std::vector<Note> notes)
{
    const double sr = 48000.0;
    const int block = 256;
    PlayHead head;
    p.setPlayHead (&head);
    p.setRateAndBufferSizeDetails (sr, block);
    p.prepareToPlay (sr, block);

    const double samplesPerBeat = sr * 60.0 / head.bpm;
    const int total = (int) (beats * samplesPerBeat);
    juce::AudioBuffer<float> out (2, total);
    juce::AudioBuffer<float> buffer (2, block);

    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        buffer.setSize (2, n, false, false, true);
        juce::MidiBuffer midi;
        for (auto& note : notes)
        {
            const int on = (int) (note.start * samplesPerBeat), off = (int) ((note.start + note.length) * samplesPerBeat);
            if (on >= pos && on < pos + n) midi.addEvent (juce::MidiMessage::noteOn (1, note.note, (juce::uint8) 110), on - pos);
            if (off >= pos && off < pos + n) midi.addEvent (juce::MidiMessage::noteOff (1, note.note), off - pos);
        }
        head.ppq = pos / samplesPerBeat;
        p.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, pos, buffer, ch, 0, n);
    }

    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
    if (auto* writer = wav.createWriterFor (stream.get(), sr, 2, 24, {}, 0))
    {
        stream.release();
        std::unique_ptr<juce::AudioFormatWriter> w (writer);
        w->writeFromAudioSampleBuffer (out, 0, total);
    }
    std::printf ("%s : crête %.3f\n", file.getFileName().toRawUTF8(), out.getMagnitude (0, total));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    dir.createDirectory();

    // 1. Réglages par défaut, une note tenue
    {
        SequencesAudioProcessor p;
        renderTo (p, dir.getChildFile ("01-default.wav"), 16.0, { { 45, 0.0, 15.0 } });
    }

    // 2. Mono, RANDOM ALL, sur la hauteur + la coupure (séquence hi-tech type)
    {
        SequencesAudioProcessor p;
        p.setRandomSeed (2026);
        set (p, "voiceMode", 0.0f);
        set (p, "drive", 30.0f);
        set (p, "seqDiv", 4.0f);        // 1/16
        p.randomizeAll();
        set (p, "mod3Src", 0.0f);
        set (p, "mod3Dest", (float) seqs::Dest::pitch);
        set (p, "mod3Amt", 25.0f);
        renderTo (p, dir.getChildFile ("02-mono-random-all.wav"), 16.0, { { 45, 0.0, 8.0 }, { 47, 7.9, 8.0 } });
    }

    // 3. Banque d'usine sur la hauteur seule : on entend chaque LFO tour à tour
    {
        SequencesAudioProcessor p;
        set (p, "mod1Dest", 0.0f);
        set (p, "mod2Dest", 0.0f);
        set (p, "mod3Dest", (float) seqs::Dest::pitch);
        set (p, "mod3Amt", 30.0f);
        set (p, "cutoff", 4000.0f);
        set (p, "ampSustain", 100.0f);
        renderTo (p, dir.getChildFile ("03-pitch-bank.wav"), 16.0, { { 45, 0.0, 15.0 } });
    }

    // 4. Hard sync piloté par le séquenceur
    {
        SequencesAudioProcessor p;
        set (p, "fmLevel", 0.0f);
        set (p, "oscLevel", 100.0f);
        set (p, "mod2Dest", (float) seqs::Dest::sync);
        set (p, "mod2Amt", 70.0f);
        set (p, "cutoff", 3000.0f);
        renderTo (p, dir.getChildFile ("04-sync.wav"), 16.0, { { 40, 0.0, 15.0 } });
    }

    // 5. Accord en polyphonique, gate sur l'ampli
    {
        SequencesAudioProcessor p;
        set (p, "mod3Dest", (float) seqs::Dest::amp);
        set (p, "mod3Amt", 100.0f);
        set (p, "ampRelease", 400.0f);
        set (p, "modWave", 7.0f);       // RESO 2
        renderTo (p, dir.getChildFile ("05-poly-gate-reso.wav"), 16.0, { { 57, 0.0, 14.0 }, { 60, 0.0, 14.0 }, { 64, 0.0, 14.0 } });
    }

    // Capture de l'interface
    {
        SequencesAudioProcessor p;
        p.prepareToPlay (48000.0, 256);
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
        juce::Graphics g (image);
        editor->paintEntireComponent (g, false);
        juce::PNGImageFormat png;
        auto file = dir.getChildFile ("interface.png");
        file.deleteFile();
        if (auto stream = file.createOutputStream())
            png.writeImageToStream (image, *stream);
        std::printf ("interface.png\n");
    }
    return 0;
}
