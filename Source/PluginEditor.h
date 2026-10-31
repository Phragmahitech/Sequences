#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PresetBar.h"

namespace ui
{
    struct Palette
    {
        static juce::Colour background()  { return juce::Colour (0xffe6e7e9); }
        static juce::Colour panel()       { return juce::Colour (0xfff7f7f8); }
        static juce::Colour panelLine()   { return juce::Colour (0xffcfd1d5); }
        static juce::Colour ink()         { return juce::Colour (0xff17181a); }
        static juce::Colour dim()         { return juce::Colour (0xff6b6e73); }
        static juce::Colour faint()       { return juce::Colour (0xffb9bbc0); }
        static juce::Colour accent()      { return juce::Colour (0xffff5a1f); }
        static juce::Colour stepOff()     { return juce::Colour (0xffe9eaec); }

        /** Couleur d'une source du séquenceur : 1..6 = LFO, 7..8 = CHAOS, 0 = OFF. */
        static juce::Colour source (int s)
        {
            static const juce::uint32 colours[] = { 0xffb9bbc0, 0xffff5a1f, 0xff1f8fff, 0xff16a874, 0xffa64bf0,
                                                     0xffe0a800, 0xffff3d8b, 0xff17181a, 0xff6b6e73 };
            return juce::Colour (colours[juce::jlimit (0, 8, s)]);
        }

        /** Couleur d'une source de modulation : SEQ, LFO 1..6, CHAOS 1..2, ENV 2, VELO. */
        static juce::Colour modSource (int s)
        {
            if (s == seqs::srcSeq)       return juce::Colour (0xff00a9b8);
            if (s == seqs::srcEnv2)      return juce::Colour (0xff8a6a3a);
            if (s == seqs::srcVelocity)  return juce::Colour (0xffd0312d);
            return source (s);
        }
    };

    juce::Font font (float height, bool bold = false, float tracking = 0.0f);

    //==========================================================================
    class LookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
        void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;
        juce::Label* createSliderTextBox (juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
        void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override                 { return font (11.5f, true, 0.04f); }
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
        juce::Font getPopupMenuFont() override                                 { return font (12.5f, true, 0.04f); }
        juce::Font getTextButtonFont (juce::TextButton&, int) override        { return font (11.0f, true, 0.12f); }
    };

    //==========================================================================
    /** Pastille d'une source de modulation, à glisser sur un potard. */
    class SourceChip final : public juce::Component
    {
    public:
        SourceChip (int source, juce::String text);

        void paint (juce::Graphics&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        static juce::String dragDescription (int source)   { return "modsrc:" + juce::String (source); }
        /** Source d'une description de glisser-déposer, -1 si ce n'en est pas une. */
        static int sourceFromDescription (const juce::var& description);

        int getSource() const noexcept                      { return source; }

    private:
        int source;
        juce::String text;
        bool dragging = false;
    };

    //==========================================================================
    /** Potard modulable : on y dépose une source ; les anneaux de couleur montrent les modulations.
        Glisser sur l'anneau (ou Alt + glisser) règle la profondeur ; clic droit : menu des modulations. */
    class ModKnob final : public juce::Slider,
                          public juce::DragAndDropTarget
    {
    public:
        ModKnob (SequencesAudioProcessor&, seqs::Dest);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        bool isInterestedInDragSource (const SourceDetails&) override;
        void itemDragEnter (const SourceDetails&) override     { dropHover = true; repaint(); }
        void itemDragExit (const SourceDetails&) override      { dropHover = false; repaint(); }
        void itemDropped (const SourceDetails&) override;

        seqs::Dest getDest() const noexcept                     { return dest; }
        /** Pour les tests : le slot que le prochain réglage de profondeur modifie. */
        int getActiveSlot() const noexcept                      { return activeSlot; }
        void setActiveSlot (int s) noexcept                      { activeSlot = s; }

        /** Le point (en coordonnées du potard) est-il sur l'anneau de modulation ? */
        bool isOnRing (juce::Point<float>) const;

    private:
        juce::Rectangle<float> rotaryArea() const;
        int slotForDepth() const;
        void showMenu();

        SequencesAudioProcessor& processor;
        seqs::Dest dest;
        bool dropHover = false;
        int activeSlot = -1, depthSlot = -1;
        float depthStart = 0.0f;
        juce::RangedAudioParameter* depthParam = nullptr;
    };

