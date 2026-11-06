// Tests du plugin complet (paramètres, MIDI, état, presets, interface).
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

#include <cstdio>
#include <optional>

static int failures = 0, checks = 0;

#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        ++checks;                                                           \
        if (! (cond)) {                                                     \
            ++failures;                                                     \
            std::printf ("ECHEC %s:%d : %s | ", __FILE__, __LINE__, #cond); \
            std::printf (__VA_ARGS__);                                      \
            std::printf ("\n");                                             \
        }                                                                   \
    } while (0)

struct FakePlayHead final : public juce::AudioPlayHead
{
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        info.setPpqPosition (ppq);
        info.setIsPlaying (playing);
        return info;
    }
};

static float value (SequencesAudioProcessor& p, const juce::String& id)
{
    auto* param = p.apvts.getParameter (id);
    return param->convertFrom0to1 (param->getValue());
}

static void set (SequencesAudioProcessor& p, const juce::String& id, float v)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (v));
}

static double renderRms (SequencesAudioProcessor& p, int blocks, int blockSize, juce::MidiBuffer first = {})
{
    juce::AudioBuffer<float> buffer (2, blockSize);
    double sum = 0.0;
    long count = 0;
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) midi = first;
        p.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
            {
                const float s = buffer.getSample (ch, i);
                if (! std::isfinite (s)) return -1.0;
                sum += (double) s * s;
                ++count;
            }
    }
    return std::sqrt (sum / (double) juce::jmax (1L, count));
}

