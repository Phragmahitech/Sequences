#include "PluginEditor.h"

using ui::Palette;

juce::Font ui::font (float height, bool bold, float tracking)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (tracking);
}

//==============================================================================
ui::LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Palette::ink());
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Palette::accent().withAlpha (0.3f));
    setColour (juce::Label::textColourId, Palette::ink());
    setColour (juce::Label::textWhenEditingColourId, Palette::ink());
    setColour (juce::Label::backgroundWhenEditingColourId, juce::Colours::white);
    setColour (juce::Label::outlineWhenEditingColourId, Palette::accent());
    setColour (juce::TextEditor::highlightColourId, Palette::accent().withAlpha (0.3f));
    setColour (juce::TextEditor::highlightedTextColourId, Palette::ink());
    setColour (juce::CaretComponent::caretColourId, Palette::accent());
    setColour (juce::ComboBox::textColourId, Palette::ink());
    setColour (juce::ComboBox::arrowColourId, Palette::dim());
    setColour (juce::PopupMenu::backgroundColourId, juce::Colours::white);
    setColour (juce::PopupMenu::textColourId, Palette::ink());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::ink());
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void ui::LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto c = area.getCentre();
    const float angle = start + pos * (end - start);
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (start + end) * 0.5f : start;
    const float track = 3.0f;

    juce::Path bg;
    bg.addCentredArc (c.x, c.y, radius - track, radius - track, 0.0f, start, end, true);
    g.setColour (Palette::panelLine());
    g.strokePath (bg, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, radius - track, radius - track, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (s.isMouseOverOrDragging() ? Palette::accent() : Palette::ink());
        g.strokePath (arc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float knobRadius = radius - track - 5.0f;
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (c));
    g.setColour (Palette::panelLine());
    g.drawEllipse (juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (c), 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.25f, -knobRadius + 3.0f, 2.5f, knobRadius * 0.55f, 1.2f);
    g.setColour (Palette::ink());
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c));
}

void ui::LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                        juce::Slider::SliderStyle, juce::Slider& s)
{
    // Barre horizontale avec son nom et sa valeur dedans
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    g.setColour (juce::Colours::white);
    g.fillRoundedRectangle (area, 3.0f);

    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float zero = bipolar ? area.getCentreX() : area.getX();
    auto fill = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (zero, pos), area.getY(), juce::jmax (zero, pos), area.getBottom());
    g.setColour ((s.isMouseOverOrDragging() ? Palette::accent() : Palette::ink()).withAlpha (0.16f));
    g.fillRect (fill.reduced (0.0f, 1.0f));
    g.setColour (s.isMouseOverOrDragging() ? Palette::accent() : Palette::ink());
    g.fillRect (juce::Rectangle<float> (2.0f, area.getHeight() - 2.0f).withCentre ({ pos, area.getCentreY() }));

    g.setColour (Palette::panelLine());
    g.drawRoundedRectangle (area.reduced (0.5f), 3.0f, 1.0f);

    const auto inner = area.reduced (8.0f, 0.0f);
    g.setFont (ui::font (10.0f, true, 0.12f));
    g.setColour (Palette::dim());
    g.drawText (s.getProperties().getWithDefault ("label", "").toString(), inner, juce::Justification::centredLeft, false);
    g.setColour (Palette::ink());
    g.setFont (ui::font (11.0f, true, 0.02f));
    g.drawText (s.getTextFromValue (s.getValue()), inner, juce::Justification::centredRight, false);
}

juce::Label* ui::LookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (s);
    label->setFont (ui::font (11.0f, true, 0.02f));
    label->setJustificationType (juce::Justification::centred);
    label->setColour (juce::Label::textColourId, Palette::ink());
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return label;
}

void ui::LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto area = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const bool accent = b.getComponentID() == "random";

    juce::Colour fill = juce::Colours::white;
    if (on) fill = Palette::ink();
    if (accent) fill = Palette::accent();
    if (down) fill = fill.darker (0.15f);

    g.setColour (fill);
    g.fillRoundedRectangle (area, 3.0f);
    g.setColour (accent ? Palette::accent().darker (0.2f) : (over ? Palette::ink() : Palette::panelLine()));
    g.drawRoundedRectangle (area, 3.0f, 1.0f);
}

