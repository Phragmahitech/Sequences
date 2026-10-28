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
ui::StepMatrix::StepMatrix (SequencesAudioProcessor& p) : processor (p)
{
    for (int i = 0; i < SequencesAudioProcessor::numSteps; ++i)
        stepParams[(size_t) i] = processor.apvts.getParameter (SequencesAudioProcessor::stepId (i));
    stepsParam = processor.apvts.getParameter ("seqSteps");
    setTitle ("Sequencer steps");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> ui::StepMatrix::gridArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft ((float) labelWidth);
}

int ui::StepMatrix::columnAt (float x) const
{
    const auto grid = gridArea();
    const float w = grid.getWidth() / (float) SequencesAudioProcessor::numSteps;
    return juce::jlimit (0, SequencesAudioProcessor::numSteps - 1, (int) std::floor ((x - grid.getX()) / w));
}

int ui::StepMatrix::rowAt (float y) const
{
    const float h = (float) getHeight() / (float) numRows;
    return juce::jlimit (0, numRows - 1, (int) std::floor (y / h));
}

void ui::StepMatrix::setStep (int step, int source)
{
    auto* param = stepParams[(size_t) step];
    if (std::find (touched.begin(), touched.end(), param) == touched.end())
    {
        param->beginChangeGesture();
        touched.push_back (param);
    }
    param->setValueNotifyingHost (param->convertTo0to1 ((float) source));
}

void ui::StepMatrix::mouseDown (const juce::MouseEvent& e)
{
    if (e.position.x < (float) labelWidth)
        return;

    const int column = columnAt (e.position.x);
    setStep (column, e.mods.isPopupMenu() ? 0 : sourceForRow (rowAt (e.position.y)));
    lastColumn = column;
    repaint();
}

void ui::StepMatrix::mouseDrag (const juce::MouseEvent& e)
{
    if (lastColumn < 0)
        return;

    // On peint la ligne sous la souris sur toutes les colonnes traversées
    const int column = columnAt (e.position.x);
    const int source = e.mods.isPopupMenu() ? 0 : sourceForRow (rowAt (e.position.y));
    for (int c = juce::jmin (column, lastColumn); c <= juce::jmax (column, lastColumn); ++c)
        setStep (c, source);
    lastColumn = column;
    repaint();
}

void ui::StepMatrix::mouseUp (const juce::MouseEvent&)
{
    for (auto* param : touched)
        param->endChangeGesture();
    touched.clear();
    lastColumn = -1;
}

void ui::StepMatrix::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.position.x < (float) labelWidth)
        return;
    setStep (columnAt (e.position.x), 0);
    mouseUp (e);
    repaint();
}