//==============================================================================
static void testParameters()
{
    SequencesAudioProcessor p;
    const auto& params = p.getParameters();
    CHECK (params.size() > 250, "nombre de paramètres : %d", params.size());

    juce::StringArray ids;
    for (auto* param : params)
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            ids.add (ranged->getParameterID());
    ids.removeDuplicates (false);
    CHECK (ids.size() == params.size(), "identifiants uniques");

    for (auto* param : params)
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            for (float n : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
            {
                const float real = ranged->convertFrom0to1 (n);
                const auto text = ranged->getText (n, 32);
                CHECK (std::isfinite (real) && ! text.containsIgnoreCase ("nan") && ! text.containsIgnoreCase ("inf "),
                       "%s à %.2f : %s", ranged->getParameterID().toRawUTF8(), n, text.toRawUTF8());
                CHECK (std::abs (ranged->convertTo0to1 (real) - n) < 0.02f || dynamic_cast<juce::AudioParameterFloat*> (ranged) == nullptr,
                       "%s : aller-retour à %.2f", ranged->getParameterID().toRawUTF8(), n);
            }

    // Chaque destination a son potard, dont la position (0..1) est exactement celle du moteur
    const seqs::Settings init;
    for (int d = 1; d < seqs::numDests; ++d)
    {
        const auto id = SequencesAudioProcessor::paramForDest ((seqs::Dest) d);
        auto* param = p.apvts.getParameter (id);
        CHECK (param != nullptr, "potard de la destination %d", d);
        CHECK (SequencesAudioProcessor::destForParam (id) == (seqs::Dest) d, "destination de %s", id.toRawUTF8());
        if (param == nullptr) continue;
        CHECK (std::abs (param->getDefaultValue() - init.base[(size_t) d]) < 1e-4f, "%s : défaut du moteur (%f / %f)",
               id.toRawUTF8(), param->getDefaultValue(), init.base[(size_t) d]);
    }
    CHECK (SequencesAudioProcessor::destForParam ("glide") == seqs::Dest::off, "GLIDE n'est pas une destination");
    CHECK (std::abs (value (p, "cutoff") - 20000.0f) < 1.0f, "coupure grande ouverte au départ");
    CHECK (std::abs (p.apvts.getParameter ("cutoff")->convertFrom0to1 (0.5f) - seqs::DestRange::toReal (seqs::Dest::cutoff, 0.5f)) < 0.5f, "course de la coupure");
    CHECK (std::abs (p.apvts.getParameter ("wtSemi")->convertFrom0to1 (0.75f) - 24.0f) < 1e-3f, "course de SEMI");
    CHECK (p.apvts.getParameter ("wtSemi")->getText (p.apvts.getParameter ("wtSemi")->convertTo0to1 (7.5f), 16) == "+7.50 st", "texte SEMI");
    CHECK (p.apvts.getParameter ("volume")->getCurrentValueAsText() == "0.0 dB", "volume : %s", p.apvts.getParameter ("volume")->getCurrentValueAsText().toRawUTF8());
    CHECK (p.apvts.getParameter ("wtPan")->getCurrentValueAsText() == "C", "pan au centre");
    CHECK (p.apvts.getParameter ("algoModel")->getCurrentValueAsText() == "VIRTUAL ANALOG", "Plaits : VIRTUAL ANALOG");
    CHECK (p.apvts.getParameter ("oversampling")->getCurrentValueAsText() == "2X", "suréchantillonnage 2x");

    seqs::Settings s;
    s.oversampling = 0; s.unison = 0; s.algoModel = 0;
    p.readSettings (s);
    CHECK (s.table != nullptr && s.table->name == "CLASSIC", "table de départ : CLASSIC");
    CHECK (s.oversampling == 2 && s.unison == 1 && s.algoModel == 8, "réglages lus");
    CHECK (std::abs (s.base[(size_t) seqs::Dest::wtLevel] - 0.8f) < 1e-4f, "niveau WT lu");
    CHECK (p.usedSlotCount() == 0, "aucun routage au départ");

    set (p, "fmPdWt", 100.0f);
    set (p, "fmMode", 1.0f);
    set (p, "voiceMode", 2.0f);
    set (p, "mlDest1", (float) seqs::Dest::wtPos);
    set (p, "mlAmt1", -40.0f);
    set (p, "subOct", -2.0f);
    p.readSettings (s);
    CHECK (std::abs (s.fm[1][0] - 1.0f) < 1e-6f && std::abs (s.fm[0][0]) < 1e-6f && s.fmMode == seqs::FmMode::linear, "matrice FM lue");
    CHECK (s.voiceMode == seqs::VoiceMode::legato && s.subOctave == -2, "voix / sub lus");
    CHECK (s.mainLfo.dest[0] == seqs::Dest::wtPos && std::abs (s.mainLfo.amount[0] + 0.4f) < 1e-4f, "MAIN LFO lu");

    seqs::ArpSettings a;
    set (p, "arpOn", 1.0f); set (p, "arpRate", 2.0f); set (p, "arpOct", 3.0f);
    p.readArpSettings (a);
    CHECK (a.on && std::abs (a.stepBeats - 1.0 / 3.0) < 1e-9 && a.octaves == 3, "ARP lu");
    seqs::FxSettings f;
    set (p, "delayOn", 1.0f); set (p, "delayTime", 4.0f); set (p, "reverbMix", 50.0f);
    p.readFxSettings (f);
    CHECK (f.delayOn && std::abs (f.delayBeats - 0.75) < 1e-9 && std::abs (f.reverbMix - 0.5f) < 1e-4f, "FX lus");
}

static void testModulationApi()
{
    SequencesAudioProcessor p;
    const int k = p.addModulation (3, seqs::Dest::wtPos);
    CHECK (k == 0, "premier slot libre : %d", k);
    CHECK (p.getModulationSource (k) == 3 && p.getModulationDest (k) == seqs::Dest::wtPos && std::abs (p.getModulationAmount (k) - 0.25f) < 1e-4f,
           "routage créé");
    CHECK (p.addModulation (3, seqs::Dest::wtPos) == k, "même routage : même slot");
    const int semi = p.addModulation (1, seqs::Dest::pdSemi);
    CHECK (std::abs (p.getModulationAmount (semi) - 0.125f) < 1e-4f, "SEMI : une octave par défaut");
    p.addModulation (seqs::srcMainLfo, seqs::Dest::cutoff);
    p.addModulation (seqs::srcModWheel, seqs::Dest::cutoff);
    CHECK (p.slotsForDest (seqs::Dest::cutoff).size() == 2, "deux routages vers la coupure");

    p.setModulationAmount (k, -0.4f);
    CHECK (std::abs (p.getModulationAmount (k) + 0.4f) < 1e-4f, "profondeur");
    p.setModulationAmount (k, 3.0f);
    CHECK (std::abs (p.getModulationAmount (k) - 1.0f) < 1e-4f, "profondeur bornée");

    p.removeModulation (k);
    CHECK (p.getModulationDest (k) == seqs::Dest::off && p.slotsForDest (seqs::Dest::wtPos).empty(), "supprimé");
    CHECK (p.addModulation (2, seqs::Dest::off) == -1 && p.addModulation (99, seqs::Dest::cutoff) == -1, "routages invalides refusés");

    int last = 0;
    for (int d = 1; d < seqs::numDests && last >= 0; ++d)
        for (int src = 0; src < seqs::numModSources && last >= 0; ++src)
            last = p.addModulation (src, (seqs::Dest) d);
    CHECK (last == -1 && p.usedSlotCount() == seqs::numSlots, "plein : %d", p.usedSlotCount());

    // La modulation agit sur le son : VELO -> WT SEMI +12 (vélocité max) = 220 Hz pour un la 110
    SequencesAudioProcessor q;
    q.setBuiltinTable (seqs::BuiltinTables::indexOf ("SINE"));
    q.setModulationAmount (q.addModulation (seqs::srcVelocity, seqs::Dest::wtSemi), 12.0f / 96.0f);
    q.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (2, 512);
    std::vector<float> out;
    for (int b = 0; b < 40; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 127), 0);
        q.processBlock (buffer, midi);
        out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 512);
    }
    int crossings = 0;
    for (size_t i = 4800; i < out.size(); ++i) crossings += out[i - 1] < 0.0f && out[i] >= 0.0f ? 1 : 0;
    const double hz = crossings * 48000.0 / (double) (out.size() - 4800);
    CHECK (std::abs (hz - 220.0) < 3.0, "VELO -> SEMI : %.1f Hz", hz);
    CHECK (q.getLiveOffset (seqs::Dest::wtSemi) > 0.12f, "décalage montré : %f", q.getLiveOffset (seqs::Dest::wtSemi));

    // Molette (CC 1) -> coupure : le son s'éclaircit
    SequencesAudioProcessor w;
    set (w, "cutoff", 200.0f);
    set (w, "wtPos", 50.0f);
    w.setModulationAmount (w.addModulation (seqs::srcModWheel, seqs::Dest::cutoff), 1.0f);
    w.prepareToPlay (48000.0, 512);
    auto brightness = [&w] (juce::MidiBuffer first)
    {
        juce::AudioBuffer<float> b (2, 512);
        double d = 0.0, e = 0.0;
        float prev = 0.0f;
        for (int blk = 0; blk < 20; ++blk)
        {
            juce::MidiBuffer midi;
            if (blk == 0) midi = first;
            w.processBlock (b, midi);
            if (blk < 4) continue;
            for (int i = 0; i < 512; ++i) { const float x = b.getSample (0, i); d += (x - prev) * (x - prev); e += x * x; prev = x; }
        }
        return std::sqrt (d / (e + 1e-20));
    };
    juce::MidiBuffer note;
    note.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
    const double closed = brightness (note);
    juce::MidiBuffer wheel;
    wheel.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 0);
    const double open = brightness (wheel);
    CHECK (open > 2.0 * closed, "molette ouvre le filtre (%.4f / %.4f)", closed, open);
}
static void testAudio()
{
    SequencesAudioProcessor p;
    FakePlayHead head;
    head.bpm = 190.0;
    p.setPlayHead (&head);

    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        p.setRateAndBufferSizeDetails (sr, 256);
        p.prepareToPlay (sr, 256);
        CHECK (renderRms (p, 20, 256) < 1e-9, "silence sans note (sr %.0f)", sr);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 110), 17);
        const double r = renderRms (p, 200, 256, midi);
        CHECK (r > 0.01 && r < 2.0, "note jouée (sr %.0f) : rms %.4f", sr, r);

        juce::MidiBuffer off;
        off.addEvent (juce::MidiMessage::noteOff (1, 45), 0);
        renderRms (p, 1, 256, off);
        CHECK (renderRms (p, (int) (sr / 256.0), 256) < 0.01, "relâché (sr %.0f)", sr);
        p.reset();
    }

    // Mono (1 canal de sortie)
    {
        SequencesAudioProcessor m;
        m.setPlayHead (&head);
        juce::AudioProcessor::BusesLayout layout;
        layout.outputBuses.add (juce::AudioChannelSet::mono());
        CHECK (m.setBusesLayout (layout), "sortie mono acceptée");
        m.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (1, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
        double sum = 0.0;
        for (int b = 0; b < 40; ++b)
        {
            m.processBlock (buffer, midi);
            midi.clear();
            for (int i = 0; i < 512; ++i) sum += std::abs (buffer.getSample (0, i));
        }
        CHECK (sum > 1.0, "mono : du son");
    }

    // Tête de lecture du séquenceur
    {
        SequencesAudioProcessor s;
        head.playing = true;
        head.ppq = 2.0;
        s.setPlayHead (&head);
        set (s, "seqSync", 1.0f);   // HOST
        set (s, "seqDiv", 4.0f);    // 1/16
        s.prepareToPlay (48000.0, 128);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        juce::AudioBuffer<float> buffer (2, 128);
        s.processBlock (buffer, midi);
        CHECK (s.getPlayheadStep() == 8, "HOST : pas 8 à la noire 2 : %d", s.getPlayheadStep());
        CHECK (s.getPlayheadBeats() >= 2.0 && s.getPlayheadBeats() < 2.1, "position : %f", s.getPlayheadBeats());
        head.playing = false;
        head.ppq = 0.0;
    }
}