void ui::LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool light = b.getToggleState() || b.getComponentID() == "random";
    g.setColour (light ? juce::Colours::white : Palette::ink());
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

void ui::LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto area = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (juce::Colours::white);
    g.fillRoundedRectangle (area, 3.0f);
    g.setColour (box.isMouseOver (true) ? Palette::ink() : Palette::panelLine());
    g.drawRoundedRectangle (area, 3.0f, 1.0f);

    juce::Path caret;
    const float cx = (float) w - 11.0f, cy = (float) h * 0.5f;
    caret.addTriangle (cx - 3.5f, cy - 1.5f, cx + 3.5f, cy - 1.5f, cx, cy + 2.5f);
    g.setColour (Palette::dim());
    g.fillPath (caret);
}

void ui::LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 0, box.getWidth() - 20, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

//==============================================================================
ui::StepGrid::StepGrid (SequencesAudioProcessor& p, int seqIndex) : processor (p), seq (seqIndex)
{
    for (int i = 0; i < SequencesAudioProcessor::numSteps; ++i)
    {
        valueParams[(size_t) i] = processor.apvts.getParameter (SequencesAudioProcessor::stepId (seq, i));
        glideParams[(size_t) i] = processor.apvts.getParameter (SequencesAudioProcessor::glideId (seq, i));
    }
    stepsParam = processor.apvts.getParameter (SequencesAudioProcessor::seqId (seq, "Steps"));
    setTitle (seq == 0 ? "Sequencer A steps" : "Sequencer B steps");
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

juce::Rectangle<float> ui::StepGrid::barsArea() const
{
    return getLocalBounds().toFloat().withTrimmedBottom ((float) glideRowHeight + 6.0f);
}

juce::Rectangle<float> ui::StepGrid::glideArea() const
{
    return getLocalBounds().toFloat().removeFromBottom ((float) glideRowHeight);
}

int ui::StepGrid::stepAtX (float x) const
{
    const float w = (float) getWidth() / (float) SequencesAudioProcessor::numSteps;
    return juce::jlimit (0, SequencesAudioProcessor::numSteps - 1, (int) std::floor (x / w));
}

float ui::StepGrid::valueAtY (float y) const
{
    const auto bars = barsArea();
    return juce::jlimit (0.0f, 1.0f, 1.0f - (y - bars.getY()) / bars.getHeight());
}

void ui::StepGrid::touch (juce::RangedAudioParameter* param)
{
    if (std::find (touched.begin(), touched.end(), param) == touched.end())
    {
        param->beginChangeGesture();
        touched.push_back (param);
    }
}

void ui::StepGrid::setStepValue (int step, float value)
{
    auto* param = valueParams[(size_t) step];
    touch (param);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void ui::StepGrid::setGlide (int step, bool on)
{
    auto* param = glideParams[(size_t) step];
    touch (param);
    param->setValueNotifyingHost (on ? 1.0f : 0.0f);
}

void ui::StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.position;
    const int step = stepAtX (pos.x);

    if (pos.y >= glideArea().getY() - 3.0f)
    {
        draggingGlide = true;
        glidePaintState = glideParams[(size_t) step]->getValue() < 0.5f;
        setGlide (step, glidePaintState);
    }
    else
    {
        draggingGlide = false;
        lastValue = valueAtY (pos.y);
        setStepValue (step, lastValue);
    }

    lastStep = step;
    repaint();
}

void ui::StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    const auto pos = e.position;
    const int step = stepAtX (pos.x);

    if (draggingGlide)
    {
        const int a = juce::jmin (step, lastStep), b = juce::jmax (step, lastStep);
        for (int i = a; i <= b; ++i)
            setGlide (i, glidePaintState);
    }
    else
    {
        // Tracé rapide : on remplit les pas sautés en ligne droite
        const float value = valueAtY (pos.y);
        if (step == lastStep)
            setStepValue (step, value);
        else
        {
            const int dir = step > lastStep ? 1 : -1;
            const int count = std::abs (step - lastStep);
            for (int k = 1; k <= count; ++k)
                setStepValue (lastStep + dir * k, lastValue + (value - lastValue) * (float) k / (float) count);
        }
        lastValue = value;
    }

    lastStep = step;
    repaint();
}