    //==========================================================================
    /** Liste des modulations en cours : source -> potard, profondeur, suppression. */
    class ModList final : public juce::Component
    {
    public:
        explicit ModList (SequencesAudioProcessor&);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;

        static constexpr int rowHeight = 22;
        /** Hauteur voulue (suivant le nombre de modulations). */
        int idealHeight() const;
        std::vector<int> rows() const;

    private:
        juce::Rectangle<float> sourceArea (int row) const;
        juce::Rectangle<float> destArea (int row) const;
        juce::Rectangle<float> amountArea (int row) const;
        juce::Rectangle<float> removeArea (int row) const;

        SequencesAudioProcessor& processor;
        int dragSlot = -1;
        juce::RangedAudioParameter* dragParam = nullptr;
    };

    //==========================================================================
    /** Affichage d'une wavetable + choix de table (menu, ◀ ▶, glisser un fichier .wav depuis le Finder). */
    class WavetableView final : public juce::Component,
                                public juce::FileDragAndDropTarget
    {
    public:
        WavetableView (SequencesAudioProcessor&, int osc);

        void paint (juce::Graphics&) override;
        void resized() override;

        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void fileDragEnter (const juce::StringArray&, int, int) override    { fileHover = true; repaint(); }
        void fileDragExit (const juce::StringArray&) override               { fileHover = false; repaint(); }
        void filesDropped (const juce::StringArray& files, int, int) override;

        /** Importe un fichier (affiche l'erreur s'il y en a une). */
        void load (const juce::File& file);
        void step (int delta);
        juce::String getError() const                       { return error; }

    private:
        void showMenu();
        void openChooser (const juce::File& folder);

        SequencesAudioProcessor& processor;
        int osc;
        juce::TextButton prev, next, name;
        std::unique_ptr<juce::FileChooser> chooser;
        juce::String error;
        juce::uint32 errorTime = 0;
        bool fileHover = false;
    };

    //==========================================================================
    /** Matrice du séquenceur : une ligne par source (et OFF), une colonne par pas. */
    class StepMatrix final : public juce::Component
    {
    public:
        explicit StepMatrix (SequencesAudioProcessor&);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;

        static constexpr int labelWidth = 74;
        static constexpr int numRows = seqs::numSources + 1;   // LFO 1..6, CHAOS 1..2, OFF

        static int sourceForRow (int row)          { return row == numRows - 1 ? 0 : row + 1; }
        static int rowForSource (int source)       { return source == 0 ? numRows - 1 : source - 1; }

    private:
        juce::Rectangle<float> gridArea() const;
        int columnAt (float x) const;
        int rowAt (float y) const;
        void setStep (int step, int source);

        SequencesAudioProcessor& processor;
        std::array<juce::RangedAudioParameter*, SequencesAudioProcessor::numSteps> stepParams {};
        juce::RangedAudioParameter* stepsParam = nullptr;
        std::vector<juce::RangedAudioParameter*> touched;
        int lastColumn = -1;
    };

    //==========================================================================
    class CircuitView final : public juce::Component
    {
    public:
        explicit CircuitView (SequencesAudioProcessor& p) : processor (p) {}
        void paint (juce::Graphics&) override;

    private:
        SequencesAudioProcessor& processor;
        seqs::Settings settings;
    };

    //==========================================================================
    class LfoPreview final : public juce::Component
    {
    public:
        LfoPreview (SequencesAudioProcessor& p, int index) : processor (p), lfo (index) {}
        void paint (juce::Graphics&) override;

