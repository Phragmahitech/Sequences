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
    CHECK (params.size() == 187, "nombre de paramètres : %d", params.size());

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
    CHECK (value (p, "seqAOn") == 1.0f && value (p, "seqBOn") == 0.0f, "SEQ A allumé, B éteint");
    CHECK (value (p, "seqASteps") == 16.0f, "16 pas");
    CHECK (juce::roundToInt (value (p, "seqADest1")) == (int) seqs::Dest::cutoff, "SEQ A -> CUTOFF");
    CHECK (SequencesAudioProcessor::destinationNames().size() == (int) seqs::Dest::count, "noms des destinations");
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
        set (s, "seqASync", 1.0f);   // HOST
        s.prepareToPlay (48000.0, 128);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        juce::AudioBuffer<float> buffer (2, 128);
        s.processBlock (buffer, midi);
        CHECK (s.getPlayheadStep (0) == 8, "HOST : pas 8 à la noire 2 : %d", s.getPlayheadStep (0));
        CHECK (s.getPlayheadStep (1) == -1, "SEQ B éteint : pas de tête");
        head.playing = false;
        head.ppq = 0.0;
    }
}

static void testRandomAndTools()
{
    SequencesAudioProcessor p;
    p.setRandomSeed (42);

    std::vector<float> before;
    for (int i = 0; i < 32; ++i) before.push_back (value (p, SequencesAudioProcessor::stepId (0, i)));

    p.randomizeSequence (0);
    int changed = 0;
    for (int i = 0; i < 32; ++i)
    {
        const float v = value (p, SequencesAudioProcessor::stepId (0, i));
        CHECK (v >= 0.0f && v <= 1.0f, "valeur aléatoire bornée");
        changed += std::abs (v - before[(size_t) i]) > 1e-4f ? 1 : 0;
    }
    CHECK (changed > 8, "RANDOM change le motif : %d pas", changed);

    // SEQ B éteint et sans destination : RANDOM l'allume et le branche sur la coupure
    p.randomizeSequence (1);
    CHECK (value (p, "seqBOn") == 1.0f, "RANDOM allume SEQ B");
    CHECK (juce::roundToInt (value (p, "seqBDest1")) == (int) seqs::Dest::cutoff && value (p, "seqBAmt1") > 50.0f, "RANDOM branche SEQ B");

    // Décalage dans la longueur choisie
    set (p, "seqASteps", 4.0f);
    for (int i = 0; i < 4; ++i) set (p, SequencesAudioProcessor::stepId (0, i), 0.1f * (float) (i + 1));
    set (p, SequencesAudioProcessor::glideId (0, 0), 1.0f);
    set (p, SequencesAudioProcessor::glideId (0, 1), 0.0f);
    set (p, SequencesAudioProcessor::glideId (0, 2), 0.0f);
    set (p, SequencesAudioProcessor::glideId (0, 3), 0.0f);
    const float outside = value (p, SequencesAudioProcessor::stepId (0, 10));
    p.rotateSequence (0, +1);
    CHECK (std::abs (value (p, SequencesAudioProcessor::stepId (0, 0)) - 0.4f) < 1e-3f, "décalage : le dernier pas passe devant");
    CHECK (std::abs (value (p, SequencesAudioProcessor::stepId (0, 1)) - 0.1f) < 1e-3f, "décalage : pas 1");
    CHECK (value (p, SequencesAudioProcessor::glideId (0, 1)) == 1.0f, "décalage : le glide suit");
    CHECK (value (p, SequencesAudioProcessor::stepId (0, 10)) == outside, "décalage : hors de la boucle, rien ne bouge");
    p.rotateSequence (0, -1);
    CHECK (std::abs (value (p, SequencesAudioProcessor::stepId (0, 0)) - 0.1f) < 1e-3f, "décalage aller-retour");

    p.clearSequence (0, 0.0f);
    bool allZero = true;
    for (int i = 0; i < 32; ++i)
        allZero = allZero && value (p, SequencesAudioProcessor::stepId (0, i)) == 0.0f && value (p, SequencesAudioProcessor::glideId (0, i)) == 0.0f;
    CHECK (allZero, "CLEAR");
}

static void testState()
{
    SequencesAudioProcessor a;
    a.setRandomSeed (7);
    a.randomizeSequence (0);
    set (a, "cutoff", 2500.0f);
    set (a, "seqBRate", 5.0f);
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
    set (p, SequencesAudioProcessor::stepId (1, 5), 0.33f);
    CHECK (p.presets.isModified(), "modifié");
    CHECK (p.presets.save ("Hitech Seq"), "enregistré");
    CHECK (! p.presets.isModified(), "plus modifié après SAVE");

    p.presets.loadFactory (0);
    CHECK (std::abs (value (p, "fmIndex") - 3.0f) < 1e-3f, "DEFAULT remet les valeurs");

    CHECK (p.presets.load (dir.getChildFile ("Hitech Seq.seqpreset")), "rechargé");
    CHECK (std::abs (value (p, "fmIndex") - 7.5f) < 1e-3f, "valeur du preset");
    CHECK (std::abs (value (p, SequencesAudioProcessor::stepId (1, 5)) - 0.33f) < 1e-3f, "pas du preset");

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
    CHECK (editor->getWidth() == 930, "75 %% : %d", editor->getWidth());
    CHECK ((int) p.apvts.state.getProperty ("editorWidth") == 930, "taille enregistrée");

    // RANDOM depuis l'interface
    p.setRandomSeed (99);
    std::vector<float> before;
    for (int i = 0; i < 32; ++i) before.push_back (value (p, SequencesAudioProcessor::stepId (0, i)));
    e->getFacade().getRandomButton (0).triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    int changedSteps = 0;
    for (int i = 0; i < 32; ++i)
        changedSteps += std::abs (value (p, SequencesAudioProcessor::stepId (0, i)) - before[(size_t) i]) > 1e-4f ? 1 : 0;
    CHECK (changedSteps > 8, "bouton RANDOM : %d pas changés", changedSteps);

    // Dessin dans la grille : un clic en haut d'une case met le pas au maximum
    auto& grid = e->getFacade().getGrid (1);
    const auto local = juce::Point<float> ((float) grid.getWidth() / 32.0f * 6.5f, 2.0f);
    juce::MouseEvent down (juce::Desktop::getInstance().getMainMouseSource(), local, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                           &grid, &grid, juce::Time::getCurrentTime(), local, juce::Time::getCurrentTime(), 1, false);
    grid.mouseDown (down);
    grid.mouseUp (down);
    CHECK (value (p, SequencesAudioProcessor::stepId (1, 6)) > 0.95f, "dessin d'un pas : %.3f", value (p, SequencesAudioProcessor::stepId (1, 6)));

    // Clic sur la rangée des glides
    const auto glidePoint = juce::Point<float> ((float) grid.getWidth() / 32.0f * 9.5f, (float) grid.getHeight() - 5.0f);
    const float glideBefore = value (p, SequencesAudioProcessor::glideId (1, 9));
    juce::MouseEvent glideClick (juce::Desktop::getInstance().getMainMouseSource(), glidePoint, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &grid, &grid, juce::Time::getCurrentTime(), glidePoint, juce::Time::getCurrentTime(), 1, false);
    grid.mouseDown (glideClick);
    grid.mouseUp (glideClick);
    CHECK (value (p, SequencesAudioProcessor::glideId (1, 9)) != glideBefore, "glide basculé");

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