void ui::StepMatrix::paint (juce::Graphics& g)
{
    const int numSteps = SequencesAudioProcessor::numSteps;
    const int active = juce::roundToInt (stepsParam->convertFrom0to1 (stepsParam->getValue()));
    const int playhead = processor.getPlayheadStep();
    const auto grid = gridArea();
    const float cw = grid.getWidth() / (float) numSteps;
    const float rh = (float) getHeight() / (float) numRows;

    // Étiquettes des lignes
    for (int row = 0; row < numRows; ++row)
    {
        const int source = sourceForRow (row);
        const auto r = juce::Rectangle<float> (0.0f, (float) row * rh, (float) labelWidth - 6.0f, rh);
        g.setColour (Palette::source (source));
        if (source != 0)
            g.fillRoundedRectangle (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ 6.0f, r.getCentreY() }), 1.5f);
        g.setColour (source == 0 ? Palette::dim() : Palette::ink());
        g.setFont (font (9.5f, true, 0.1f));
        g.drawText (SequencesAudioProcessor::sourceNames()[source], r.withTrimmedLeft (14.0f), juce::Justification::centredLeft, false);
    }

    g.setColour (juce::Colours::white);
    g.fillRect (grid);

    for (int c = 0; c < numSteps; ++c)
    {
        const auto column = juce::Rectangle<float> (grid.getX() + (float) c * cw, 0.0f, cw, (float) getHeight());
        const bool inLoop = c < active;

        if ((c / 4) % 2 == 1)
        {
            g.setColour (Palette::stepOff().withAlpha (0.55f));
            g.fillRect (column);
        }
        if (c == playhead)
        {
            g.setColour (Palette::accent().withAlpha (0.14f));
            g.fillRect (column);
        }

        const int source = juce::roundToInt (stepParams[(size_t) c]->convertFrom0to1 (stepParams[(size_t) c]->getValue()));
        const int row = rowForSource (source);
        const auto cell = juce::Rectangle<float> (column.getX(), (float) row * rh, cw, rh).reduced (2.0f, 2.0f);
        auto colour = Palette::source (source);
        if (! inLoop)
            colour = colour.withMultipliedSaturation (0.2f).withAlpha (0.35f);
        g.setColour (colour);
        if (source == 0)
            g.drawRoundedRectangle (cell, 2.0f, 1.2f);
        else
            g.fillRoundedRectangle (cell, 2.0f);
    }

    // Lignes horizontales légères
    g.setColour (Palette::stepOff());
    for (int row = 1; row < numRows; ++row)
        g.fillRect (grid.getX(), (float) row * rh - 0.5f, grid.getWidth(), 1.0f);

    if (active < numSteps)
    {
        g.setColour (Palette::ink());
        g.fillRect (juce::Rectangle<float> (grid.getX() + (float) active * cw - 1.0f, 0.0f, 2.0f, (float) getHeight()));
    }

    g.setColour (Palette::panelLine());
    g.drawRect (grid, 1.0f);
}

//==============================================================================
void ui::CircuitView::paint (juce::Graphics& g)
{
    processor.readSettings (settings);
    const auto& q = settings.seq;
    const double bpm = processor.getBpm();
    const double loopBeats = q.on ? (double) q.steps * q.stepBeats : 4.0;

    const auto area = getLocalBounds().toFloat();
    const auto plot = area.withTrimmedLeft ((float) StepMatrix::labelWidth);
    g.setColour (juce::Colours::white);
    g.fillRect (plot);

    // Libellé
    g.setColour (Palette::ink());
    g.setFont (font (9.5f, true, 0.12f));
    g.drawText ("LFO", area.withWidth ((float) StepMatrix::labelWidth - 6.0f).withHeight (area.getHeight() * 0.5f),
                juce::Justification::bottomLeft, false);
    g.drawText ("CIRCUIT", area.withWidth ((float) StepMatrix::labelWidth - 6.0f).withTrimmedTop (area.getHeight() * 0.5f).withHeight (14.0f),
                juce::Justification::topLeft, false);

    // Pas et ligne du zéro
    if (q.on)
    {
        g.setColour (Palette::stepOff());
        for (int s = 1; s < q.steps; ++s)
        {
            const auto pos = seqs::stepAt ((double) s * q.stepBeats + 1.0e-9, q.steps, q.stepBeats, q.swing);
            const float x = plot.getX() + (float) (pos.start / loopBeats) * plot.getWidth();
            g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
        }
    }

    const float mid = plot.getCentreY();
    const float amp = plot.getHeight() * 0.44f;
    g.setColour (Palette::panelLine());
    g.fillRect (plot.getX(), mid, plot.getWidth(), 1.0f);

    // Courbe, colorée par source
    const int n = juce::roundToInt (plot.getWidth());
    juce::Point<float> prev;
    for (int i = 0; i <= n; ++i)
    {
        const double t = loopBeats * (double) i / (double) n;
        const auto out = seqs::Mod::seq (settings, t, bpm, 0x5EEDull);
        const juce::Point<float> pt (plot.getX() + (float) i, mid - juce::jlimit (-1.1f, 1.1f, out.value) * amp);
        if (i > 0)
        {
            g.setColour (Palette::source (out.source == 0 ? 0 : out.source));
            g.drawLine ({ prev, pt }, 1.8f);
        }
        prev = pt;
    }

    // Tête de lecture
    const double beats = processor.getPlayheadBeats();
    if (beats >= 0.0 && loopBeats > 0.0)
    {
        const double inLoop = std::fmod (beats, loopBeats);
        const float x = plot.getX() + (float) (inLoop / loopBeats) * plot.getWidth();
        g.setColour (Palette::accent());
        g.fillRect (x - 1.0f, plot.getY(), 2.0f, plot.getHeight());
    }

    if (! q.on)
    {
        g.setColour (Palette::dim());
        g.setFont (font (9.5f, true, 0.12f));
        g.drawText ("SEQ OFF : " + SequencesAudioProcessor::sourceNames()[q.manual],
                    plot.reduced (8.0f, 4.0f), juce::Justification::topRight, false);
    }

    g.setColour (Palette::panelLine());
    g.drawRect (plot, 1.0f);
}

