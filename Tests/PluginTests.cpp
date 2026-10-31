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
    CHECK (params.size() == 168, "nombre de paramètres : %d", params.size());

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

    // Chaque destination a son potard, avec exactement la course du moteur
    for (int d = 1; d < seqs::numDests; ++d)
    {
        const auto id = SequencesAudioProcessor::paramForDest ((seqs::Dest) d);
        auto* param = p.apvts.getParameter (id);
        CHECK (param != nullptr, "potard de la destination %d", d);
        CHECK (SequencesAudioProcessor::destForParam (id) == (seqs::Dest) d, "destination de %s", id.toRawUTF8());
        if (param == nullptr) continue;
        for (float n : { 0.0f, 0.3f, 0.71f, 1.0f })
        {
            float real = param->convertFrom0to1 (n);
            float engine = seqs::DestRange::toReal ((seqs::Dest) d, n);
            if (id == "pan") real *= 0.01f;
            if (id.endsWith ("Level") || id.endsWith ("Pos") || id.endsWith ("Warp") || id == "noise" || id == "resonance" || id == "drive")
                real *= 0.01f;
            if (id == "volume") real = engine;   // le paramètre est la position du potard, le texte montre les dB
            CHECK (std::abs (real - engine) <= 1e-3f * std::max (1.0f, std::abs (engine)), "%s à %.2f : %f / %f", id.toRawUTF8(), n, real, engine);
        }
    }
    CHECK (SequencesAudioProcessor::destForParam ("glide") == seqs::Dest::off, "GLIDE n'est pas une destination");

    CHECK (p.apvts.getParameter ("aCoarse")->getText (p.apvts.getParameter ("aCoarse")->convertTo0to1 (7.5f), 16) == "+7.50 st", "texte COARSE : %s",
           p.apvts.getParameter ("aCoarse")->getText (p.apvts.getParameter ("aCoarse")->convertTo0to1 (7.5f), 16).toRawUTF8());
    CHECK (p.apvts.getParameter ("aCoarse")->getText (p.apvts.getParameter ("aCoarse")->convertTo0to1 (-12.0f), 16) == "-12 st", "texte COARSE entier");
    CHECK (p.apvts.getParameter ("volume")->getCurrentValueAsText() == "-3.0 dB", "volume : %s", p.apvts.getParameter ("volume")->getCurrentValueAsText().toRawUTF8());
    CHECK (p.apvts.getParameter ("pan")->getCurrentValueAsText() == "C", "pan au centre");
    CHECK (p.apvts.getParameter ("aWarpMode")->getCurrentValueAsText() == "FM (FROM B)", "A : FM depuis B");
    CHECK (p.apvts.getParameter ("bWarpMode")->getText (p.apvts.getParameter ("bWarpMode")->convertTo0to1 (2.0f), 32) == "FM (FROM A)", "B : FM depuis A");

    seqs::Settings s;
    p.readSettings (s);
    CHECK (s.osc[0].warp == seqs::Warp::fm && s.osc[1].octave == 1, "réglages des oscillateurs lus");
    CHECK (s.osc[0].table != nullptr && s.osc[0].table->name == "BASIC SHAPES", "table de départ");
    CHECK (std::abs (s.base[(size_t) seqs::Dest::aLevel] - 0.8f) < 1e-4f, "niveau A lu");
    CHECK (s.slotSource[0] == seqs::srcSeq && s.slotDest[0] == seqs::Dest::cutoff && std::abs (s.slotAmount[0] - 0.27f) < 1e-4f, "routage de départ");
    CHECK (s.slotSource[2] == seqs::srcEnv2 && s.slotDest[3] == seqs::Dest::aWarp, "ENV 2 de départ");
    CHECK (p.usedSlotCount() == 4, "4 routages au départ");
}