static int stepSource (SequencesAudioProcessor& p, int i)   { return juce::roundToInt (value (p, SequencesAudioProcessor::stepId (i))); }

static void testRandomAndTools()
{
    SequencesAudioProcessor p;
    p.setRandomSeed (42);

    std::vector<int> before;
    for (int i = 0; i < 32; ++i) before.push_back (stepSource (p, i));

    p.randomizePattern();
    int changed = 0;
    for (int i = 0; i < 32; ++i)
    {
        const int v = stepSource (p, i);
        CHECK (v >= 0 && v <= seqs::numSources, "source valide");
        changed += v != before[(size_t) i] ? 1 : 0;
    }
    CHECK (changed > 6, "RANDOM change le motif : %d pas", changed);

    // Une source à quantité nulle n'est jamais tirée
    for (int i = 0; i < seqs::numLfos; ++i)
        set (p, SequencesAudioProcessor::lfoId (i, "Amt"), i == 1 || i == 4 ? 100.0f : 0.0f);
    for (int c = 0; c < seqs::numChaos; ++c)
        set (p, SequencesAudioProcessor::chaosId (c, "Amt"), 0.0f);
    bool onlyAudible = true;
    for (int run = 0; run < 30; ++run)
    {
        p.randomizePattern();
        for (int i = 0; i < 32; ++i)
        {
            const int v = stepSource (p, i);
            onlyAudible = onlyAudible && (v == 0 || v == 2 || v == 5);
        }
    }
    CHECK (onlyAudible, "RANDOM ne tire que les sources audibles");

    // Séquenceur éteint et rien ne l'écoute : RANDOM l'allume et le branche sur la coupure
    set (p, "seqOn", 0.0f);
    for (int k = 0; k < seqs::numSlots; ++k)
        if (p.getModulationSource (k) == seqs::srcSeq)
            p.removeModulation (k);
    p.randomizePattern();
    CHECK (value (p, "seqOn") > 0.5f, "RANDOM allume le séquenceur");
    bool listening = false;
    for (int k = 0; k < seqs::numSlots; ++k)
        listening = listening || (p.getModulationSource (k) == seqs::srcSeq && p.getModulationDest (k) == seqs::Dest::cutoff);
    CHECK (listening, "RANDOM branche SEQ sur la coupure");

    // RANDOM ALL change la banque
    std::vector<float> bank;
    for (int i = 0; i < seqs::numLfos; ++i)
        for (const char* n : { "Rate", "Shape", "Step" })
            bank.push_back (value (p, SequencesAudioProcessor::lfoId (i, n)));
    p.randomizeAll();
    int bankChanged = 0;
    size_t k = 0;
    for (int i = 0; i < seqs::numLfos; ++i)
        for (const char* n : { "Rate", "Shape", "Step" })
            bankChanged += std::abs (value (p, SequencesAudioProcessor::lfoId (i, n)) - bank[k++]) > 1e-3f ? 1 : 0;
    CHECK (bankChanged > 6, "RANDOM ALL change la banque : %d", bankChanged);

    // Décalage dans la longueur choisie
    set (p, "seqSteps", 4.0f);
    for (int i = 0; i < 4; ++i) set (p, SequencesAudioProcessor::stepId (i), (float) (i + 1));
    const int outside = stepSource (p, 10);
    p.rotatePattern (+1);
    CHECK (stepSource (p, 0) == 4 && stepSource (p, 1) == 1 && stepSource (p, 3) == 3, "décalage à droite");
    CHECK (stepSource (p, 10) == outside, "hors de la boucle, rien ne bouge");
    p.rotatePattern (-1);
    CHECK (stepSource (p, 0) == 1 && stepSource (p, 3) == 4, "décalage aller-retour");

    p.fillPattern (0);
    bool allOff = true;
    for (int i = 0; i < 32; ++i) allOff = allOff && stepSource (p, i) == 0;
    CHECK (allOff, "CLEAR");
}