void ui::StepGrid::mouseUp (const juce::MouseEvent&)
{
    for (auto* param : touched)
        param->endChangeGesture();
    touched.clear();
}

void ui::StepGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Double-clic sur un pas : il revient à zéro
    if (e.position.y < glideArea().getY() - 3.0f)
    {
        setStepValue (stepAtX (e.position.x), 0.0f);
        mouseUp (e);
        repaint();
    }
}

void ui::StepGrid::paint (juce::Graphics& g)
{
    const int numSteps = SequencesAudioProcessor::numSteps;
    const int active = juce::roundToInt (stepsParam->convertFrom0to1 (stepsParam->getValue()));
    const int playhead = processor.getPlayheadStep (seq);
    const auto bars = barsArea();
    const auto glides = glideArea();
    const float w = (float) getWidth() / (float) numSteps;

    g.setColour (juce::Colours::white);
    g.fillRect (bars);

    for (int i = 0; i < numSteps; ++i)
    {
        const float x = (float) i * w;
        const bool inLoop = i < active;
        const auto column = juce::Rectangle<float> (x, bars.getY(), w, bars.getHeight());

        // Repère toutes les 4 cases
        if ((i / 4) % 2 == 1)
        {
            g.setColour (Palette::stepOff().withAlpha (0.6f));
            g.fillRect (column);
        }

        if (i == playhead)
        {
            g.setColour (Palette::accent().withAlpha (0.12f));
            g.fillRect (column);
        }

        const float value = valueParams[(size_t) i]->getValue();
        const bool glide = glideParams[(size_t) i]->getValue() > 0.5f;
        const float next = valueParams[(size_t) ((i + 1) % juce::jmax (1, active))]->getValue();
        const auto inner = column.reduced (2.0f, 0.0f);
        const float yLeft = bars.getBottom() - value * bars.getHeight();
        const float yRight = glide && inLoop ? bars.getBottom() - next * bars.getHeight() : yLeft;

        juce::Path bar;
        bar.startNewSubPath (inner.getX(), bars.getBottom());
        bar.lineTo (inner.getX(), yLeft);
        bar.lineTo (inner.getRight(), yRight);
        bar.lineTo (inner.getRight(), bars.getBottom());
        bar.closeSubPath();

        const auto colour = ! inLoop ? Palette::faint().withAlpha (0.6f) : i == playhead ? Palette::accent() : Palette::ink();
        g.setColour (colour.withAlpha (inLoop ? 0.85f : 0.5f));
        g.fillPath (bar);
        g.setColour (colour);
        g.drawLine (inner.getX(), yLeft, inner.getRight(), yRight, 2.0f);

        // Glide
        const auto cell = juce::Rectangle<float> (x, glides.getY(), w, glides.getHeight()).reduced (3.0f, 3.0f);
        if (glide)
        {
            g.setColour (inLoop ? Palette::accent() : Palette::faint());
            g.fillRoundedRectangle (cell, 2.0f);
            g.setColour (juce::Colours::white);
            g.drawLine (cell.getX() + 3.0f, cell.getBottom() - 3.0f, cell.getRight() - 3.0f, cell.getY() + 3.0f, 1.5f);
        }
        else
        {
            g.setColour (inLoop ? Palette::panelLine() : Palette::stepOff());
            g.drawRoundedRectangle (cell, 2.0f, 1.0f);
        }
    }

    // Fin de boucle
    if (active < numSteps)
    {
        g.setColour (Palette::ink());
        g.fillRect (juce::Rectangle<float> ((float) active * w - 1.0f, bars.getY(), 2.0f, bars.getHeight()));
    }

    g.setColour (Palette::panelLine());
    g.drawRect (bars, 1.0f);
}

