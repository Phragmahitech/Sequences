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

        /** Couleur d'une source de modulation. */
        static juce::Colour modSource (int s)
        {
            switch (s)
            {
                case seqs::srcSeq:      return juce::Colour (0xff00a9b8);
                case seqs::srcEnv2:     return juce::Colour (0xff8a6a3a);
                case seqs::srcEnv3:     return juce::Colour (0xff5d7a2a);
                case seqs::srcVelocity: return juce::Colour (0xffd0312d);
                case seqs::srcMainLfo:  return juce::Colour (0xff3a4bd0);
                case seqs::srcModWheel: return juce::Colour (0xff9a3a7a);
                default:                return source (s);
            }
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
        juce::Font getComboBoxFont (juce::ComboBox&) override                 { return font (11.0f, true, 0.04f); }
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
        juce::Font getPopupMenuFont() override                                 { return font (12.5f, true, 0.04f); }
        juce::Font getTextButtonFont (juce::TextButton&, int) override        { return font (10.5f, true, 0.12f); }
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
        int getActiveSlot() const noexcept                      { return activeSlot; }
        void setActiveSlot (int s) noexcept                      { activeSlot = s; }
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

        static constexpr int rowHeight = 24;
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
    /** Affichage de la wavetable + choix de table (menu, ◀ ▶, glisser un fichier .wav depuis le Finder). */
    class WavetableView final : public juce::Component,
                                public juce::FileDragAndDropTarget
    {
    public:
        explicit WavetableView (SequencesAudioProcessor&);

        void paint (juce::Graphics&) override;
        void resized() override;

        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void fileDragEnter (const juce::StringArray&, int, int) override    { fileHover = true; repaint(); }
        void fileDragExit (const juce::StringArray&) override               { fileHover = false; repaint(); }
        void filesDropped (const juce::StringArray& files, int, int) override;

        void load (const juce::File& file);
        void step (int delta);
        void openChooser();
        juce::String getError() const                       { return error; }

    private:
        void showMenu();
        void openChooserAt (const juce::File& folder);

        SequencesAudioProcessor& processor;
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

    /** Forme d'onde de la PHASE DISTORTION (forme, PD AMT et SHAPE, modulation comprise). */
    class PdView final : public juce::Component
    {
    public:
        explicit PdView (SequencesAudioProcessor& p) : processor (p) {}
        void paint (juce::Graphics&) override;

    private:
        SequencesAudioProcessor& processor;
        seqs::Settings settings;
    };

    /** Courbe d'une enveloppe ADSR. */
    class EnvView final : public juce::Component
    {
    public:
        EnvView (SequencesAudioProcessor& p, juce::String prefix, juce::Colour c) : processor (p), id (std::move (prefix)), colour (c) {}
        void paint (juce::Graphics&) override;

    private:
        SequencesAudioProcessor& processor;
        juce::String id;
        juce::Colour colour;
    };

    class MainLfoView final : public juce::Component
    {
    public:
        explicit MainLfoView (SequencesAudioProcessor& p) : processor (p) {}
        void paint (juce::Graphics&) override;

    private:
        SequencesAudioProcessor& processor;
        seqs::Settings settings;
    };

    /** Case de la matrice FM : glisser vers le haut / bas règle la quantité, double-clic remet à zéro. */
    class FmCell final : public juce::Component
    {
    public:
        FmCell (SequencesAudioProcessor&, int row, int col);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;

        float getValue() const;     // 0..1

    private:
        void setValue (float v);
        SequencesAudioProcessor& processor;
        int row, col;
        juce::RangedAudioParameter* param = nullptr;
        float startValue = 0.0f;
        bool dragged = false;
    };

    /** Oscilloscope + crête de sortie. */
    class Scope final : public juce::Component
    {
    public:
        explicit Scope (SequencesAudioProcessor& p) : processor (p) {}
        void paint (juce::Graphics&) override;

    private:
        SequencesAudioProcessor& processor;
        std::array<float, SequencesAudioProcessor::scopeSize> data {};
    };

    /** Une page (onglet) : ses panneaux et légendes sont dessinés derrière ses potards. */
    class Page final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;

        struct Panel { juce::String title; juce::Rectangle<int> bounds; juce::String note; };
        std::vector<Panel> panels;
        std::vector<std::pair<juce::String, juce::Rectangle<int>>> captions, leftCaptions, notes;
    };
}