static juce::File writeTestTable (const juce::File& dir, const juce::String& name, int frames, int frameSize = 2048)
{
    std::vector<std::vector<float>> cycles ((size_t) frames, std::vector<float> ((size_t) frameSize));
    for (int f = 0; f < frames; ++f)
        for (int i = 0; i < frameSize; ++i)
            cycles[(size_t) f][(size_t) i] = 0.7f * (float) std::sin (juce::MathConstants<double>::twoPi * (f + 1) * i / frameSize);
    const auto bytes = seqs::WavetableFile::writeSerum (cycles, frameSize);
    auto file = dir.getChildFile (name + ".wav");
    file.replaceWithData (bytes.data(), bytes.size());
    return file;
}

static void testWavetables (const juce::StringArray& serumFiles)
{
    auto dir = juce::File::createTempFile ("seqtables");
    dir.createDirectory();

    SequencesAudioProcessor p;
    CHECK (p.getTableInfo().name == "CLASSIC" && p.getTableInfo().builtin == 0, "table intégrée au départ");

    p.setBuiltinTable (seqs::BuiltinTables::indexOf ("RESO SAW"));
    CHECK (p.getTableInfo().name == "RESO SAW" && p.getTable()->numFrames == 64, "table intégrée choisie");

    const auto file = writeTestTable (dir, "My Table", 12);
    CHECK (p.loadTableFile (file).isEmpty(), "import WAV");
    CHECK (p.getTableInfo().name == "MY TABLE" && p.getTableInfo().builtin == -1 && p.getTable()->numFrames == 12, "table importée");
    CHECK (p.getTableInfo().file == file, "fichier d'origine retenu");
    CHECK (p.loadTableFile (dir.getChildFile ("absent.wav")).isNotEmpty(), "fichier absent : erreur");
    auto junk = dir.getChildFile ("junk.wav");
    junk.replaceWithText ("pas un wav");
    CHECK (p.loadTableFile (junk).isNotEmpty(), "fichier invalide : erreur");
    CHECK (p.getTableInfo().name == "MY TABLE", "une erreur ne change pas la table");

    set (p, "wtPos", 100.0f * 5.0f / 11.0f);
    p.prepareToPlay (48000.0, 512);
    juce::MidiBuffer on;
    on.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
    CHECK (renderRms (p, 40, 512, on) > 0.02, "table importée jouée");

    juce::MemoryBlock data;
    p.getStateInformation (data);
    file.deleteFile();
    SequencesAudioProcessor q;
    q.setStateInformation (data.getData(), (int) data.getSize());
    CHECK (q.getTableInfo().name == "MY TABLE" && q.getTable()->numFrames == 12, "table importée relue depuis le projet");
    {
        float maxDiff = 0.0f;
        for (double ph = 0.0; ph < 1.0; ph += 0.01)
            maxDiff = std::max (maxDiff, std::abs (p.getTable()->read (5.0, 0.0, ph) - q.getTable()->read (5.0, 0.0, ph)));
        CHECK (maxDiff < 2e-4f, "contenu identique (16 bits) : %f", maxDiff);
    }
    CHECK (data.getSize() < 200000, "état compact : %d octets", (int) data.getSize());

    p.presets.setFolderOverride (dir);
    CHECK (p.presets.save ("With Table"), "preset enregistré");
    p.presets.loadFactory (0);
    CHECK (p.getTableInfo().name == "CLASSIC", "INIT : table intégrée");
    CHECK (p.presets.load (dir.getChildFile ("With Table.seqpreset")), "preset rechargé");
    CHECK (p.getTableInfo().name == "MY TABLE", "table du preset : %s", p.getTableInfo().name.toRawUTF8());

    {
        auto xml = juce::parseXML ("<Sequences><TABLES><TABLE osc=\"0\" builtin=\"-1\" name=\"X\" data=\"@@@notbase64\"/></TABLES></Sequences>");
        juce::MemoryBlock bad;
        juce::AudioProcessor::copyXmlToBinary (*xml, bad);
        q.setStateInformation (bad.getData(), (int) bad.getSize());
        CHECK (q.getTableInfo().name == "CLASSIC", "données abîmées : table par défaut");
    }

    for (const auto& path : serumFiles)
    {
        const juce::File f (path);
        if (! f.existsAsFile()) continue;
        const auto err = p.loadTableFile (f);
        CHECK (err.isEmpty() && p.getTable()->numFrames > 0, "Serum : %s (%s)", f.getFileName().toRawUTF8(), err.toRawUTF8());
    }

    dir.deleteRecursively();
}