//==============================================================================
SequencesFacade::SequencesFacade (SequencesAudioProcessor& p)
    : audioProcessor (p),
      presetBar (p.presets, [] (float h, bool bold, float tracking) { return ui::font (h, bold, tracking); })
{
    setLookAndFeel (&lookAndFeel);
    setSize (width, height);

    constexpr int margin = 16, gap = 12, pad = 12, titleHeight = 24, cellHeight = 112;

    // --- Rangée 1 : VOICE, FM, OSC ---
    {
        const int top = 64, rowHeight = 150, cells = 16, numPanels = 3;
        const int cell = (width - 2 * margin - numPanels * 2 * pad - (numPanels - 1) * gap) / cells;
        int x = margin;

        auto panel = [&] (const juce::String& title, int numCells)
        {
            const juce::Rectangle<int> r (x, top, numCells * cell + 2 * pad, rowHeight);
            panels.push_back ({ title, r });
            x += r.getWidth() + gap;
            return r;
        };
        auto cellAt = [&] (juce::Rectangle<int> r, int i) { return juce::Rectangle<int> (r.getX() + pad + i * cell, r.getY() + titleHeight + 6, cell, cellHeight); };

        auto voice = panel ("VOICE", 5);
        addChoice ("voiceMode", "MODE", cellAt (voice, 0));
        addKnob ("glide", "GLIDE", cellAt (voice, 1));
        addKnob ("velocity", "VELOCITY", cellAt (voice, 2));
        addKnob ("transpose", "TRANSPOSE", cellAt (voice, 3), true);
        addKnob ("level", "LEVEL", cellAt (voice, 4));

        auto fm = panel ("FM", 5);
        addKnob ("fmLevel", "LEVEL", cellAt (fm, 0));
        addKnob ("fmRatio", "RATIO", cellAt (fm, 1));
        addKnob ("fmIndex", "INDEX", cellAt (fm, 2));
        addKnob ("fmFeedback", "FEEDBACK", cellAt (fm, 3));
        addKnob ("fmEnv", "MOD ENV", cellAt (fm, 4), true);

        auto osc = panel ("OSC", 6);
        addChoice ("oscWave", "WAVE", cellAt (osc, 0));
        addKnob ("oscLevel", "LEVEL", cellAt (osc, 1));
        addKnob ("oscOctave", "OCTAVE", cellAt (osc, 2), true);
        addKnob ("oscDetune", "DETUNE", cellAt (osc, 3), true);
        addKnob ("oscPw", "PW", cellAt (osc, 4));
        addKnob ("noise", "NOISE", cellAt (osc, 5));
    }

    // --- Rangée 2 : FILTER, DRIVE, AMP ENV, MOD ENV ---
    {
        const int top = 222, rowHeight = 150, cells = 15, numPanels = 4;
        const int cell = (width - 2 * margin - numPanels * 2 * pad - (numPanels - 1) * gap) / cells;
        int x = margin;

        auto panel = [&] (const juce::String& title, int numCells)
        {
            const juce::Rectangle<int> r (x, top, numCells * cell + 2 * pad, rowHeight);
            panels.push_back ({ title, r });
            x += r.getWidth() + gap;
            return r;
        };
        auto cellAt = [&] (juce::Rectangle<int> r, int i) { return juce::Rectangle<int> (r.getX() + pad + i * cell, r.getY() + titleHeight + 6, cell, cellHeight); };

        // Deux menus empilés dans une case
        auto stacked = [&] (juce::Rectangle<int> c, const char* firstLabel, const char* firstId, const char* secondLabel, const char* secondId)
        {
            captions.push_back ({ firstLabel, c.withHeight (14) });
            addInlineChoice (firstId, c.withTrimmedTop (18).withHeight (26).reduced (3, 0));
            captions.push_back ({ secondLabel, c.withTrimmedTop (56).withHeight (14) });
            addInlineChoice (secondId, c.withTrimmedTop (74).withHeight (26).reduced (3, 0));
        };

        auto filter = panel ("FILTER", 5);
        stacked (cellAt (filter, 0), "TYPE", "filterType", "SLOPE", "filterSlope");
        addKnob ("cutoff", "CUTOFF", cellAt (filter, 1));
        addKnob ("resonance", "RESO", cellAt (filter, 2));
        addKnob ("filterEnv", "MOD ENV", cellAt (filter, 3), true);
        addKnob ("keyTrack", "KEY TRACK", cellAt (filter, 4));

        auto drive = panel ("DRIVE", 2);
        stacked (cellAt (drive, 0), "TYPE", "driveType", "POSITION", "drivePos");
        addKnob ("drive", "AMOUNT", cellAt (drive, 1));

        auto amp = panel ("AMP ENV", 4);
        addKnob ("ampAttack", "ATTACK", cellAt (amp, 0));
        addKnob ("ampDecay", "DECAY", cellAt (amp, 1));
        addKnob ("ampSustain", "SUSTAIN", cellAt (amp, 2));
        addKnob ("ampRelease", "RELEASE", cellAt (amp, 3));

        auto modEnv = panel ("MOD ENV", 4);
        addKnob ("modAttack", "ATTACK", cellAt (modEnv, 0));
        addKnob ("modDecay", "DECAY", cellAt (modEnv, 1));
        addKnob ("modSustain", "SUSTAIN", cellAt (modEnv, 2));
        addKnob ("modRelease", "RELEASE", cellAt (modEnv, 3));

    }

    // --- Séquenceurs ---
    buildSequencer (0, { margin, 382, width - 2 * margin, 232 });
    buildSequencer (1, { margin, 622, width - 2 * margin, 232 });

    // --- En-tête : presets + taille ---
    addAndMakeVisible (presetBar);
    presetBar.setBounds (width - 96 - phragma::PresetBar::idealWidth, 16, phragma::PresetBar::idealWidth, 28);
    presetBar.attachSavePanelTo (*this);

    // Les enfants ont été créés avant d'être posés sur la façade : on leur applique son look
    sendLookAndFeelChange();

    startTimerHz (30);
}

