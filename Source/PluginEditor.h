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
    /** Grille de pas d'un séquenceur : on dessine les valeurs à la souris, glides en dessous. */
    class StepGrid final : public juce::Component
    {
    public:
        StepGrid (SequencesAudioProcessor&, int seqIndex);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;

        static constexpr int glideRowHeight = 18;

    private:
        juce::Rectangle<float> barsArea() const;
        juce::Rectangle<float> glideArea() const;
        int stepAtX (float x) const;
        float valueAtY (float y) const;
        void setStepValue (int step, float value);
        void setGlide (int step, bool on);
        void touch (juce::RangedAudioParameter*);

        SequencesAudioProcessor& processor;
        int seq;
        std::array<juce::RangedAudioParameter*, SequencesAudioProcessor::numSteps> valueParams {}, glideParams {};
        juce::RangedAudioParameter* stepsParam = nullptr;
        std::vector<juce::RangedAudioParameter*> touched;
        bool draggingGlide = false, glidePaintState = false;
        int lastStep = -1;
        float lastValue = 0.0f;
    };
}

//==============================================================================
/** La façade, dessinée à sa taille d'origine puis mise à l'échelle par l'éditeur. */
class SequencesFacade final : public juce::Component,
                              private juce::Timer
{
public:
    static constexpr int width = 1240, height = 860;

    explicit SequencesFacade (SequencesAudioProcessor&);
    ~SequencesFacade() override;

    void paint (juce::Graphics&) override;
    void resized() override {}

    phragma::PresetBar& getPresetBar() noexcept       { return presetBar; }

    /** Pour les tests : boutons d'un séquenceur. */
    juce::TextButton& getRandomButton (int seq)       { return seqUis[(size_t) seq]->random; }
    ui::StepGrid& getGrid (int seq)                   { return *seqUis[(size_t) seq]->grid; }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::String label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Choice
    {
        juce::ComboBox box;
        juce::String label;
        std::unique_ptr<ComboAttachment> attachment;
    };

    struct Panel
    {
        juce::String title;
        juce::Rectangle<int> bounds;
    };

    struct SeqUi
    {
        std::unique_ptr<ui::StepGrid> grid;
        juce::TextButton on, random, left, right, clear;
        std::unique_ptr<ButtonAttachment> onAttachment;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;

    Knob& addKnob (const juce::String& id, const juce::String& label, juce::Rectangle<int> cell, bool bipolar = false);
    Choice& addChoice (const juce::String& id, const juce::String& label, juce::Rectangle<int> cell);
    Choice& addInlineChoice (const juce::String& id, juce::Rectangle<int> area);
    Knob& addBar (const juce::String& id, const juce::String& label, juce::Rectangle<int> area, bool bipolar);
    void buildSequencer (int seq, juce::Rectangle<int> area);

    SequencesAudioProcessor& audioProcessor;
    ui::LookAndFeel lookAndFeel;
    phragma::PresetBar presetBar;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<Panel> panels;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> captions;
    std::array<std::unique_ptr<SeqUi>, seqs::numSeqs> seqUis;
    std::array<int, seqs::numSeqs> shownPlayhead { -2, -2 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencesFacade)
};

//==============================================================================
/** L'éditeur : la façade, mise à l'échelle de la fenêtre (redimensionnable, proportions gardées). */
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
