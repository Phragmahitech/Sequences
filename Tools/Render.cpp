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

static void snapshot (SequencesAudioProcessor& p, const juce::File& file, int tab)
{
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    if (auto* e = dynamic_cast<SequencesAudioProcessorEditor*> (editor.get()))
    {
        e->setUiScale (1.0f);
        e->getFacade().showTab (tab);
    }
    juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    juce::Graphics g (image);
    editor->paintEntireComponent (g, false);
    juce::PNGImageFormat png;
    file.deleteFile();
    if (auto stream = file.createOutputStream())
        png.writeImageToStream (image, *stream);
    std::printf ("%s\n", file.getFileName().toRawUTF8());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    const juce::File serumTable (argc > 2 ? juce::String (argv[2]) : juce::String());
    dir.createDirectory();

    // Basse psytrance : 3 doubles-croches sur 4 (la grosse caisse prend la première), 4 mesures
    std::vector<Note> roll;
    for (int beat = 0; beat < 16; ++beat)
        for (int k = 1; k < 4; ++k)
            roll.push_back ({ beat >= 12 ? 43 : 45, beat + k * 0.25, 0.2 });
    const std::vector<Note> held { { 45, 0.0, 15.0 } };
    const std::vector<Note> chord { { 57, 0.0, 14.0 }, { 60, 0.0, 14.0 }, { 64, 0.0, 14.0 } };

    // Chaque preset d'usine
    for (int i = 0; i < SequencesAudioProcessor::numFactoryPresets(); ++i)
    {
        SequencesAudioProcessor p;
        p.presets.loadFactory (i);
        const auto name = SequencesAudioProcessor::factoryPresetName (i);
        const bool poly = p.apvts.getRawParameterValue ("voiceMode")->load() < 0.5f;
        const auto& notes = name.contains ("SAW") || name.contains ("PLUCK") ? chord : name.contains ("SUB") || name.contains ("ACID") || name.contains ("ZAP") || name.contains ("GROWL") ? roll : held;
        renderTo (p, dir.getChildFile (juce::String (i + 1).paddedLeft ('0', 2) + "-" + name.replace (" ", "-").toLowerCase() + ".wav"), 16.0,
                  poly || &notes != &chord ? notes : held);
    }

    // Arpégiateur + Plaits : accord tenu
    {
        SequencesAudioProcessor p;
        p.presets.loadFactory (3);
        set (p, "arpOn", 1.0f);
        set (p, "arpMode", 2.0f);
        set (p, "arpOct", 2.0f);
        renderTo (p, dir.getChildFile ("20-arp-plaits.wav"), 16.0, chord);
    }

    // Table Serum importée, SEQ -> POS
    if (serumTable.existsAsFile())
    {
        SequencesAudioProcessor p;
        std::printf ("import : %s\n", p.loadTableFile (serumTable).toRawUTF8());
        p.setModulationAmount (p.addModulation (seqs::srcSeq, seqs::Dest::wtPos), 0.6f);
        renderTo (p, dir.getChildFile ("21-serum-table.wav"), 16.0, held);
    }

    // Captures de l'interface (preset HITECH ZAPS, une note en cours)
    {
        SequencesAudioProcessor p;
        p.presets.loadFactory (1);
        if (serumTable.existsAsFile())
            p.loadTableFile (serumTable);
        p.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buffer (2, 256);
        for (int b = 0; b < 40; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
            p.processBlock (buffer, midi);
        }
        const char* names[] = { "main", "seq", "matrix", "arp", "fx" };
        for (int t = 0; t < SequencesFacade::numTabs; ++t)
            snapshot (p, dir.getChildFile (juce::String ("interface-") + names[t] + ".png"), t);
    }
    return 0;
}