SequencesFacade::~SequencesFacade()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

SequencesFacade::Knob& SequencesFacade::addKnob (const juce::String& id, const juce::String& label, juce::Rectangle<int> cell, bool bipolar)
{
    auto k = std::make_unique<Knob>();
    k->label = label;
    auto& s = k->slider;
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, cell.getWidth(), 18);
    s.setMouseDragSensitivity (220);
    s.setVelocityBasedMode (false);
    s.getProperties().set ("bipolar", bipolar);
    s.setTitle (label);
    s.setComponentID (id);
    s.setBounds (cell.withTrimmedTop (16).withHeight (cell.getHeight() - 16));
    addAndMakeVisible (s);
    k->attachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, id, s);

    if (auto* param = audioProcessor.apvts.getParameter (id))
        s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    captions.push_back ({ label, cell.withHeight (14) });
    knobs.push_back (std::move (k));
    return *knobs.back();
}

SequencesFacade::Knob& SequencesFacade::addBar (const juce::String& id, const juce::String& label, juce::Rectangle<int> area, bool bipolar)
{
    auto k = std::make_unique<Knob>();
    k->label = label;
    auto& s = k->slider;
    s.setSliderStyle (juce::Slider::LinearBar);
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setVelocityBasedMode (false);
    s.getProperties().set ("bipolar", bipolar);
    s.getProperties().set ("label", label);
    s.setTitle (label);
    s.setComponentID (id);
    s.setBounds (area);
    addAndMakeVisible (s);
    k->attachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, id, s);

    if (auto* param = audioProcessor.apvts.getParameter (id))
        s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    knobs.push_back (std::move (k));
    return *knobs.back();
}

SequencesFacade::Choice& SequencesFacade::addInlineChoice (const juce::String& id, juce::Rectangle<int> area)
{
    auto c = std::make_unique<Choice>();
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (audioProcessor.apvts.getParameter (id)))
        c->box.addItemList (param->choices, 1);
    c->box.setTitle (id);
    c->box.setComponentID (id);
    c->box.setBounds (area);
    addAndMakeVisible (c->box);
    c->attachment = std::make_unique<ComboAttachment> (audioProcessor.apvts, id, c->box);
    choices.push_back (std::move (c));
    return *choices.back();
}