//==============================================================================
class SequencesFacade final : public juce::Component,
                              public juce::DragAndDropContainer,
                              private juce::Timer
{
public:
    static constexpr int width = 1320, height = 900;
    enum Tab { tabMain = 0, tabSeq, tabMatrix, tabArp, tabFx, numTabs };

    explicit SequencesFacade (SequencesAudioProcessor&);
    ~SequencesFacade() override;

    void paint (juce::Graphics&) override;
    void resized() override {}

    phragma::PresetBar& getPresetBar() noexcept       { return presetBar; }

    void showTab (int tab);
    int getTab() const noexcept                       { return currentTab; }

    /** Pour les tests. */
    juce::TextButton& getRandomButton()               { return randomButton; }
    juce::TextButton& getRandomAllButton()            { return randomAllButton; }
    juce::TextButton& getTabButton (int t)            { return tabButtons[(size_t) t]; }
    ui::Page& getPage (int t)                         { return pages[(size_t) t]; }
    ui::StepMatrix& getMatrix()                       { return *matrix; }
    ui::ModKnob* getModKnob (seqs::Dest);
    ui::SourceChip* getChip (int source);
    ui::ModList& getModList()                         { return *modList; }
    ui::WavetableView& getTableView()                 { return *tableView; }
    ui::FmCell* getFmCell (int row, int col);
    juce::Component* findById (const juce::String& id);

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

    void timerCallback() override;

    // Construction : chaque élément va sur une page
    juce::Slider& addKnob (ui::Page&, const juce::String& id, const juce::String& label, juce::Rectangle<int> cell, bool bipolar = false, bool small = false);
    juce::ComboBox& addInlineChoice (ui::Page&, const juce::String& id, juce::Rectangle<int> area);
    void addLabelledChoice (ui::Page&, const juce::String& id, const juce::String& label, juce::Rectangle<int> area, int labelWidth);
    void addCaptionedChoice (ui::Page&, const juce::String& id, const juce::String& label, juce::Rectangle<int> area);
    void addBar (ui::Page&, const juce::String& id, const juce::String& label, juce::Rectangle<int> area, bool bipolar);
    juce::TextButton& addToggle (ui::Page&, const juce::String& id, const juce::String& text, juce::Rectangle<int> area);
    void addButton (juce::Component& parent, juce::TextButton& b, const juce::String& text, const juce::String& title, juce::Rectangle<int> area);
    ui::SourceChip& addChip (int source, const juce::String& text, juce::Rectangle<int> area);
    /** Une rangée de potards répartis sur la largeur. */
    struct K { juce::String id, label; bool bipolar = false; };
    void knobRow (ui::Page&, juce::Rectangle<int> area, std::initializer_list<K> knobs, bool small = false);
    juce::Rectangle<int> panel (ui::Page&, const juce::String& title, juce::Rectangle<int> bounds, const juce::String& note = {});

    void buildMain();
    void buildSeq();
    void buildMatrix();
    void buildArp();
    void buildFx();

    SequencesAudioProcessor& audioProcessor;
    ui::LookAndFeel lookAndFeel;
    phragma::PresetBar presetBar;

    std::array<ui::Page, numTabs> pages;
    std::array<juce::TextButton, numTabs> tabButtons;
    int currentTab = tabMain;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<std::unique_ptr<Toggle>> toggles;
    std::vector<std::unique_ptr<ui::SourceChip>> chips;
    std::vector<ui::ModKnob*> modKnobs;
    std::vector<std::unique_ptr<juce::Component>> owned;     // vues diverses (formes, enveloppes...)
    std::vector<ui::FmCell*> fmCells;

    std::unique_ptr<ui::WavetableView> tableView;
    std::unique_ptr<ui::StepMatrix> matrix;
    std::unique_ptr<ui::CircuitView> circuit;
    std::unique_ptr<ui::ModList> modList, seqModList;
    juce::Viewport modViewport, seqModViewport;
    std::unique_ptr<ui::Scope> scope;
    std::vector<ui::LfoPreview*> previews;
    juce::TextButton loadButton, leftButton, rightButton, clearButton, randomButton, randomAllButton, addModButton;
    juce::Slider* mlHzKnob = nullptr;
    juce::ComboBox* mlRateBox = nullptr;
    juce::Slider* delayMsKnob = nullptr;
    juce::ComboBox* delayTimeBox = nullptr;
    juce::Rectangle<int> modListArea, seqModListArea, headerInfo;

    int shownStep = -2, tick = 0, shownRows = -1, shownVoices = -1;

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