static void testModulationApi()
{
    SequencesAudioProcessor p;
    const int k = p.addModulation (3, seqs::Dest::bPos);
    CHECK (k == 4, "premier slot libre : %d", k);
    CHECK (p.getModulationSource (k) == 3 && p.getModulationDest (k) == seqs::Dest::bPos && std::abs (p.getModulationAmount (k) - 0.25f) < 1e-4f,
           "routage créé");
    CHECK (p.addModulation (3, seqs::Dest::bPos) == k, "même routage : même slot");
    const int coarse = p.addModulation (1, seqs::Dest::aCoarse);
    CHECK (std::abs (p.getModulationAmount (coarse) - 0.125f) < 1e-4f, "COARSE : une octave par défaut");
    CHECK (p.slotsForDest (seqs::Dest::cutoff).size() == 2, "deux routages vers la coupure");

    p.setModulationAmount (k, -0.4f);
    CHECK (std::abs (p.getModulationAmount (k) + 0.4f) < 1e-4f, "profondeur");
    p.setModulationAmount (k, 3.0f);
    CHECK (std::abs (p.getModulationAmount (k) - 1.0f) < 1e-4f, "profondeur bornée");

    p.removeModulation (k);
    CHECK (p.getModulationDest (k) == seqs::Dest::off && p.slotsForDest (seqs::Dest::bPos).empty(), "supprimé");
    CHECK (p.addModulation (2, seqs::Dest::off) == -1 && p.addModulation (99, seqs::Dest::cutoff) == -1, "routages invalides refusés");

    // 16 slots au plus
    int last = 0;
    for (int d = 1; d < seqs::numDests && last >= 0; ++d)
        for (int src = 0; src < seqs::numModSources && last >= 0; ++src)
            last = p.addModulation (src, (seqs::Dest) d);
    CHECK (last == -1 && p.usedSlotCount() == 16, "plein : %d", p.usedSlotCount());

    // La modulation agit sur le son : VELO -> PITCH +12 (vélocité max)
    SequencesAudioProcessor q;
    for (int s = 0; s < seqs::numSlots; ++s) q.removeModulation (s);
    set (q, "aWarp", 0.0f);
    set (q, "aLevel", 100.0f);
    set (q, "cutoff", 20000.0f);
    set (q, "resonance", 0.0f);
    q.setModulationAmount (q.addModulation (seqs::srcVelocity, seqs::Dest::pitch), 12.0f / 48.0f);
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
    CHECK (std::abs (hz - 220.0) < 3.0, "VELO -> PITCH : %.1f Hz", hz);
    CHECK (q.getLiveOffset (seqs::Dest::pitch) > 0.24f, "décalage montré : %f", q.getLiveOffset (seqs::Dest::pitch));
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
        CHECK (renderRms (p, 20, 256) == 0.0, "silence sans note (sr %.0f)", sr);

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
    CHECK (value (p, "seqOn") == 1.0f, "RANDOM allume le séquenceur");
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
    CHECK (p.getTableInfo (0).name == "BASIC SHAPES" && p.getTableInfo (0).builtin == 0, "table intégrée au départ");

    p.setBuiltinTable (1, seqs::BuiltinTables::indexOf ("RESO SAW"));
    CHECK (p.getTableInfo (1).name == "RESO SAW" && p.getTable (1)->numFrames == 64, "table intégrée choisie");

    const auto file = writeTestTable (dir, "My Table", 12);
    CHECK (p.loadTableFile (0, file).isEmpty(), "import WAV");
    CHECK (p.getTableInfo (0).name == "MY TABLE" && p.getTableInfo (0).builtin == -1 && p.getTable (0)->numFrames == 12, "table importée");
    CHECK (p.getTableInfo (0).file == file, "fichier d'origine retenu");

    CHECK (p.loadTableFile (0, dir.getChildFile ("absent.wav")).isNotEmpty(), "fichier absent : erreur");
    auto junk = dir.getChildFile ("junk.wav");
    junk.replaceWithText ("pas un wav");
    CHECK (p.loadTableFile (0, junk).isNotEmpty(), "fichier invalide : erreur");
    CHECK (p.getTableInfo (0).name == "MY TABLE", "une erreur ne change pas la table");

    // La table importée joue (frame 6, sinus x6) : son présent
    set (p, "aPos", 100.0f * 5.0f / 11.0f);
    set (p, "aWarp", 0.0f);
    set (p, "cutoff", 20000.0f);
    p.prepareToPlay (48000.0, 512);
    juce::MidiBuffer on;
    on.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
    CHECK (renderRms (p, 40, 512, on) > 0.02, "table importée jouée");

    // Projet : la table importée est enregistrée dans l'état, même si le fichier disparaît
    juce::MemoryBlock data;
    p.getStateInformation (data);
    file.deleteFile();
    SequencesAudioProcessor q;
    q.setStateInformation (data.getData(), (int) data.getSize());
    CHECK (q.getTableInfo (0).name == "MY TABLE" && q.getTable (0)->numFrames == 12, "table importée relue depuis le projet");
    CHECK (q.getTableInfo (1).name == "RESO SAW", "table intégrée relue depuis le projet");
    {
        const auto* a = p.getTable (0);
        const auto* b = q.getTable (0);
        float maxDiff = 0.0f;
        for (double ph = 0.0; ph < 1.0; ph += 0.01)
            maxDiff = std::max (maxDiff, std::abs (a->read (5.0, 0.0, ph) - b->read (5.0, 0.0, ph)));
        CHECK (maxDiff < 2e-4f, "contenu identique (16 bits) : %f", maxDiff);
    }
    CHECK (data.getSize() < 200000, "état compact : %d octets", (int) data.getSize());

    // Presets : la table voyage avec le preset ; DEFAULT revient aux tables intégrées
    p.presets.setFolderOverride (dir);
    CHECK (p.presets.save ("With Table"), "preset enregistré");
    p.presets.loadFactory (0);
    CHECK (p.getTableInfo (0).name == "BASIC SHAPES" && p.getTableInfo (1).name == "BASIC SHAPES", "DEFAULT : tables intégrées");
    CHECK (p.presets.load (dir.getChildFile ("With Table.seqpreset")), "preset rechargé");
    CHECK (p.getTableInfo (0).name == "MY TABLE" && p.getTableInfo (1).name == "RESO SAW", "tables du preset : %s / %s",
           p.getTableInfo (0).name.toRawUTF8(), p.getTableInfo (1).name.toRawUTF8());

    // État abîmé dans la partie TABLES : on revient aux tables intégrées sans planter
    {
        auto xml = juce::parseXML ("<Sequences><TABLES><TABLE osc=\"0\" builtin=\"-1\" name=\"X\" data=\"@@@notbase64\"/>"
                                   "<TABLE osc=\"7\" builtin=\"3\"/></TABLES></Sequences>");
        juce::MemoryBlock bad;
        juce::AudioProcessor::copyXmlToBinary (*xml, bad);
        q.setStateInformation (bad.getData(), (int) bad.getSize());
        CHECK (q.getTableInfo (0).name == "BASIC SHAPES", "données abîmées : table par défaut");
    }

    // Vraies tables Serum
    for (const auto& path : serumFiles)
    {
        const juce::File f (path);
        if (! f.existsAsFile()) continue;
        const auto err = p.loadTableFile (1, f);
        CHECK (err.isEmpty() && p.getTable (1)->numFrames > 0, "Serum : %s (%s)", f.getFileName().toRawUTF8(), err.toRawUTF8());
    }

    dir.deleteRecursively();
}