SequencesFacade::Choice& SequencesFacade::addChoice (const juce::String& id, const juce::String& label, juce::Rectangle<int> cell)
{
    captions.push_back ({ label, cell.withHeight (14) });
    auto& c = addInlineChoice (id, cell.withTrimmedTop (34).withHeight (26).reduced (3, 0));
    c.label = label;
    return c;
}

void SequencesFacade::buildSequencer (int seq, juce::Rectangle<int> area)
{
    auto u = std::make_unique<SeqUi>();
    u->bounds = area;
    const auto id = [seq] (const char* name) { return SequencesAudioProcessor::seqId (seq, name); };

    const int top = area.getY() + 10, h = 24;
    int x = area.getX() + 76;

    u->on.setButtonText ("ON");
    u->on.setClickingTogglesState (true);
    u->on.setTitle (seq == 0 ? "Sequencer A on" : "Sequencer B on");
    u->on.setBounds (x, top, 44, h);
    addAndMakeVisible (u->on);
    u->onAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, id ("On"), u->on);
    x += 44 + 10;

    addBar (id ("Steps"), "STEPS", { x, top, 112, h }, false);             x += 112 + 10;
    captions.push_back ({ "RATE", { x, top, 36, h } });
    addInlineChoice (id ("Rate"), { x + 38, top, 70, h });                   x += 108 + 10;
    addInlineChoice (id ("Feel"), { x, top, 100, h });                       x += 100 + 10;
    captions.push_back ({ "SYNC", { x, top, 36, h } });
    addInlineChoice (id ("Sync"), { x + 38, top, 76, h });                   x += 114 + 10;
    addBar (id ("Swing"), "SWING", { x, top, 128, h }, false);              x += 128 + 10;
    addBar (id ("Smooth"), "SMOOTH", { x, top, 136, h }, false);

    // Boutons à droite
    int right = area.getRight() - 12;
    auto placeButton = [&] (juce::TextButton& b, const juce::String& text, int w)
    {
        right -= w;
        b.setButtonText (text);
        b.setBounds (right, top, w, h);
        b.setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
        right -= 6;
    };

    u->random.setComponentID ("random");
    placeButton (u->random, "RANDOM", 92);
    placeButton (u->clear, "CLEAR", 58);
    placeButton (u->right, juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6")), 30);
    placeButton (u->left, juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x80")), 30);
    u->random.setTitle ("Random pattern");
    u->clear.setTitle ("Clear pattern");
    u->left.setTitle ("Shift left");
    u->right.setTitle ("Shift right");

    u->random.onClick = [this, seq] { audioProcessor.randomizeSequence (seq); repaint(); };
    u->clear.onClick = [this, seq] { audioProcessor.clearSequence (seq, 0.0f); repaint(); };
    u->left.onClick = [this, seq] { audioProcessor.rotateSequence (seq, -1); repaint(); };
    u->right.onClick = [this, seq] { audioProcessor.rotateSequence (seq, +1); repaint(); };

    // Grille
    const int gridWidth = 32 * 26;
    u->grid = std::make_unique<ui::StepGrid> (audioProcessor, seq);
    u->grid->setBounds (area.getX() + 12, area.getY() + 46, gridWidth, area.getHeight() - 46 - 12);
    addAndMakeVisible (*u->grid);

    // Destinations
    const int destX = area.getX() + 12 + gridWidth + 20;
    const int destWidth = area.getRight() - 12 - destX;
    for (int k = 0; k < seqs::numSlots; ++k)
    {
        const int y = area.getY() + 46 + k * 58;
        captions.push_back ({ "DEST " + juce::String (k + 1), { destX, y, destWidth, 14 } });
        addInlineChoice (SequencesAudioProcessor::destId (seq, k), { destX, y + 18, 120, 26 });
        addBar (SequencesAudioProcessor::amountId (seq, k), "AMOUNT", { destX + 128, y + 18, destWidth - 128, 26 }, true);
    }

    seqUis[(size_t) seq] = std::move (u);
}

void SequencesFacade::timerCallback()
{
    for (int q = 0; q < seqs::numSeqs; ++q)
    {
        const int step = audioProcessor.getPlayheadStep (q);
        if (step != shownPlayhead[(size_t) q] || seqUis[(size_t) q]->grid->isMouseButtonDown())
        {
            shownPlayhead[(size_t) q] = step;
            seqUis[(size_t) q]->grid->repaint();
        }
    }
}

void SequencesFacade::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background());

    // En-tête
    g.setColour (Palette::ink());
    g.setFont (ui::font (24.0f, true, 0.22f));
    g.drawText ("SEQUENCES", juce::Rectangle<int> (16, 12, 300, 36), juce::Justification::centredLeft, false);
    g.setColour (Palette::dim());
    g.setFont (ui::font (10.0f, true, 0.3f));
    g.drawText ("BY PHRAGMA", juce::Rectangle<int> (232, 12, 140, 36), juce::Justification::centredLeft, false);
    g.drawText ("V" SEQUENCES_VERSION, juce::Rectangle<int> (width - 86, 12, 70, 36), juce::Justification::centredRight, false);

    // Panneaux
    auto drawPanel = [&g] (juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (Palette::panel());
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (Palette::panelLine());
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (Palette::ink());
        g.setFont (ui::font (11.0f, true, 0.28f));
        g.drawText (title, r.withTrimmedLeft (12).withTrimmedTop (8).withHeight (16), juce::Justification::centredLeft, false);
    };

    for (const auto& p : panels)
        drawPanel (p.bounds, p.title);

    for (int q = 0; q < seqs::numSeqs; ++q)
    {
        const auto& u = *seqUis[(size_t) q];
        g.setColour (Palette::panel());
        g.fillRoundedRectangle (u.bounds.toFloat(), 6.0f);
        g.setColour (Palette::panelLine());
        g.drawRoundedRectangle (u.bounds.toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (Palette::ink());
        g.setFont (ui::font (13.0f, true, 0.24f));
        g.drawText (q == 0 ? "SEQ A" : "SEQ B", juce::Rectangle<int> (u.bounds.getX() + 12, u.bounds.getY() + 10, 64, 24),
                    juce::Justification::centredLeft, false);
    }

    // Légendes
    g.setFont (ui::font (9.5f, true, 0.16f));
    g.setColour (Palette::dim());
    for (const auto& [text, r] : captions)
        g.drawText (text, r, r.getHeight() > 20 ? juce::Justification::centredLeft : juce::Justification::centred, false);
}