static void testState()
{
    SequencesAudioProcessor a;
    a.setRandomSeed (7);
    a.randomizeAll();
    set (a, "cutoff", 2500.0f);
    set (a, "wtSemi", 7.25f);
    set (a, "voiceMode", 1.0f);
    set (a, "algoModel", 13.0f);
    set (a, "fmNoisePd", 40.0f);
    set (a, "arpOn", 1.0f);
    set (a, "reverbOn", 1.0f);
    a.addModulation (5, seqs::Dest::algoPan);
    a.apvts.state.setProperty ("editorWidth", 990, nullptr);

    juce::MemoryBlock data;
    a.getStateInformation (data);

    SequencesAudioProcessor b;
    b.setStateInformation (data.getData(), (int) data.getSize());
    bool same = true;
    for (int i = 0; i < a.getParameters().size(); ++i)
        same = same && std::abs (a.getParameters()[i]->getValue() - b.getParameters()[i]->getValue()) < 1e-6f;
    CHECK (same, "état relu à l'identique");
    CHECK ((int) b.apvts.state.getProperty ("editorWidth") == 990, "taille de fenêtre gardée");
    CHECK (! b.apvts.state.getChildWithName ("TABLES").isValid(), "les tables ne restent pas dans l'arbre des paramètres");

    const char junk[] = "pas un état";
    b.setStateInformation (junk, (int) sizeof (junk));
    CHECK (std::abs (value (b, "cutoff") - 2500.0f) < 1.0f, "état abîmé ignoré");
}

static void testPresets()
{
    auto dir = juce::File::createTempFile ("seqpresets");
    dir.createDirectory();

    SequencesAudioProcessor p;
    p.presets.setFolderOverride (dir);
    CHECK (p.presets.getDisplayName() == "INIT", "nom au départ : %s", p.presets.getDisplayName().toRawUTF8());

    set (p, "pdSemi", -5.5f);
    set (p, SequencesAudioProcessor::stepId (5), 7.0f);
    CHECK (p.presets.isModified(), "modifié");
    CHECK (p.presets.save ("Hitech Seq"), "enregistré");
    CHECK (! p.presets.isModified(), "plus modifié après SAVE");

    p.presets.loadFactory (0);
    CHECK (std::abs (value (p, "pdSemi")) < 1e-3f, "INIT remet les valeurs");

    CHECK (p.presets.load (dir.getChildFile ("Hitech Seq.seqpreset")), "rechargé");
    CHECK (std::abs (value (p, "pdSemi") + 5.5f) < 1e-3f, "valeur du preset");
    CHECK (stepSource (p, 5) == 7, "pas du preset");

    // Presets d'usine : chacun se charge, se reconnaît, et sonne
    CHECK (SequencesAudioProcessor::numFactoryPresets() >= 8, "%d presets d'usine", SequencesAudioProcessor::numFactoryPresets());
    for (int i = 0; i < SequencesAudioProcessor::numFactoryPresets(); ++i)
    {
        SequencesAudioProcessor f;
        f.presets.setFolderOverride (dir);
        f.presets.loadFactory (i);
        const auto name = SequencesAudioProcessor::factoryPresetName (i);
        CHECK (f.presets.getDisplayName() == name && ! f.presets.isModified(), "preset %s chargé (%s)", name.toRawUTF8(), f.presets.getDisplayName().toRawUTF8());
        FakePlayHead head;
        head.bpm = 145.0;
        f.setPlayHead (&head);
        f.prepareToPlay (48000.0, 256);
        juce::MidiBuffer on;
        on.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 110), 0);
        on.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 110), 0);
        const double r = renderRms (f, 200, 256, on);
        CHECK (r > 0.01 && r < 1.5, "preset %s sonne : rms %.4f", name.toRawUTF8(), r);
    }
    {
        SequencesAudioProcessor f;
        f.presets.loadFactory (1);
        CHECK (f.usedSlotCount() == 3 && f.getModulationSource (0) == seqs::srcSeq, "HITECH ZAPS : routages");
        f.presets.loadFactory (0);
        CHECK (f.usedSlotCount() == 0, "INIT : plus de routages");
    }

    dir.deleteRecursively();
}