//==============================================================================
void ui::LfoPreview::paint (juce::Graphics& g)
{
    processor.readSettings (settings);
    const auto& l = settings.lfo[(size_t) lfo];
    const auto area = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (juce::Colours::white);
    g.fillRoundedRectangle (area, 2.0f);
    g.setColour (Palette::stepOff());
    g.fillRect (area.getX(), area.getCentreY(), area.getWidth(), 1.0f);

    juce::Path path;
    const int n = juce::roundToInt (area.getWidth());
    for (int i = 0; i <= n; ++i)
    {
        const double t = l.periodBeats * (double) i / (double) n;
        const float v = seqs::Mod::lfo (l, t * 0.9999);
        const juce::Point<float> pt (area.getX() + (float) i, area.getCentreY() - v * area.getHeight() * 0.42f);
        if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
    }
    g.setColour (Palette::source (lfo + 1));
    g.strokePath (path, juce::PathStrokeType (1.5f));
    g.setColour (Palette::panelLine());
    g.drawRoundedRectangle (area, 2.0f, 1.0f);
}

//==============================================================================
SequencesFacade::SequencesFacade (SequencesAudioProcessor& p)
    : audioProcessor (p),
      presetBar (p.presets, [] (float h, bool bold, float tracking) { return ui::font (h, bold, tracking); })
{
    setLookAndFeel (&lookAndFeel);
    setSize (width, height);

    constexpr int margin = 16, gap = 12, pad = 12, titleHeight = 24, cellHeight = 112;

    auto makeRow = [&] (int top, int rowHeight, std::vector<std::pair<const char*, int>> layout)
    {
        int cells = 0;
        for (auto& [name, count] : layout) cells += count;
        const int n = (int) layout.size();
        const int cell = (width - 2 * margin - n * 2 * pad - (n - 1) * gap) / cells;
        std::vector<juce::Rectangle<int>> rects;
        int x = margin;
        for (auto& [name, count] : layout)
        {
            const juce::Rectangle<int> r (x, top, count * cell + 2 * pad, rowHeight);
            panels.push_back ({ name, r, {} });
            rects.push_back (r);
            x += r.getWidth() + gap;
        }
        return std::make_pair (rects, cell);
    };

    // --- Rangée 1 : FM, OSC, VOICE ---
    {
        auto [r, cell] = makeRow (56, 150, { { "FM", 7 }, { "OSC", 7 }, { "VOICE", 5 } });
        auto at = [&, cell = cell] (juce::Rectangle<int> panel, int i)
        { return juce::Rectangle<int> (panel.getX() + pad + i * cell, panel.getY() + titleHeight + 6, cell, cellHeight); };

        addKnob ("fmLevel", "LEVEL", at (r[0], 0));
        addKnob ("fmRatio", "RATIO", at (r[0], 1));
        addKnob ("fmIndex", "INDEX", at (r[0], 2));
        addKnob ("fmFeedback", "FEEDBACK", at (r[0], 3));
        addKnob ("fmEnv", "MOD ENV", at (r[0], 4), true);
        addChoice ("modWave", "MOD WAVE", at (r[0], 5).withWidth (cell + 8));
        addKnob ("timbre", "TIMBRE", at (r[0], 6).withTrimmedLeft (8).withWidth (cell));

        addChoice ("oscWave", "WAVE", at (r[1], 0));
        addKnob ("oscLevel", "LEVEL", at (r[1], 1));
        addKnob ("oscOctave", "OCTAVE", at (r[1], 2), true);
        addKnob ("oscDetune", "DETUNE", at (r[1], 3), true);
        addKnob ("oscPw", "PW", at (r[1], 4));
        addKnob ("oscSync", "SYNC", at (r[1], 5));
        addKnob ("noise", "NOISE", at (r[1], 6));

        addChoice ("voiceMode", "MODE", at (r[2], 0));
        addKnob ("glide", "GLIDE", at (r[2], 1));
        addKnob ("velocity", "VELOCITY", at (r[2], 2));
        addKnob ("transpose", "TRANSP", at (r[2], 3), true);
        addKnob ("level", "LEVEL", at (r[2], 4));
    }

    // --- Rangée 2 : FILTER, DRIVE, AMP ENV, MOD ENV ---
    {
        auto [r, cell] = makeRow (214, 150, { { "FILTER", 5 }, { "DRIVE", 2 }, { "AMP ENV", 4 }, { "MOD ENV", 4 } });
        auto at = [&, cell = cell] (juce::Rectangle<int> panel, int i)
        { return juce::Rectangle<int> (panel.getX() + pad + i * cell, panel.getY() + titleHeight + 6, cell, cellHeight); };

        auto stacked = [&] (juce::Rectangle<int> c, const char* firstLabel, const char* firstId, const char* secondLabel, const char* secondId)
        {
            captions.push_back ({ firstLabel, c.withHeight (14) });
            addInlineChoice (firstId, c.withTrimmedTop (18).withHeight (26).reduced (3, 0));
            captions.push_back ({ secondLabel, c.withTrimmedTop (56).withHeight (14) });
            addInlineChoice (secondId, c.withTrimmedTop (74).withHeight (26).reduced (3, 0));
        };

        stacked (at (r[0], 0), "TYPE", "filterType", "SLOPE", "filterSlope");
        addKnob ("cutoff", "CUTOFF", at (r[0], 1));
        addKnob ("resonance", "RESO", at (r[0], 2));
        addKnob ("filterEnv", "MOD ENV", at (r[0], 3), true);
        addKnob ("keyTrack", "KEY TRACK", at (r[0], 4));

        stacked (at (r[1], 0), "TYPE", "driveType", "POSITION", "drivePos");
        addKnob ("drive", "AMOUNT", at (r[1], 1));

        addKnob ("ampAttack", "ATTACK", at (r[2], 0));
        addKnob ("ampDecay", "DECAY", at (r[2], 1));
        addKnob ("ampSustain", "SUSTAIN", at (r[2], 2));
        addKnob ("ampRelease", "RELEASE", at (r[2], 3));

        addKnob ("modAttack", "ATTACK", at (r[3], 0));
        addKnob ("modDecay", "DECAY", at (r[3], 1));
        addKnob ("modSustain", "SUSTAIN", at (r[3], 2));
        addKnob ("modRelease", "RELEASE", at (r[3], 3));
    }

    bankPanel = { margin, 372, width - 2 * margin, 216 };
    seqPanel = { margin, 596, width - 2 * margin, 316 };
    buildBank (bankPanel);
    buildSequencer (seqPanel);

    addAndMakeVisible (presetBar);
    presetBar.setBounds (width - 96 - phragma::PresetBar::idealWidth, 14, phragma::PresetBar::idealWidth, 28);
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

void SequencesFacade::buildBank (juce::Rectangle<int> area)
{
    const int columns = seqs::numSources;
    const int colWidth = (area.getWidth() - 24) / columns;
    const int y0 = area.getY() + 30;

    for (int c = 0; c < columns; ++c)
    {
        const int x = area.getX() + 12 + c * colWidth;
        const int w = colWidth - 12;
        const bool isLfo = c < seqs::numLfos;
        const juce::String name = SequencesAudioProcessor::sourceNames()[c + 1];
        panels.push_back ({ name, { x, y0, w, 18 }, ui::Palette::source (c + 1) });   // en-tête de colonne

        if (isLfo)
        {
            auto preview = std::make_unique<ui::LfoPreview> (audioProcessor, c);
            preview->setBounds (x + w - 70, y0, 70, 18);
            addAndMakeVisible (*preview);
            previews.push_back (std::move (preview));

            addLabelledChoice (SequencesAudioProcessor::lfoId (c, "Rate"), "RATE", { x, y0 + 24, w, 22 }, 40);
            addLabelledChoice (SequencesAudioProcessor::lfoId (c, "Shape"), "SHAPE", { x, y0 + 50, w, 22 }, 40);
            addLabelledChoice (SequencesAudioProcessor::lfoId (c, "Step"), "STEP", { x, y0 + 76, w / 2 + 14, 22 }, 40);
            addInlineChoice (SequencesAudioProcessor::lfoId (c, "Pol"), { x + w / 2 + 18, y0 + 76, w - w / 2 - 18, 22 });
            addKnob (SequencesAudioProcessor::lfoId (c, "Amt"), "AMT", { x, y0 + 102, w / 2, 82 }, false, true);
            addKnob (SequencesAudioProcessor::lfoId (c, "Phase"), "PHASE", { x + w / 2, y0 + 102, w - w / 2, 82 }, false, true);
        }
        else
        {
            const int k = c - seqs::numLfos;
            addLabelledChoice (SequencesAudioProcessor::chaosId (k, "Rate"), "RATE", { x, y0 + 24, w, 22 }, 40);
            addKnob (SequencesAudioProcessor::chaosId (k, "Amt"), "AMT", { x, y0 + 64, w / 2, 112 });
            addKnob (SequencesAudioProcessor::chaosId (k, "Smooth"), "SMOOTH", { x + w / 2, y0 + 64, w - w / 2, 112 });
        }
    }
}

void SequencesFacade::buildSequencer (juce::Rectangle<int> area)
{
    const int top = area.getY() + 10, h = 24;
    int x = area.getX() + 116;

    addToggle ("seqOn", "ON", { x, top, 44, h });                                   x += 44 + 8;
    addBar ("seqSteps", "STEPS", { x, top, 112, h }, false);                       x += 112 + 8;
    addLabelledChoice ("seqDiv", "DIV", { x, top, 92, h }, 30);                    x += 92 + 8;
    addInlineChoice ("seqFeel", { x, top, 94, h });                                x += 94 + 8;
    addLabelledChoice ("seqSync", "SYNC", { x, top, 106, h }, 36);                 x += 106 + 8;
    addBar ("seqSwing", "SWING", { x, top, 112, h }, false);                       x += 112 + 8;
    addToggle ("seqRestart", "RESTART", { x, top, 76, h });                         x += 76 + 8;
    addBar ("seqXfade", "XFADE", { x, top, 124, h }, false);                        x += 124 + 8;
    addLabelledChoice ("seqManual", "LFO SWITCH", { x, top, area.getRight() - 12 - x, h }, 76);

    // Matrice + LFO CIRCUIT
    const int gridWidth = ui::StepMatrix::labelWidth + SequencesAudioProcessor::numSteps * 25;
    matrix = std::make_unique<ui::StepMatrix> (audioProcessor);
    matrix->setBounds (area.getX() + 12, area.getY() + 46, gridWidth, ui::StepMatrix::numRows * 15);
    addAndMakeVisible (*matrix);

    circuit = std::make_unique<ui::CircuitView> (audioProcessor);
    circuit->setBounds (area.getX() + 12, matrix->getBottom() + 10, gridWidth, area.getBottom() - 12 - (matrix->getBottom() + 10));
    addAndMakeVisible (*circuit);

    // Boutons et routages, à droite
    const int rx = matrix->getRight() + 20, rw = area.getRight() - 12 - rx;
    int bx = area.getRight() - 12;
    auto placeRight = [&] (juce::TextButton& b, const juce::String& text, const juce::String& title, int w)
    {
        bx -= w;
        addButton (b, text, title, { bx, area.getY() + 46, w, 26 });
        bx -= 6;
    };
    randomAllButton.setComponentID ("random");
    randomButton.setComponentID ("random");
    placeRight (randomAllButton, "RANDOM ALL", "Random LFO bank and pattern", 100);
    placeRight (randomButton, "RANDOM", "Random pattern", 86);
    placeRight (clearButton, "CLEAR", "Clear pattern", 60);
    placeRight (rightButton, juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6")), "Shift right", 30);
    placeRight (leftButton, juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x80")), "Shift left", 30);

    randomButton.onClick = [this] { audioProcessor.randomizePattern(); repaint(); };
    randomAllButton.onClick = [this] { audioProcessor.randomizeAll(); repaint(); };
    clearButton.onClick = [this] { audioProcessor.fillPattern (0); repaint(); };
    leftButton.onClick = [this] { audioProcessor.rotatePattern (-1); repaint(); };
    rightButton.onClick = [this] { audioProcessor.rotatePattern (+1); repaint(); };

    for (int k = 0; k < seqs::numSlots; ++k)
    {
        const int y = area.getY() + 84 + k * 56;
        leftCaptions.push_back ({ "MOD " + juce::String (k + 1), { rx, y, rw, 14 } });
        addInlineChoice (SequencesAudioProcessor::slotId (k, "Src"), { rx, y + 16, 92, 26 });
        addInlineChoice (SequencesAudioProcessor::slotId (k, "Dest"), { rx + 98, y + 16, 112, 26 });
        addBar (SequencesAudioProcessor::slotId (k, "Amt"), "AMOUNT", { rx + 216, y + 16, rw - 216, 26 }, true);
    }
}

void SequencesFacade::addKnob (const juce::String& id, const juce::String& label, juce::Rectangle<int> cell, bool bipolar, bool small)
{
    auto k = std::make_unique<Knob>();
    auto& s = k->slider;
    const int captionHeight = small ? 12 : 16;
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, cell.getWidth(), small ? 14 : 18);
    s.setMouseDragSensitivity (220);
    s.setVelocityBasedMode (false);
    s.getProperties().set ("bipolar", bipolar);
    s.setTitle (label);
    s.setComponentID (id);
    s.setBounds (cell.withTrimmedTop (captionHeight));
    addAndMakeVisible (s);
    k->attachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, id, s);

    if (auto* param = audioProcessor.apvts.getParameter (id))
        s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    captions.push_back ({ label, cell.withHeight (captionHeight - 2) });
    knobs.push_back (std::move (k));
}

void SequencesFacade::addBar (const juce::String& id, const juce::String& label, juce::Rectangle<int> area, bool bipolar)
{
    auto k = std::make_unique<Knob>();
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
}

juce::ComboBox& SequencesFacade::addInlineChoice (const juce::String& id, juce::Rectangle<int> area)
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
    return choices.back()->box;
}

