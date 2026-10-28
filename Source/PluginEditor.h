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

        /** Couleur d'une source : 1..6 = LFO, 7..8 = CHAOS, 0 = OFF. */
        static juce::Colour source (int s)
        {
            static const juce::uint32 colours[] = { 0xffb9bbc0, 0xffff5a1f, 0xff1f8fff, 0xff16a874, 0xffa64bf0,
                                                     0xffe0a800, 0xffff3d8b, 0xff17181a, 0xff6b6e73 };
            return juce::Colour (colours[juce::jlimit (0, 8, s)]);
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

        /** Ligne -> source (ligne du bas = OFF). */
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
    /** LFO CIRCUIT : la courbe que sort le séquenceur sur une boucle, colorée par source. */
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
    /** Petit dessin d'un cycle de LFO, dans l'en-tête de sa colonne. */
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
/** La façade, dessinée à sa taille d'origine puis mise à l'échelle par l'éditeur. */
class SequencesFacade final : public juce::Component,
                              private juce::Timer
{
public:
    static constexpr int width = 1320, height = 920;

    explicit SequencesFacade (SequencesAudioProcessor&);
    ~SequencesFacade() override;

    void paint (juce::Graphics&) override;
    void resized() override {}

    phragma::PresetBar& getPresetBar() noexcept       { return presetBar; }

    /** Pour les tests. */
    juce::TextButton& getRandomButton()               { return randomButton; }
    juce::TextButton& getRandomAllButton()            { return randomAllButton; }
    ui::StepMatrix& getMatrix()                       { return *matrix; }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
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
        juce::Colour swatch;
    };

    void timerCallback() override;

    void addKnob (const juce::String& id, const juce::String& label, juce::Rectangle<int> cell, bool bipolar = false, bool small = false);
    void addChoice (const juce::String& id, const juce::String& label, juce::Rectangle<int> cell);
    juce::ComboBox& addInlineChoice (const juce::String& id, juce::Rectangle<int> area);
    void addLabelledChoice (const juce::String& id, const juce::String& label, juce::Rectangle<int> area, int labelWidth);
    void addBar (const juce::String& id, const juce::String& label, juce::Rectangle<int> area, bool bipolar);
    void addToggle (const juce::String& id, const juce::String& text, juce::Rectangle<int> area);
    void addButton (juce::TextButton& b, const juce::String& text, const juce::String& title, juce::Rectangle<int> area);

    void buildBank (juce::Rectangle<int> area);
    void buildSequencer (juce::Rectangle<int> area);

    SequencesAudioProcessor& audioProcessor;
    ui::LookAndFeel lookAndFeel;
    phragma::PresetBar presetBar;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<std::unique_ptr<Toggle>> toggles;
    std::vector<Panel> panels;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> captions, leftCaptions;

    std::unique_ptr<ui::StepMatrix> matrix;
    std::unique_ptr<ui::CircuitView> circuit;
    std::vector<std::unique_ptr<ui::LfoPreview>> previews;
    juce::TextButton leftButton, rightButton, clearButton, randomButton, randomAllButton;
    juce::Rectangle<int> seqPanel, bankPanel;

    int shownStep = -2, tick = 0;

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