static void testArpAndFx()
{
    // ARP : un accord tenu donne des notes successives (le son change de hauteur à chaque pas)
    SequencesAudioProcessor p;
    FakePlayHead head;
    head.bpm = 120.0;
    p.setPlayHead (&head);
    p.setBuiltinTable (seqs::BuiltinTables::indexOf ("SINE"));
    set (p, "arpOn", 1.0f);
    set (p, "arpRate", 3.0f);   // 1/16 = 6000 échantillons
    set (p, "arpGate", 50.0f);
    set (p, "ampAttack", 1.0f);
    set (p, "ampRelease", 5.0f);
    p.prepareToPlay (48000.0, 500);
    juce::MidiBuffer chord;
    chord.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
    chord.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0);
    juce::AudioBuffer<float> buffer (2, 500);
    std::vector<float> out;
    for (int b = 0; b < 48; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) midi = chord;
        p.processBlock (buffer, midi);
        out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 500);
    }
    auto rmsOf = [&out] (int from, int to) { double s = 0.0; for (int i = from; i < to; ++i) s += out[(size_t) i] * out[(size_t) i]; return std::sqrt (s / (to - from)); };
    auto zeroCrossings = [&out] (int from, int to) { int n = 0; for (int i = from + 1; i < to; ++i) n += out[(size_t) i - 1] < 0.0f && out[(size_t) i] >= 0.0f; return n; };
    CHECK (rmsOf (500, 2500) > 0.05, "ARP : 1re note");
    CHECK (rmsOf (3600, 5800) < 0.01, "ARP : silence après la porte (50 %%) : %.4f", rmsOf (3600, 5800));
    CHECK (rmsOf (6500, 8500) > 0.05, "ARP : 2e note");
    const int z1 = zeroCrossings (500, 2500), z2 = zeroCrossings (6500, 8500);
    CHECK (z2 > (int) (1.8 * z1), "ARP : la 2e note est une octave plus haut (%d / %d)", z1, z2);
    CHECK (p.getActiveVoices() <= 2, "ARP : une note à la fois");

    // Couper l'ARP : plus rien ne joue de lui-même
    set (p, "arpOn", 0.0f);
    juce::MidiBuffer release;
    release.addEvent (juce::MidiMessage::noteOff (1, 57), 0);
    release.addEvent (juce::MidiMessage::noteOff (1, 69), 0);
    renderRms (p, 4, 500, release);
    CHECK (renderRms (p, 40, 500) < 1e-4, "ARP coupé : silence");

    // FX : la reverb prolonge une note courte
    SequencesAudioProcessor dry, wet;
    for (auto* q : { &dry, &wet })
    {
        q->setPlayHead (&head);
        set (*q, "ampRelease", 5.0f);
        q->prepareToPlay (48000.0, 512);
    }
    set (wet, "reverbOn", 1.0f);
    set (wet, "reverbMix", 40.0f);
    set (wet, "reverbDecay", 70.0f);
    wet.prepareToPlay (48000.0, 512);
    for (auto* q : { &dry, &wet })
    {
        juce::MidiBuffer note;
        note.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
        note.addEvent (juce::MidiMessage::noteOff (1, 57), 400);
        renderRms (*q, 1, 512, note);
        renderRms (*q, 20, 512);
    }
    const double tailDry = renderRms (dry, 20, 512), tailWet = renderRms (wet, 20, 512);
    CHECK (tailDry < 1e-4 && tailWet > 1e-3, "REVERB : queue (%.5f / %.5f)", tailDry, tailWet);
}
static juce::MouseEvent mouseAt (juce::Component& c, juce::Point<float> pt, juce::ModifierKeys mods = {},
                                 std::optional<juce::Point<float>> downAt = {})
{
    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pt, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             &c, &c, juce::Time::getCurrentTime(), downAt.value_or (pt), juce::Time::getCurrentTime(), 1, false);
}