//==============================================================================
SequencesAudioProcessorEditor::SequencesAudioProcessorEditor (SequencesAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), facade (p)
{
    addAndMakeVisible (facade);

    // Taille gardée avec le projet (lue avant de régler les limites : les régler redimensionne déjà la fenêtre).
    const int savedWidth = (int) audioProcessor.apvts.state.getProperty ("editorWidth", SequencesFacade::width);

    setResizable (true, true);
    setResizeLimits (juce::roundToInt (SequencesFacade::width * 0.6), juce::roundToInt (SequencesFacade::height * 0.6),
                     juce::roundToInt (SequencesFacade::width * 1.5), juce::roundToInt (SequencesFacade::height * 1.5));
    getConstrainer()->setFixedAspectRatio ((double) SequencesFacade::width / (double) SequencesFacade::height);

    facade.getPresetBar().getScale = [this] { return getUiScale(); };
    facade.getPresetBar().setScale = [this] (float scale) { setUiScale (scale); };

    const int shownWidth = juce::jlimit (juce::roundToInt (SequencesFacade::width * 0.6), juce::roundToInt (SequencesFacade::width * 1.5), savedWidth);
    setSize (shownWidth, juce::roundToInt ((double) shownWidth * SequencesFacade::height / SequencesFacade::width));
    sized = true;
    resized();
}

void SequencesAudioProcessorEditor::setUiScale (float scale)
{
    scale = juce::jlimit (0.6f, 1.5f, scale);
    setSize (juce::roundToInt ((float) SequencesFacade::width * scale), juce::roundToInt ((float) SequencesFacade::height * scale));
}

void SequencesAudioProcessorEditor::resized()
{
    facade.setTransform (juce::AffineTransform::scale (getUiScale()));
    facade.getPresetBar().refresh();

    if (sized)
        audioProcessor.apvts.state.setProperty ("editorWidth", getWidth(), nullptr);
}