void SequencesFacade::addChoice (const juce::String& id, const juce::String& label, juce::Rectangle<int> cell)
{
    captions.push_back ({ label, cell.withHeight (14) });
    addInlineChoice (id, cell.withTrimmedTop (34).withHeight (26).reduced (3, 0));
}

void SequencesFacade::addLabelledChoice (const juce::String& id, const juce::String& label, juce::Rectangle<int> area, int labelWidth)
{
    leftCaptions.push_back ({ label, area.withWidth (labelWidth) });
    addInlineChoice (id, area.withTrimmedLeft (labelWidth));
}

void SequencesFacade::addToggle (const juce::String& id, const juce::String& text, juce::Rectangle<int> area)
{
    auto t = std::make_unique<Toggle>();
    t->button.setButtonText (text);
    t->button.setClickingTogglesState (true);
    t->button.setTitle (id);
    t->button.setComponentID (id);
    t->button.setWantsKeyboardFocus (false);
    t->button.setBounds (area);
    addAndMakeVisible (t->button);
    t->attachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, id, t->button);
    toggles.push_back (std::move (t));
}

void SequencesFacade::addButton (juce::TextButton& b, const juce::String& text, const juce::String& title, juce::Rectangle<int> area)
{
    b.setButtonText (text);
    b.setTitle (title);
    b.setWantsKeyboardFocus (false);
    b.setBounds (area);
    addAndMakeVisible (b);
}