static void testEditor()
{
    juce::ScopedJuceInitialiser_GUI gui;
    SequencesAudioProcessor p;
    p.prepareToPlay (48000.0, 256);

    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    CHECK (editor != nullptr, "éditeur créé");
    CHECK (editor->getWidth() == juce::roundToInt (SequencesFacade::width * 0.9), "taille par défaut : %d", editor->getWidth());

    auto* e = dynamic_cast<SequencesAudioProcessorEditor*> (editor.get());
    e->setUiScale (0.75f);
    CHECK (editor->getWidth() == 990, "75 %% : %d", editor->getWidth());
    auto& facade = e->getFacade();

    // Un potard modulable pour chaque destination, une pastille pour chaque source
    for (int d = 1; d < seqs::numDests; ++d)
        CHECK (facade.getModKnob ((seqs::Dest) d) != nullptr, "potard modulable %s", SequencesAudioProcessor::destinationNames()[d].toRawUTF8());
    for (int s = 0; s < seqs::numModSources; ++s)
        CHECK (facade.getChip (s) != nullptr, "pastille %s", SequencesAudioProcessor::modSourceNames()[s].toRawUTF8());

    // Glisser-déposer : LFO 4 sur WT POS
    auto* knob = facade.getModKnob (seqs::Dest::wtPos);
    auto* chip = facade.getChip (4);
    juce::DragAndDropTarget::SourceDetails details (ui::SourceChip::dragDescription (4), chip, {});
    CHECK (knob->isInterestedInDragSource (details), "le potard accepte une source");
    CHECK (! knob->isInterestedInDragSource (juce::DragAndDropTarget::SourceDetails ("autre chose", nullptr, {})), "et refuse le reste");
    knob->itemDropped (details);
    const auto slots = p.slotsForDest (seqs::Dest::wtPos);
    CHECK (slots.size() == 1 && p.getModulationSource (slots[0]) == 4, "LFO 4 -> WT POS créé");
    CHECK (knob->getActiveSlot() == slots[0], "slot actif");

    // Régler la profondeur en tirant sur l'anneau
    const auto ringArea = knob->getLookAndFeel().getSliderLayout (*knob).sliderBounds.toFloat().reduced (4.0f);
    const float radius = juce::jmin (ringArea.getWidth(), ringArea.getHeight()) * 0.5f;
    const auto ringPoint = ringArea.getCentre().translated (0.0f, -radius);
    CHECK (knob->isOnRing (ringPoint) && ! knob->isOnRing (ringArea.getCentre()), "anneau repéré");
    const float valueBefore = (float) knob->getValue();
    knob->mouseDown (mouseAt (*knob, ringPoint));
    knob->mouseDrag (mouseAt (*knob, ringPoint.translated (0.0f, -50.0f), {}, ringPoint));
    knob->mouseUp (mouseAt (*knob, ringPoint.translated (0.0f, -50.0f), {}, ringPoint));
    CHECK (std::abs (p.getModulationAmount (slots[0]) - 0.5f) < 0.02f, "profondeur réglée sur l'anneau : %f", p.getModulationAmount (slots[0]));
    CHECK (std::abs ((float) knob->getValue() - valueBefore) < 1e-6f, "la valeur du potard ne bouge pas");

    // Liste : la ligne apparaît, ×, profondeur
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    auto& list = facade.getModList();
    const auto rows = list.rows();
    CHECK ((int) rows.size() == p.usedSlotCount() && rows.size() == 1, "liste : %d lignes", (int) rows.size());
    const int row = (int) (std::find (rows.begin(), rows.end(), slots[0]) - rows.begin());
    const float y = (float) (row * ui::ModList::rowHeight) + ui::ModList::rowHeight * 0.5f;
    const float barLeft = 222.0f, barRight = (float) list.getWidth() - 26.0f;
    list.mouseDown (mouseAt (list, { barLeft + (barRight - barLeft) * 0.25f, y }));
    list.mouseUp (mouseAt (list, { barLeft + (barRight - barLeft) * 0.25f, y }));
    CHECK (std::abs (p.getModulationAmount (slots[0]) + 0.5f) < 0.02f, "profondeur depuis la liste : %f", p.getModulationAmount (slots[0]));
    list.mouseDown (mouseAt (list, { (float) list.getWidth() - 10.0f, y }));
    CHECK (p.slotsForDest (seqs::Dest::wtPos).empty(), "× supprime le routage");

    // RANDOM
    p.setRandomSeed (99);
    std::vector<int> before;
    for (int i = 0; i < 32; ++i) before.push_back (stepSource (p, i));
    facade.getRandomButton().triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    int changedSteps = 0;
    for (int i = 0; i < 32; ++i) changedSteps += stepSource (p, i) != before[(size_t) i] ? 1 : 0;
    CHECK (changedSteps > 6, "bouton RANDOM : %d pas changés", changedSteps);

    // Matrice
    auto& m = facade.getMatrix();
    auto click = [&m] (int column, int rowIndex, bool right = false)
    {
        const float cw = (float) (m.getWidth() - ui::StepMatrix::labelWidth) / 32.0f;
        const float rh = (float) m.getHeight() / (float) ui::StepMatrix::numRows;
        const juce::Point<float> pt ((float) ui::StepMatrix::labelWidth + cw * ((float) column + 0.5f), rh * ((float) rowIndex + 0.5f));
        const auto ev = mouseAt (m, pt, right ? juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier) : juce::ModifierKeys());
        m.mouseDown (ev);
        m.mouseUp (ev);
    };
    click (6, ui::StepMatrix::rowForSource (8));
    CHECK (stepSource (p, 6) == 8, "matrice : CHAOS 2");
    click (6, 0, true);
    CHECK (stepSource (p, 6) == 0, "clic droit : OFF");

    // Wavetable : fichier déposé depuis le Finder
    {
        auto dir = juce::File::createTempFile ("seqdrop");
        dir.createDirectory();
        const auto f1 = writeTestTable (dir, "A first", 5);
        writeTestTable (dir, "B second", 9);
        auto& view = facade.getTableView();
        CHECK (view.isInterestedInFileDrag ({ f1.getFullPathName() }) && ! view.isInterestedInFileDrag ({ "/x/y.png" }), "accepte les .wav");
        view.filesDropped ({ f1.getFullPathName() }, 10, 10);
        CHECK (p.getTableInfo().name == "A FIRST" && view.getError().isEmpty(), "table déposée");
        view.step (+1);
        CHECK (p.getTableInfo().name == "B SECOND", "table suivante du dossier");
        view.step (+1);
        CHECK (p.getTableInfo().name == "A FIRST", "en boucle");
        p.setBuiltinTable (0);
        view.step (-1);
        CHECK (p.getTableInfo().name == juce::String (seqs::BuiltinTables::names().back()), "intégrées en boucle");
        auto bad = dir.getChildFile ("bad.wav");
        bad.replaceWithText ("x");
        view.filesDropped ({ bad.getFullPathName() }, 10, 10);
        CHECK (view.getError().isNotEmpty(), "erreur affichée");
        dir.deleteRecursively();
    }

    // Matrice FM : clic = tout ou rien, glisser = quantité, double-clic = zéro
    {
        auto* cell = facade.getFmCell (1, 0);
        CHECK (cell != nullptr && facade.getFmCell (2, 1) != nullptr, "cases FM");
        const juce::Point<float> c (10.0f, 10.0f);
        cell->mouseDown (mouseAt (*cell, c));
        cell->mouseUp (mouseAt (*cell, c));
        CHECK (std::abs (value (p, "fmPdWt") - 100.0f) < 0.01f, "clic : 100 %%");
        cell->mouseDown (mouseAt (*cell, c));
        cell->mouseDrag (mouseAt (*cell, c.translated (0.0f, 75.0f), {}, c));
        cell->mouseUp (mouseAt (*cell, c.translated (0.0f, 75.0f), {}, c));
        CHECK (std::abs (value (p, "fmPdWt") - 50.0f) < 1.0f, "glisser : %.1f", value (p, "fmPdWt"));
        cell->mouseDoubleClick (mouseAt (*cell, c));
        CHECK (std::abs (value (p, "fmPdWt")) < 1e-6f, "double-clic : 0");
    }

    // Onglets : une page visible à la fois, chaque page a ses contrôles
    for (int t = 0; t < SequencesFacade::numTabs; ++t)
    {
        facade.getTabButton (t).triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        int visible = 0;
        for (int u = 0; u < SequencesFacade::numTabs; ++u) visible += facade.getPage (u).isVisible() ? 1 : 0;
        CHECK (facade.getTab() == t && visible == 1 && facade.getPage (t).isVisible(), "onglet %d", t);
        CHECK (facade.getPage (t).getNumChildComponents() > 1, "page %d remplie", t);
    }
    for (const char* id : { "arpOn", "arpMode", "delayFeedback", "reverbMix", "chorusMix", "algoModel", "pdWave", "mlDest1", "oversampling", "filterModel" })
        CHECK (facade.findById (id) != nullptr, "contrôle %s présent", id);

    // Aucun contrôle ne déborde de sa page, ni ne chevauche un voisin
    for (int t = 0; t < SequencesFacade::numTabs; ++t)
    {
        auto& page = facade.getPage (t);
        std::vector<juce::Rectangle<int>> boxes;
        for (auto* c : page.getChildren())
        {
            CHECK (page.getLocalBounds().contains (c->getBounds()), "page %d : %s dépasse", t, c->getComponentID().toRawUTF8());
            if (dynamic_cast<juce::Slider*> (c) != nullptr || dynamic_cast<juce::ComboBox*> (c) != nullptr || dynamic_cast<juce::Button*> (c) != nullptr)
            {
                for (auto& b : boxes)
                    CHECK (! b.intersects (c->getBounds()), "page %d : %s chevauche", t, c->getComponentID().toRawUTF8());
                boxes.push_back (c->getBounds());
            }
        }
    }

    // Rendu de la façade
    juce::Image image (juce::Image::ARGB, SequencesFacade::width, SequencesFacade::height, true);
    juce::Graphics g (image);
    facade.paintEntireComponent (g, false);
    CHECK (image.getPixelAt (5, 5).getAlpha() > 0, "façade dessinée");

    editor.reset();
}

int main (int argc, char** argv)
{
    juce::StringArray serum;
    for (int i = 1; i < argc; ++i) serum.add (argv[i]);

    testParameters();
    testModulationApi();
    testAudio();
    testRandomAndTools();
    testWavetables (serum);
    testState();
    testPresets();
    testArpAndFx();
    testEditor();

    std::printf ("%d vérifications, %d échecs\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