static void testState()
{
    SequencesAudioProcessor a;
    a.setRandomSeed (7);
    a.randomizeAll();
    set (a, "cutoff", 2500.0f);
    set (a, "aCoarse", 7.25f);
    set (a, "voiceMode", 0.0f);
    a.addModulation (5, seqs::Dest::pan);
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
    CHECK (p.presets.getDisplayName() == "DEFAULT", "nom au départ : %s", p.presets.getDisplayName().toRawUTF8());

    set (p, "bCoarse", -5.5f);
    set (p, SequencesAudioProcessor::stepId (5), 7.0f);
    CHECK (p.presets.isModified(), "modifié");
    CHECK (p.presets.save ("Hitech Seq"), "enregistré");
    CHECK (! p.presets.isModified(), "plus modifié après SAVE");

    p.presets.loadFactory (0);
    CHECK (std::abs (value (p, "bCoarse")) < 1e-3f, "DEFAULT remet les valeurs");

    CHECK (p.presets.load (dir.getChildFile ("Hitech Seq.seqpreset")), "rechargé");
    CHECK (std::abs (value (p, "bCoarse") + 5.5f) < 1e-3f, "valeur du preset");
    CHECK (stepSource (p, 5) == 7, "pas du preset");

    dir.deleteRecursively();
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
    CHECK (editor->getWidth() == SequencesFacade::width && editor->getHeight() == SequencesFacade::height, "taille par défaut");

    auto* e = dynamic_cast<SequencesAudioProcessorEditor*> (editor.get());
    e->setUiScale (0.75f);
    CHECK (editor->getWidth() == 990, "75 %% : %d", editor->getWidth());
    auto& facade = e->getFacade();

    // Un potard modulable pour chaque destination, une pastille pour chaque source
    for (int d = 1; d < seqs::numDests; ++d)
        CHECK (facade.getModKnob ((seqs::Dest) d) != nullptr, "potard modulable %s", SequencesAudioProcessor::destinationNames()[d].toRawUTF8());
    for (int s = 0; s < seqs::numModSources; ++s)
        CHECK (facade.getChip (s) != nullptr, "pastille %s", SequencesAudioProcessor::modSourceNames()[s].toRawUTF8());

    // Glisser-déposer : LFO 4 sur B POS
    auto* knob = facade.getModKnob (seqs::Dest::bPos);
    auto* chip = facade.getChip (4);
    juce::DragAndDropTarget::SourceDetails details (ui::SourceChip::dragDescription (4), chip, {});
    CHECK (knob->isInterestedInDragSource (details), "le potard accepte une source");
    CHECK (! knob->isInterestedInDragSource (juce::DragAndDropTarget::SourceDetails ("autre chose", nullptr, {})), "et refuse le reste");
    knob->itemDropped (details);
    const auto slots = p.slotsForDest (seqs::Dest::bPos);
    CHECK (slots.size() == 1 && p.getModulationSource (slots[0]) == 4, "LFO 4 -> B POS créé");
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
    CHECK ((int) rows.size() == p.usedSlotCount() && rows.size() == 5, "liste : %d lignes", (int) rows.size());
    const int row = (int) (std::find (rows.begin(), rows.end(), slots[0]) - rows.begin());
    const float y = (float) (row * ui::ModList::rowHeight) + ui::ModList::rowHeight * 0.5f;
    const float barLeft = 188.0f, barRight = (float) list.getWidth() - 26.0f;
    list.mouseDown (mouseAt (list, { barLeft + (barRight - barLeft) * 0.25f, y }));
    list.mouseUp (mouseAt (list, { barLeft + (barRight - barLeft) * 0.25f, y }));
    CHECK (std::abs (p.getModulationAmount (slots[0]) + 0.5f) < 0.02f, "profondeur depuis la liste : %f", p.getModulationAmount (slots[0]));
    list.mouseDown (mouseAt (list, { (float) list.getWidth() - 10.0f, y }));
    CHECK (p.slotsForDest (seqs::Dest::bPos).empty(), "× supprime le routage");

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
        auto& view = facade.getTableView (1);
        CHECK (view.isInterestedInFileDrag ({ f1.getFullPathName() }) && ! view.isInterestedInFileDrag ({ "/x/y.png" }), "accepte les .wav");
        view.filesDropped ({ f1.getFullPathName() }, 10, 10);
        CHECK (p.getTableInfo (1).name == "A FIRST" && view.getError().isEmpty(), "table déposée");
        view.step (+1);
        CHECK (p.getTableInfo (1).name == "B SECOND", "table suivante du dossier");
        view.step (+1);
        CHECK (p.getTableInfo (1).name == "A FIRST", "en boucle");
        p.setBuiltinTable (1, 0);
        view.step (-1);
        CHECK (p.getTableInfo (1).name == juce::String (seqs::BuiltinTables::names().back()), "intégrées en boucle");
        auto bad = dir.getChildFile ("bad.wav");
        bad.replaceWithText ("x");
        view.filesDropped ({ bad.getFullPathName() }, 10, 10);
        CHECK (view.getError().isNotEmpty(), "erreur affichée");
        dir.deleteRecursively();
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
    testEditor();

    std::printf ("%d vérifications, %d échecs\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