void SequencesFacade::timerCallback()
{
    ++tick;
    const int step = audioProcessor.getPlayheadStep();
    if (step != shownStep || matrix->isMouseButtonDown())
    {
        shownStep = step;
        matrix->repaint();
    }

    const bool playing = audioProcessor.getPlayheadBeats() >= 0.0;
    if (playing || tick % 6 == 0)
        circuit->repaint();

    if (tick % 6 == 0)
        for (auto& p : previews)
            p->repaint();
}

void SequencesFacade::paint (juce::Graphics& g)
{
    using ui::Palette;
    g.fillAll (Palette::background());

    g.setColour (Palette::ink());
    g.setFont (ui::font (24.0f, true, 0.22f));
    g.drawText ("SEQUENCES", juce::Rectangle<int> (16, 10, 300, 36), juce::Justification::centredLeft, false);
    g.setColour (Palette::dim());
    g.setFont (ui::font (10.0f, true, 0.3f));
    g.drawText ("BY PHRAGMA", juce::Rectangle<int> (232, 10, 140, 36), juce::Justification::centredLeft, false);
    g.drawText ("V" SEQUENCES_VERSION, juce::Rectangle<int> (width - 86, 10, 70, 36), juce::Justification::centredRight, false);

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

    drawPanel (bankPanel, "LFO BANK");
    drawPanel (seqPanel, "");
    g.setColour (Palette::ink());
    g.setFont (ui::font (13.0f, true, 0.22f));
    g.drawText ("SEQUENCER", juce::Rectangle<int> (seqPanel.getX() + 12, seqPanel.getY() + 10, 104, 24), juce::Justification::centredLeft, false);

    for (const auto& p : panels)
    {
        if (p.swatch.isTransparent())
        {
            drawPanel (p.bounds, p.title);
            continue;
        }

        // En-tête d'une colonne de la banque : pastille + nom
        g.setColour (p.swatch);
        g.fillRoundedRectangle (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ (float) p.bounds.getX() + 5.0f, (float) p.bounds.getCentreY() }), 2.0f);
        g.setColour (Palette::ink());
        g.setFont (ui::font (11.0f, true, 0.2f));
        g.drawText (p.title, p.bounds.withTrimmedLeft (16), juce::Justification::centredLeft, false);
    }

    // Séparateurs de colonnes de la banque
    g.setColour (Palette::panelLine());
    const int colWidth = (bankPanel.getWidth() - 24) / seqs::numSources;
    for (int c = 1; c < seqs::numSources; ++c)
    {
        const int x = bankPanel.getX() + 12 + c * colWidth - 6;
        g.fillRect (x, bankPanel.getY() + 30, 1, bankPanel.getHeight() - 40);
    }

    g.setFont (ui::font (9.5f, true, 0.16f));
    g.setColour (Palette::dim());
    for (const auto& [text, r] : captions)
        g.drawText (text, r, juce::Justification::centred, false);
    for (const auto& [text, r] : leftCaptions)
        g.drawText (text, r, juce::Justification::centredLeft, false);
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