    private:
        SequencesAudioProcessor& processor;
        int lfo;
        seqs::Settings settings;
    };
}

//==============================================================================
class SequencesFacade final : public juce::Component,
                              public juce::DragAndDropContainer,
                              private juce::Timer
{
public:
    static constexpr int width = 1320, height = 900;

    explicit SequencesFacade (SequencesAudioProcessor&);
    ~SequencesFacade() override;

    void paint (juce::Graphics&) override;
    void resized() override {}

    phragma::PresetBar& getPresetBar() noexcept       { return presetBar; }

    /** Pour les tests. */
    juce::TextButton& getRandomButton()               { return randomButton; }
    juce::TextButton& getRandomAllButton()            { return randomAllButton; }
    ui::StepMatrix& getMatrix()                       { return *matrix; }
    ui::ModKnob* getModKnob (seqs::Dest);
    ui::SourceChip* getChip (int source);
    ui::ModList& getModList()                         { return *modList; }
    ui::WavetableView& getTableView (int osc)         { return *tableViews[(size_t) osc]; }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Choice
    {
        juce::ComboBox box;
        std::unique_ptr<ComboAttachment> attachment;
    };

    struct Toggle
    {
        juce::TextButton button;
        std::unique_ptr<ButtonAttachment> attachment;
    };

    struct Panel
    {
        juce::String title;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;

    juce::Slider& addKnob (const juce::String& id, const juce::String& label, juce::Rectangle<int> cell, bool bipolar = false, bool small = false);
    void addChoice (const juce::String& id, const juce::String& label, juce::Rectangle<int> cell);
    juce::ComboBox& addInlineChoice (const juce::String& id, juce::Rectangle<int> area);
    void addLabelledChoice (const juce::String& id, const juce::String& label, juce::Rectangle<int> area, int labelWidth);
    void addBar (const juce::String& id, const juce::String& label, juce::Rectangle<int> area, bool bipolar);
    void addToggle (const juce::String& id, const juce::String& text, juce::Rectangle<int> area);
    void addButton (juce::TextButton& b, const juce::String& text, const juce::String& title, juce::Rectangle<int> area);
    ui::SourceChip& addChip (int source, const juce::String& text, juce::Rectangle<int> area);

    void buildOsc (int osc, juce::Rectangle<int> area);
    void buildBank (juce::Rectangle<int> area);
    void buildSequencer (juce::Rectangle<int> area);

    SequencesAudioProcessor& audioProcessor;
    ui::LookAndFeel lookAndFeel;
    phragma::PresetBar presetBar;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<std::unique_ptr<Toggle>> toggles;
    std::vector<std::unique_ptr<ui::SourceChip>> chips;
    std::vector<ui::ModKnob*> modKnobs;
    std::vector<Panel> panels;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> captions, leftCaptions;

    std::array<std::unique_ptr<ui::WavetableView>, seqs::numOscs> tableViews;
    std::unique_ptr<ui::StepMatrix> matrix;
    std::unique_ptr<ui::CircuitView> circuit;
    std::unique_ptr<ui::ModList> modList;
    juce::Viewport modViewport;
    std::vector<std::unique_ptr<ui::LfoPreview>> previews;
    juce::TextButton leftButton, rightButton, clearButton, randomButton, randomAllButton;
    juce::Rectangle<int> seqPanel, bankPanel, modListArea;

    int shownStep = -2, tick = 0, shownRows = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencesFacade)
};

//==============================================================================
class SequencesAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SequencesAudioProcessorEditor (SequencesAudioProcessor&);
    ~SequencesAudioProcessorEditor() override = default;

    void resized() override;

    SequencesFacade& getFacade() noexcept                { return facade; }

    float getUiScale() const                             { return (float) getWidth() / (float) SequencesFacade::width; }
    void setUiScale (float scale);

private:
    SequencesAudioProcessor& audioProcessor;
    SequencesFacade facade;
    bool sized = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencesAudioProcessorEditor)
};
