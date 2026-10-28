// Tests du plugin complet (paramètres, MIDI, état, presets, interface).
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

#include <cstdio>

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
    CHECK (params.size() == 131, "nombre de paramètres : %d", params.size());

    juce::StringArray ids;
    for (auto* param : params)
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            ids.add (ranged->getParameterID());
    ids.removeDuplicates (false);
    CHECK (ids.size() == params.size(), "identifiants uniques");

    // Toutes les plages sont saines : valeurs et textes finis aux bouts et au milieu
    for (auto* param : params)
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            for (float n : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
            {
                const float real = ranged->convertFrom0to1 (n);
                const auto text = ranged->getText (n, 32);
                CHECK (std::isfinite (real) && ! text.containsIgnoreCase ("nan") && ! text.containsIgnoreCase ("inf"),
                       "%s à %.2f : %s", ranged->getParameterID().toRawUTF8(), n, text.toRawUTF8());
                CHECK (std::abs (ranged->convertTo0to1 (real) - n) < 0.02f || dynamic_cast<juce::AudioParameterFloat*> (ranged) == nullptr,
                       "%s : aller-retour à %.2f", ranged->getParameterID().toRawUTF8(), n);
            }

    CHECK (value (p, "voiceMode") == 1.0f, "POLY par défaut");
    CHECK (value (p, "seqOn") == 1.0f && value (p, "seqSteps") == 16.0f, "séquenceur allumé, 16 pas");
    CHECK (juce::roundToInt (value (p, "mod1Src")) == 0 && juce::roundToInt (value (p, "mod1Dest")) == (int) seqs::Dest::cutoff,
           "MOD 1 : SEQ -> CUTOFF");
    CHECK (SequencesAudioProcessor::destinationNames().size() == (int) seqs::Dest::count, "noms des destinations");
    CHECK (SequencesAudioProcessor::sourceNames().size() == seqs::numSources + 1, "noms des sources");
    CHECK (SequencesAudioProcessor::lfoRateNames()[4] == "1/16" && SequencesAudioProcessor::lfoRateBeats (4) == 0.25, "périodes");
    CHECK (p.apvts.getParameter ("lfo1Rate")->getCurrentValueAsText() == "1/16", "LFO 1 en 1/16");
    CHECK (p.apvts.getParameter ("lfo3Shape")->getCurrentValueAsText() == "EXP DOWN", "LFO 3 en EXP DOWN");
    CHECK (p.apvts.getParameter ("seqStep12")->getCurrentValueAsText() == "CHAOS 1", "pas 12 : CHAOS 1");

    // Les réglages lus correspondent aux paramètres
    seqs::Settings s;
    p.readSettings (s);
    CHECK (s.seq.on && s.seq.steps == 16 && std::abs (s.seq.stepBeats - 0.5) < 1e-9, "séquenceur lu");
    CHECK (s.seq.pattern[0] == 1 && s.seq.pattern[11] == 7 && s.seq.pattern[14] == 8, "motif lu");
    CHECK (s.lfo[3].steps == 8 && s.lfo[3].shape == seqs::LfoShape::triangle, "LFO 4 lu");
    CHECK (s.slotDest[0] == seqs::Dest::cutoff && std::abs (s.slotAmount[0] - 0.45f) < 1e-4f, "routage lu");
    CHECK (p.apvts.getParameter ("cutoff")->getCurrentValueAsText() == "900 Hz", "texte de la coupure : %s",
           p.apvts.getParameter ("cutoff")->getCurrentValueAsText().toRawUTF8());
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

    // Séquenceur éteint et sans routage : RANDOM l'allume et le branche
    set (p, "seqOn", 0.0f);
    set (p, "mod1Dest", 0.0f);
    set (p, "mod2Src", 3.0f);
    p.randomizePattern();
    CHECK (value (p, "seqOn") == 1.0f, "RANDOM allume le séquenceur");
    CHECK (juce::roundToInt (value (p, "mod1Dest")) == (int) seqs::Dest::cutoff && juce::roundToInt (value (p, "mod1Src")) == 0,
           "RANDOM branche MOD 1 sur la coupure");

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

static void testState()
{
    SequencesAudioProcessor a;
    a.setRandomSeed (7);
    a.randomizeAll();
    set (a, "cutoff", 2500.0f);
    set (a, "lfo2Rate", 5.0f);
    set (a, "voiceMode", 0.0f);
    a.apvts.state.setProperty ("editorWidth", 930, nullptr);

    juce::MemoryBlock data;
    a.getStateInformation (data);

    SequencesAudioProcessor b;
    b.setStateInformation (data.getData(), (int) data.getSize());
    bool same = true;
    for (int i = 0; i < a.getParameters().size(); ++i)
        same = same && std::abs (a.getParameters()[i]->getValue() - b.getParameters()[i]->getValue()) < 1e-6f;
    CHECK (same, "état relu à l'identique");
    CHECK ((int) b.apvts.state.getProperty ("editorWidth") == 930, "taille de fenêtre gardée");

    // État abîmé : ignoré sans planter
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

    set (p, "fmIndex", 7.5f);
    set (p, SequencesAudioProcessor::stepId (5), 7.0f);
    CHECK (p.presets.isModified(), "modifié");
    CHECK (p.presets.save ("Hitech Seq"), "enregistré");
    CHECK (! p.presets.isModified(), "plus modifié après SAVE");

    p.presets.loadFactory (0);
    CHECK (std::abs (value (p, "fmIndex") - 3.0f) < 1e-3f, "DEFAULT remet les valeurs");

    CHECK (p.presets.load (dir.getChildFile ("Hitech Seq.seqpreset")), "rechargé");
    CHECK (std::abs (value (p, "fmIndex") - 7.5f) < 1e-3f, "valeur du preset");
    CHECK (stepSource (p, 5) == 7, "pas du preset");

    dir.deleteRecursively();
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
    CHECK ((int) p.apvts.state.getProperty ("editorWidth") == 990, "taille enregistrée");

    // RANDOM depuis l'interface
    p.setRandomSeed (99);
    std::vector<int> before;
    for (int i = 0; i < 32; ++i) before.push_back (stepSource (p, i));
    e->getFacade().getRandomButton().triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    int changedSteps = 0;
    for (int i = 0; i < 32; ++i)
        changedSteps += stepSource (p, i) != before[(size_t) i] ? 1 : 0;
    CHECK (changedSteps > 6, "bouton RANDOM : %d pas changés", changedSteps);

    // Matrice : un clic sur la ligne CHAOS 2, colonne 6
    auto& m = e->getFacade().getMatrix();
    auto click = [&m] (int column, int row, bool right = false)
    {
        const float cw = (float) (m.getWidth() - ui::StepMatrix::labelWidth) / 32.0f;
        const float rh = (float) m.getHeight() / (float) ui::StepMatrix::numRows;
        const juce::Point<float> pt ((float) ui::StepMatrix::labelWidth + cw * ((float) column + 0.5f), rh * ((float) row + 0.5f));
        const auto mods = right ? juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier) : juce::ModifierKeys();
        juce::MouseEvent ev (juce::Desktop::getInstance().getMainMouseSource(), pt, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             &m, &m, juce::Time::getCurrentTime(), pt, juce::Time::getCurrentTime(), 1, false);
        m.mouseDown (ev);
        m.mouseUp (ev);
    };
    click (6, ui::StepMatrix::rowForSource (8));
    CHECK (stepSource (p, 6) == 8, "matrice : pas 7 sur CHAOS 2 (%d)", stepSource (p, 6));
    click (6, ui::StepMatrix::rowForSource (3));
    CHECK (stepSource (p, 6) == 3, "matrice : pas 7 sur LFO 3");
    click (6, 0, true);
    CHECK (stepSource (p, 6) == 0, "clic droit : OFF");
    click (9, ui::StepMatrix::rowForSource (0));
    CHECK (stepSource (p, 9) == 0, "ligne OFF");

    // Rendu de la façade sans erreur
    juce::Image image (juce::Image::ARGB, SequencesFacade::width, SequencesFacade::height, true);
    juce::Graphics g (image);
    e->getFacade().paintEntireComponent (g, false);
    CHECK (image.getPixelAt (5, 5).getAlpha() > 0, "façade dessinée");

    editor.reset();
}

int main()
{
    testParameters();
    testAudio();
    testRandomAndTools();
    testState();
    testPresets();
    testEditor();

    std::printf ("%d vérifications, %d échecs\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
