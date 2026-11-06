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
ui::SourceChip::SourceChip (int s, juce::String t) : source (s), text (std::move (t))
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTitle ("Modulation source " + text);
    setDescription ("Drag onto a knob to modulate it");
}

int ui::SourceChip::sourceFromDescription (const juce::var& description)
{
    const auto s = description.toString();
    if (! s.startsWith ("modsrc:"))
        return -1;
    const int v = s.fromFirstOccurrenceOf ("modsrc:", false, false).getIntValue();
    return juce::isPositiveAndBelow (v, (int) seqs::numModSources) ? v : -1;
}

void ui::SourceChip::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (0.5f);
    const auto colour = Palette::modSource (source);
    const bool over = isMouseOver (true) || dragging;

    g.setColour (over ? colour.withAlpha (0.12f) : juce::Colours::white);
    g.fillRoundedRectangle (area, 3.0f);
    g.setColour (over ? colour : Palette::panelLine());
    g.drawRoundedRectangle (area, 3.0f, over ? 1.4f : 1.0f);

    g.setColour (colour);
    g.fillRoundedRectangle (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ area.getX() + 9.0f, area.getCentreY() }), 2.0f);

    g.setColour (Palette::ink());
    g.setFont (font (10.0f, true, 0.1f));
    g.drawText (text, area.withTrimmedLeft (17.0f).withTrimmedRight (12.0f), juce::Justification::centredLeft, false);

    // Poignée
    g.setColour (over ? colour : Palette::faint());
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 2; ++c)
            g.fillEllipse (area.getRight() - 10.0f + (float) c * 3.5f, area.getCentreY() - 4.5f + (float) r * 3.5f, 2.0f, 2.0f);
}

void ui::SourceChip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 4)
        return;

    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragging = true;
        container->startDragging (dragDescription (source), this);
        repaint();
    }
}

void ui::SourceChip::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    repaint();
}

//==============================================================================
ui::ModKnob::ModKnob (SequencesAudioProcessor& p, seqs::Dest d) : processor (p), dest (d) {}

juce::Rectangle<float> ui::ModKnob::rotaryArea() const
{
    return getLookAndFeel().getSliderLayout (const_cast<ModKnob&> (*this)).sliderBounds.toFloat().reduced (4.0f);
}

bool ui::ModKnob::isOnRing (juce::Point<float> pt) const
{
    const auto area = rotaryArea();
    const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const float d = pt.getDistanceFrom (area.getCentre());
    return d >= radius - 7.0f && d <= radius + 5.0f;
}

int ui::ModKnob::slotForDepth() const
{
    if (juce::isPositiveAndBelow (activeSlot, seqs::numSlots) && processor.getModulationDest (activeSlot) == dest)
        return activeSlot;
    const auto slots = processor.slotsForDest (dest);
    return slots.empty() ? -1 : slots.back();
}

void ui::ModKnob::paint (juce::Graphics& g)
{
    juce::Slider::paint (g);

    const auto area = rotaryArea();
    const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto c = area.getCentre();
    const auto rotary = getRotaryParameters();
    const float base = (float) getNormalisableRange().convertTo0to1 (getValue());
    auto angleAt = [&rotary] (float n) { return rotary.startAngleRadians + juce::jlimit (0.0f, 1.0f, n) * (rotary.endAngleRadians - rotary.startAngleRadians); };

    const auto slots = processor.slotsForDest (dest);
    const int depth = slotForDepth();
    float ringRadius = radius + 1.5f;

    for (auto it = slots.rbegin(); it != slots.rend(); ++it)
    {
        const int k = *it;
        const float amount = processor.getModulationAmount (k);
        const float a0 = angleAt (base), a1 = angleAt (base + amount);
        if (std::abs (a1 - a0) > 0.001f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, ringRadius, ringRadius, 0.0f, juce::jmin (a0, a1), juce::jmax (a0, a1), true);
            g.setColour (Palette::modSource (processor.getModulationSource (k)).withAlpha (k == depth ? 1.0f : 0.75f));
            g.strokePath (arc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }
        ringRadius -= 3.4f;
    }

    // Valeur modulée en ce moment (dernière voix jouée)
    if (! slots.empty() && processor.getPlayheadBeats() >= 0.0)
    {
        const float a = angleAt (base + processor.getLiveOffset (dest));
        const juce::Point<float> pt (c.x + (radius + 1.5f) * std::sin (a), c.y - (radius + 1.5f) * std::cos (a));
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (pt));
        g.setColour (Palette::ink());
        g.drawEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (pt), 1.4f);
    }

    if (dropHover)
    {
        g.setColour (Palette::accent());
        g.drawEllipse (juce::Rectangle<float> ((radius + 3.0f) * 2.0f, (radius + 3.0f) * 2.0f).withCentre (c), 2.0f);
    }
}

void ui::ModKnob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showMenu();
        return;
    }

    const int slot = slotForDepth();
    if (slot >= 0 && (e.mods.isAltDown() || isOnRing (e.position)))
    {
        depthSlot = slot;
        activeSlot = slot;
        depthStart = processor.getModulationAmount (slot);
        depthParam = processor.apvts.getParameter (SequencesAudioProcessor::slotId (slot, "Amt"));
        if (depthParam != nullptr)
            depthParam->beginChangeGesture();
        return;
    }

    juce::Slider::mouseDown (e);
}

void ui::ModKnob::mouseDrag (const juce::MouseEvent& e)
{
    if (depthSlot < 0)
    {
        juce::Slider::mouseDrag (e);
        return;
    }

    const float pixels = (float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY());
    const float amount = juce::jlimit (-1.0f, 1.0f, depthStart + pixels / (e.mods.isShiftDown() ? 800.0f : 200.0f));
    if (depthParam != nullptr)
        depthParam->setValueNotifyingHost (depthParam->convertTo0to1 (amount * 100.0f));
    repaint();
}

void ui::ModKnob::mouseUp (const juce::MouseEvent& e)
{
    if (depthSlot < 0)
    {
        juce::Slider::mouseUp (e);
        return;
    }

    if (depthParam != nullptr)
        depthParam->endChangeGesture();
    depthParam = nullptr;
    depthSlot = -1;
}

bool ui::ModKnob::isInterestedInDragSource (const SourceDetails& details)
{
    return dest != seqs::Dest::off && SourceChip::sourceFromDescription (details.description) >= 0;
}

void ui::ModKnob::itemDropped (const SourceDetails& details)
{
    dropHover = false;
    const int source = SourceChip::sourceFromDescription (details.description);
    const int slot = processor.addModulation (source, dest);
    if (slot >= 0)
        activeSlot = slot;
    else
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel (&getLookAndFeel());
        menu.addItem (1, "ALL 24 MODULATION SLOTS ARE IN USE", false);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
    }
    repaint();
}

void ui::ModKnob::showMenu()
{
    const auto slots = processor.slotsForDest (dest);
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());
    menu.addSectionHeader (SequencesAudioProcessor::destinationNames()[(int) dest] + " MODULATION");

    if (slots.empty())
        menu.addItem (1, "DRAG A SOURCE HERE TO MODULATE", false);

    for (int k : slots)
    {
        const auto name = SequencesAudioProcessor::modSourceNames()[processor.getModulationSource (k)];
        const int amount = juce::roundToInt (processor.getModulationAmount (k) * 100.0f);
        juce::PopupMenu sub;
        sub.addItem (1000 + k, "INVERT");
        sub.addItem (3000 + k, "SET DEPTH WITH THIS KNOB'S RING", true, k == slotForDepth());
        sub.addItem (2000 + k, "REMOVE");
        menu.addSubMenu (name + "   " + (amount > 0 ? "+" : "") + juce::String (amount) + " %", sub);
    }

    if (slots.size() > 1)
    {
        menu.addSeparator();
        menu.addItem (4000, "REMOVE ALL");
    }

    juce::Component::SafePointer<ModKnob> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int result)
    {
        if (safe == nullptr || result == 0)
            return;
        auto& proc = safe->processor;
        if (result >= 1000 && result < 2000)      proc.setModulationAmount (result - 1000, -proc.getModulationAmount (result - 1000));
        else if (result >= 2000 && result < 3000) proc.removeModulation (result - 2000);
        else if (result >= 3000 && result < 4000) safe->activeSlot = result - 3000;
        else if (result == 4000)                   for (int k : proc.slotsForDest (safe->dest)) proc.removeModulation (k);
        safe->repaint();
    });
}

//==============================================================================
ui::ModList::ModList (SequencesAudioProcessor& p) : processor (p)
{
    setTitle ("Modulation list");
}

std::vector<int> ui::ModList::rows() const
{
    std::vector<int> r;
    for (int k = 0; k < seqs::numSlots; ++k)
        if (processor.getModulationDest (k) != seqs::Dest::off)
            r.push_back (k);
    return r;
}

int ui::ModList::idealHeight() const
{
    return juce::jmax (1, (int) rows().size()) * rowHeight;
}

juce::Rectangle<float> ui::ModList::sourceArea (int row) const  { return { 4.0f, (float) (row * rowHeight), 92.0f, (float) rowHeight }; }
juce::Rectangle<float> ui::ModList::destArea (int row) const    { return { 112.0f, (float) (row * rowHeight), 104.0f, (float) rowHeight }; }
juce::Rectangle<float> ui::ModList::amountArea (int row) const
{
    return { 222.0f, (float) (row * rowHeight) + 4.0f, (float) getWidth() - 222.0f - 26.0f, (float) rowHeight - 8.0f };
}
juce::Rectangle<float> ui::ModList::removeArea (int row) const  { return { (float) getWidth() - 22.0f, (float) (row * rowHeight), 22.0f, (float) rowHeight }; }

void ui::ModList::paint (juce::Graphics& g)
{
    const auto list = rows();
    if (list.empty())
    {
        g.setColour (Palette::dim());
        g.setFont (font (9.5f, true, 0.12f));
        g.drawText ("DRAG A SOURCE ONTO A KNOB", getLocalBounds().withHeight (rowHeight), juce::Justification::centredLeft, false);
        return;
    }

    for (int r = 0; r < (int) list.size(); ++r)
    {
        const int k = list[(size_t) r];
        const int source = processor.getModulationSource (k);
        const auto colour = Palette::modSource (source);

        if (r % 2 == 1)
        {
            g.setColour (Palette::stepOff().withAlpha (0.5f));
            g.fillRect (getLocalBounds().toFloat().withY ((float) (r * rowHeight)).withHeight ((float) rowHeight));
        }

        auto src = sourceArea (r);
        g.setColour (colour);
        g.fillRoundedRectangle (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ src.getX() + 6.0f, src.getCentreY() }), 2.0f);
        g.setColour (Palette::ink());
        g.setFont (font (10.0f, true, 0.1f));
        g.drawText (SequencesAudioProcessor::modSourceNames()[source], src.withTrimmedLeft (14.0f), juce::Justification::centredLeft, false);

        g.setColour (Palette::dim());
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92")), juce::Rectangle<float> (96.0f, src.getY(), 14.0f, src.getHeight()),
                    juce::Justification::centred, false);
        g.setColour (Palette::ink());
        g.drawText (SequencesAudioProcessor::destinationNames()[(int) processor.getModulationDest (k)], destArea (r),
                    juce::Justification::centredLeft, false);

        // Profondeur : barre centrée sur zéro
        const auto bar = amountArea (r);
        const float amount = processor.getModulationAmount (k);
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (bar, 2.0f);
        const float mid = bar.getCentreX(), end = mid + amount * bar.getWidth() * 0.5f;
        g.setColour (colour.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (mid, end), bar.getY() + 1.0f, juce::jmax (mid, end), bar.getBottom() - 1.0f));
        g.setColour (colour);
        g.fillRect (juce::Rectangle<float> (2.0f, bar.getHeight()).withCentre ({ end, bar.getCentreY() }));
        g.setColour (Palette::panelLine());
        g.drawRoundedRectangle (bar, 2.0f, 1.0f);
        g.fillRect (mid - 0.5f, bar.getY(), 1.0f, bar.getHeight());
        const int pct = juce::roundToInt (amount * 100.0f);
        g.setColour (Palette::ink());
        g.setFont (font (10.0f, true, 0.02f));
        g.drawText ((pct > 0 ? "+" : "") + juce::String (pct) + " %", bar.reduced (6.0f, 0.0f), juce::Justification::centredRight, false);

        g.setColour (Palette::dim());
        g.setFont (font (13.0f, true));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xc3\x97")), removeArea (r), juce::Justification::centred, false);
    }
}

void ui::ModList::mouseDown (const juce::MouseEvent& e)
{
    const auto list = rows();
    const int r = (int) (e.position.y / (float) rowHeight);
    if (! juce::isPositiveAndBelow (r, (int) list.size()))
        return;
    const int k = list[(size_t) r];

    if (removeArea (r).contains (e.position))
    {
        processor.removeModulation (k);
        repaint();
        return;
    }

    auto choose = [this, k] (const juce::StringArray& names, int first, int current, const char* param)
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel (&getLookAndFeel());
        for (int i = first; i < names.size(); ++i)
            menu.addItem (i + 1, names[i], true, i == current);
        juce::Component::SafePointer<ModList> safe (this);
        const juce::String id = SequencesAudioProcessor::slotId (k, param);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, id] (int result)
        {
            if (safe == nullptr || result == 0)
                return;
            if (auto* p = safe->processor.apvts.getParameter (id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 ((float) (result - 1)));
                p->endChangeGesture();
            }
            safe->repaint();
        });
    };

    if (sourceArea (r).contains (e.position))
        choose (SequencesAudioProcessor::modSourceNames(), 0, processor.getModulationSource (k), "Src");
    else if (destArea (r).contains (e.position))
        choose (SequencesAudioProcessor::destinationNames(), 1, (int) processor.getModulationDest (k), "Dest");
    else if (amountArea (r).expanded (0.0f, 3.0f).contains (e.position))
    {
        dragSlot = k;
        dragParam = processor.apvts.getParameter (SequencesAudioProcessor::slotId (k, "Amt"));
        if (dragParam != nullptr)
            dragParam->beginChangeGesture();
        mouseDrag (e);
    }
}

void ui::ModList::mouseDrag (const juce::MouseEvent& e)
{
    if (dragParam == nullptr)
        return;
    const auto list = rows();
    const auto it = std::find (list.begin(), list.end(), dragSlot);
    if (it == list.end())
        return;
    const auto bar = amountArea ((int) (it - list.begin()));
    const float amount = juce::jlimit (-1.0f, 1.0f, (e.position.x - bar.getCentreX()) / (bar.getWidth() * 0.5f));
    dragParam->setValueNotifyingHost (dragParam->convertTo0to1 (amount * 100.0f));
    repaint();
}

void ui::ModList::mouseUp (const juce::MouseEvent&)
{
    if (dragParam != nullptr)
        dragParam->endChangeGesture();
    dragParam = nullptr;
    dragSlot = -1;
}

void ui::ModList::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto list = rows();
    const int r = (int) (e.position.y / (float) rowHeight);
    if (juce::isPositiveAndBelow (r, (int) list.size()) && amountArea (r).contains (e.position))
    {
        processor.setModulationAmount (list[(size_t) r], 0.0f);
        repaint();
    }
}

//==============================================================================
namespace
{
    /** Dossiers des tables de Serum 2 et Serum (installation système, puis anciens emplacements). */
    juce::Array<juce::File> serumTableFolders()
    {
        juce::Array<juce::File> found;
        const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
        const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        for (const auto& f : { juce::File ("/Library/Audio/Presets/Xfer Records/Serum 2 Presets/Tables"),
                               juce::File ("/Library/Audio/Presets/Xfer Records/Serum Presets/Tables"),
                               home.getChildFile ("Library/Audio/Presets/Xfer Records/Serum 2 Presets/Tables"),
                               home.getChildFile ("Library/Audio/Presets/Xfer Records/Serum Presets/Tables"),
                               docs.getChildFile ("Xfer/Serum 2 Presets/Tables"),
                               docs.getChildFile ("Xfer/Serum Presets/Tables") })
            if (f.isDirectory())
                found.add (f);
        return found;
    }

    juce::File lastTableFolder;

    float paramValue (SequencesAudioProcessor& p, const juce::String& id)
    {
        auto* param = p.apvts.getParameter (id);
        return param != nullptr ? param->convertFrom0to1 (param->getValue()) : 0.0f;
    }

    void drawPlotBackground (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (area, 3.0f);
        g.setColour (Palette::stepOff());
        g.fillRect (area.getX(), area.getCentreY(), area.getWidth(), 1.0f);
    }
}

ui::WavetableView::WavetableView (SequencesAudioProcessor& p) : processor (p)
{
    prev.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x80")));
    next.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6")));
    prev.setTitle ("Previous wavetable");
    next.setTitle ("Next wavetable");
    name.setTitle ("Wavetable");
    name.setComponentID ("tableName");

    for (auto* b : { &prev, &name, &next })
    {
        b->setWantsKeyboardFocus (false);
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible (*b);
    }

    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (+1); };
    name.onClick = [this] { showMenu(); };
}

void ui::WavetableView::resized()
{
    auto top = getLocalBounds().removeFromTop (22);
    prev.setBounds (top.removeFromLeft (24));
    next.setBounds (top.removeFromRight (24));
    name.setBounds (top.reduced (3, 0));
}

void ui::WavetableView::paint (juce::Graphics& g)
{
    const auto info = processor.getTableInfo();
    if (name.getButtonText() != info.name)
        name.setButtonText (info.name);

    auto area = getLocalBounds().toFloat().withTrimmedTop (26.0f);
    g.setColour (juce::Colours::white);
    g.fillRoundedRectangle (area, 4.0f);

    const auto* table = processor.getTable();
    if (table != nullptr && table->numFrames > 0)
    {
        // Pile de frames en perspective, la frame jouée par-dessus
        const auto plot = area.reduced (10.0f, 12.0f).withTrimmedBottom (12.0f);
        const float depthX = plot.getWidth() * 0.22f, depthY = plot.getHeight() * 0.32f;
        const auto front = plot.withTrimmedRight (depthX).withTrimmedTop (depthY);
        const int level = 4;
        const int shown = juce::jmin (table->numFrames, 28);

        auto drawFrame = [&] (double framePos, juce::Point<float> offset, juce::Colour colour, float thickness)
        {
            juce::Path path;
            const int points = 96;
            for (int i = 0; i <= points; ++i)
            {
                const double ph = (double) i / points * 0.9999;
                const float v = table->read (framePos, (double) level, ph);
                const juce::Point<float> pt (front.getX() + front.getWidth() * (float) i / points + offset.x,
                                             front.getCentreY() - juce::jlimit (-1.2f, 1.2f, v) * front.getHeight() * 0.42f + offset.y);
                if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
            }
            g.setColour (colour);
            g.strokePath (path, juce::PathStrokeType (thickness));
        };

        for (int k = shown - 1; k >= 0; --k)
        {
            const float t = shown > 1 ? (float) k / (float) (shown - 1) : 0.0f;
            drawFrame ((double) t * (table->numFrames - 1), { depthX * t, -depthY * t }, Palette::faint().withAlpha (0.55f), 1.0f);
        }

        const auto* posParam = processor.apvts.getParameter ("wtPos");
        const float pos = juce::jlimit (0.0f, 1.0f, posParam->getValue()
                                                   + (processor.getPlayheadBeats() >= 0.0 ? processor.getLiveOffset (seqs::Dest::wtPos) : 0.0f));
        drawFrame ((double) pos * (table->numFrames - 1), { depthX * pos, -depthY * pos }, Palette::accent(), 2.0f);

        g.setColour (Palette::dim());
        g.setFont (font (9.0f, true, 0.12f));
        g.drawText ("FRAME " + juce::String (juce::roundToInt (pos * (float) (table->numFrames - 1)) + 1) + " / " + juce::String (table->numFrames),
                    area.reduced (8.0f, 4.0f), juce::Justification::bottomLeft, false);
        g.drawText (info.builtin >= 0 ? "BUILT-IN" : "IMPORTED", area.reduced (8.0f, 4.0f), juce::Justification::bottomRight, false);
    }

    if (error.isNotEmpty() && juce::Time::getMillisecondCounter() - errorTime < 5000)
    {
        g.setColour (juce::Colour (0xffd03a3a));
        g.setFont (font (10.0f, true, 0.1f));
        g.drawFittedText (error, area.reduced (10.0f).toNearestInt(), juce::Justification::centred, 2);
    }

    g.setColour (fileHover ? Palette::accent() : Palette::panelLine());
    g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, fileHover ? 2.0f : 1.0f);
    if (fileHover)
    {
        g.setColour (Palette::accent());
        g.setFont (font (11.0f, true, 0.2f));
        g.drawText ("DROP TO LOAD", area, juce::Justification::centredTop, false);
    }
}

bool ui::WavetableView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".wav"))
            return true;
    return false;
}

void ui::WavetableView::filesDropped (const juce::StringArray& files, int, int)
{
    fileHover = false;
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".wav"))
        {
            load (juce::File (f));
            break;
        }
    repaint();
}

void ui::WavetableView::load (const juce::File& file)
{
    error = processor.loadTableFile (file);
    errorTime = juce::Time::getMillisecondCounter();
    if (error.isEmpty())
        lastTableFolder = file.getParentDirectory();
    repaint();
}

void ui::WavetableView::step (int delta)
{
    const auto info = processor.getTableInfo();
    if (info.builtin >= 0)
    {
        const int n = seqs::BuiltinTables::count();
        processor.setBuiltinTable (((info.builtin + delta) % n + n) % n);
        error = {};
    }
    else if (info.file.existsAsFile())
    {
        // Table suivante / précédente du même dossier
        auto files = info.file.getParentDirectory().findChildFiles (juce::File::findFiles, false, "*.wav");
        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
        {
            return a.getFileName().compareNatural (b.getFileName()) < 0;
        });
        const int index = files.indexOf (info.file);
        if (! files.isEmpty())
            load (files[((index < 0 ? 0 : index + delta) % files.size() + files.size()) % files.size()]);
    }
    repaint();
}

void ui::WavetableView::openChooser()
{
    const auto serum = serumTableFolders();
    openChooserAt (lastTableFolder.isDirectory() ? lastTableFolder
                   : ! serum.isEmpty() ? serum[0]
                   : juce::File::getSpecialLocation (juce::File::userHomeDirectory));
}

void ui::WavetableView::openChooserAt (const juce::File& folder)
{
    chooser = std::make_unique<juce::FileChooser> ("Load a wavetable", folder, "*.wav");
    juce::Component::SafePointer<WavetableView> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              if (safe != nullptr && fc.getResult() != juce::File())
                                  safe->load (fc.getResult());
                          });
}

void ui::WavetableView::showMenu()
{
    const auto info = processor.getTableInfo();
    const auto serum = serumTableFolders();

    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());
    menu.addSectionHeader ("BUILT-IN");
    for (int i = 0; i < seqs::BuiltinTables::count(); ++i)
        menu.addItem (i + 1, seqs::BuiltinTables::names()[(size_t) i], true, i == info.builtin);
    menu.addSeparator();
    menu.addSectionHeader ("IMPORT");
    menu.addItem (500, juce::String (juce::CharPointer_UTF8 ("IMPORT WAV\xe2\x80\xa6")));
    for (int i = 0; i < serum.size(); ++i)
        menu.addItem (600 + i, (serum[i].getFullPathName().contains ("Serum 2") ? "SERUM 2 TABLES" : "SERUM TABLES")
                                   + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6")));
    if (info.builtin < 0 && info.file.existsAsFile())
        menu.addItem (700, "SHOW FILE");

    juce::Component::SafePointer<WavetableView> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&name).withMinimumWidth (name.getWidth()),
                        [safe, serum, info] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;
                            if (result <= seqs::BuiltinTables::count())
                            {
                                safe->processor.setBuiltinTable (result - 1);
                                safe->error = {};
                            }
                            else if (result == 500)
                                safe->openChooser();
                            else if (result >= 600 && result < 700)
                                safe->openChooserAt (serum[result - 600]);
                            else if (result == 700)
                                info.file.revealToUser();
                            safe->repaint();
                        });
}

//==============================================================================
void ui::PdView::paint (juce::Graphics& g)
{
    processor.readSettings (settings);
    const bool live = processor.getPlayheadBeats() >= 0.0;
    auto norm = [&] (seqs::Dest d) { return juce::jlimit (0.0f, 1.0f, settings.base[(size_t) d] + (live ? processor.getLiveOffset (d) : 0.0f)); };
    const double amount = norm (seqs::Dest::pdAmount), shape = norm (seqs::Dest::pdShape);

    const auto area = getLocalBounds().toFloat().reduced (0.5f);
    drawPlotBackground (g, area);
    juce::Path path;
    const int n = juce::roundToInt (area.getWidth());
    for (int i = 0; i <= n; ++i)
    {
        const double ph = 2.0 * (double) i / (double) n;      // deux cycles
        const float v = seqs::PhaseDistortion::read (settings.pdWave, ph - std::floor (ph), amount, shape);
        const juce::Point<float> pt (area.getX() + (float) i, area.getCentreY() - juce::jlimit (-1.2f, 1.2f, v) * area.getHeight() * 0.42f);
        if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
    }
    g.setColour (settings.pdOn ? Palette::accent() : Palette::faint());
    g.strokePath (path, juce::PathStrokeType (1.6f));
    g.setColour (Palette::panelLine());
    g.drawRoundedRectangle (area, 3.0f, 1.0f);
}

void ui::EnvView::paint (juce::Graphics& g)
{
    const float a = paramValue (processor, id + "Attack"), d = paramValue (processor, id + "Decay");
    const float s = 0.01f * paramValue (processor, id + "Sustain"), r = paramValue (processor, id + "Release");
    const auto area = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colours::white);
    g.fillRoundedRectangle (area, 3.0f);

    // Largeurs à la racine du temps : les segments courts restent visibles
    const float wa = std::sqrt (a), wd = std::sqrt (d), wr = std::sqrt (r), hold = std::sqrt (300.0f);
    const auto plot = area.reduced (6.0f, 6.0f);
    const float scale = plot.getWidth() / (wa + wd + hold + wr);
    const float x0 = plot.getX(), x1 = x0 + wa * scale, x2 = x1 + wd * scale, x3 = x2 + hold * scale, x4 = x3 + wr * scale;
    auto y = [&plot] (float v) { return plot.getBottom() - v * plot.getHeight(); };

    juce::Path path;
    path.startNewSubPath (x0, y (0.0f));
    path.quadraticTo (x0 + (x1 - x0) * 0.3f, y (0.7f), x1, y (1.0f));
    path.quadraticTo (x1 + (x2 - x1) * 0.15f, y (s), x2, y (s));
    path.lineTo (x3, y (s));
    path.quadraticTo (x3 + (x4 - x3) * 0.15f, y (0.0f), x4, y (0.0f));

    juce::Path fill (path);
    fill.lineTo (x4, plot.getBottom());
    fill.lineTo (x0, plot.getBottom());
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (1.6f));
    g.setColour (Palette::panelLine());
    g.drawRoundedRectangle (area, 3.0f, 1.0f);
}

void ui::MainLfoView::paint (juce::Graphics& g)
{
    processor.readSettings (settings);
    const auto area = getLocalBounds().toFloat().reduced (0.5f);
    drawPlotBackground (g, area);
    juce::Path path;
    const int n = juce::roundToInt (area.getWidth());
    for (int i = 0; i <= n; ++i)
    {
        const double cycles = 2.0 * (double) i / (double) n * 0.9999;
        const float v = seqs::mainLfoValue (settings.mainLfo.shape, cycles, 0x1234ull);
        const juce::Point<float> pt (area.getX() + (float) i, area.getCentreY() - v * area.getHeight() * 0.4f);
        if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
    }
    g.setColour (Palette::modSource (seqs::srcMainLfo));
    g.strokePath (path, juce::PathStrokeType (1.6f));
    g.setColour (Palette::panelLine());
    g.drawRoundedRectangle (area, 3.0f, 1.0f);
}

//==============================================================================
ui::FmCell::FmCell (SequencesAudioProcessor& p, int r, int c) : processor (p), row (r), col (c)
{
    param = processor.apvts.getParameter (SequencesAudioProcessor::fmId (r, c));
    setComponentID (SequencesAudioProcessor::fmId (r, c));
    setTitle ("FM " + juce::String (r) + " to " + juce::String (c));
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

float ui::FmCell::getValue() const     { return param != nullptr ? param->getValue() : 0.0f; }

void ui::FmCell::setValue (float v)
{
    if (param != nullptr)
        param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
    repaint();
}

void ui::FmCell::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (1.0f);
    const float v = getValue();
    const bool feedback = row == col;
    g.setColour (juce::Colours::white);
    g.fillRoundedRectangle (area, 3.0f);
    if (v > 0.0f)
    {
        g.setColour (Palette::accent().withAlpha (0.2f + 0.6f * v));
        g.fillRoundedRectangle (area.withWidth (juce::jmax (3.0f, area.getWidth() * v)), 3.0f);
    }
    g.setColour (isMouseOverOrDragging() ? Palette::ink() : Palette::panelLine());
    g.drawRoundedRectangle (area, 3.0f, 1.0f);
    g.setColour (v > 0.0f ? Palette::ink() : Palette::faint());
    g.setFont (font (11.0f, true, 0.04f));
    g.drawText (v > 0.0f ? juce::String (juce::roundToInt (v * 100.0f)) + " %" : (feedback ? "FB" : juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"))),
                area, juce::Justification::centred, false);
}

void ui::FmCell::mouseDown (const juce::MouseEvent&)
{
    startValue = getValue();
    dragged = false;
    if (param != nullptr)
        param->beginChangeGesture();
}

void ui::FmCell::mouseDrag (const juce::MouseEvent& e)
{
    const float pixels = (float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY());
    dragged = dragged || std::abs (pixels) > 2.0f;
    setValue (startValue + pixels / (e.mods.isShiftDown() ? 600.0f : 150.0f));
}

void ui::FmCell::mouseUp (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    if (! dragged)
        setValue (getValue() > 0.0f ? 0.0f : 1.0f);   // clic : tout ou rien
    if (param != nullptr)
        param->endChangeGesture();
}

void ui::FmCell::mouseDoubleClick (const juce::MouseEvent&)
{
    if (param != nullptr)
    {
        param->beginChangeGesture();
        setValue (0.0f);
        param->endChangeGesture();
    }
}

//==============================================================================
void ui::Scope::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Palette::ink());
    g.fillRoundedRectangle (area, 3.0f);

    processor.copyScope (data);
    const auto plot = area.reduced (4.0f, 3.0f).withTrimmedRight (10.0f);

    // Déclenchement sur un passage par zéro montant, pour une image stable
    const int window = 700;
    int start = SequencesAudioProcessor::scopeSize - window - 1;
    for (int i = SequencesAudioProcessor::scopeSize - window - 1; i > 1; --i)
        if (data[(size_t) i - 1] <= 0.0f && data[(size_t) i] > 0.0f)
        {
            start = i;
            break;
        }

    float peak = 0.05f;
    for (int i = start; i < start + window; ++i)
        peak = juce::jmax (peak, std::abs (data[(size_t) i]));

    juce::Path path;
    for (int i = 0; i < window; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) (window - 1);
        const float y = plot.getCentreY() - data[(size_t) (start + i)] / peak * plot.getHeight() * 0.48f;
        if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
    }
    g.setColour (Palette::accent());
    g.strokePath (path, juce::PathStrokeType (1.2f));

    // Crête de sortie (rouge au-delà de 0 dB)
    const float level = processor.getOutputPeak();
    const float db = level > 1.0e-5f ? 20.0f * std::log10 (level) : -100.0f;
    const float h = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 54.0f);
    auto meter = area.reduced (3.0f).removeFromRight (5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.fillRect (meter);
    g.setColour (db > 0.0f ? juce::Colour (0xffff3b30) : juce::Colour (0xff4cd964));
    g.fillRect (meter.removeFromBottom (meter.getHeight() * h));
}

//==============================================================================
void ui::Page::paint (juce::Graphics& g)
{
    for (const auto& p : panels)
    {
        const auto r = p.bounds.toFloat();
        g.setColour (Palette::panel());
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Palette::panelLine());
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (Palette::ink());
        g.setFont (font (11.0f, true, 0.28f));
        g.drawText (p.title, p.bounds.withTrimmedLeft (12).withTrimmedTop (8).withHeight (16), juce::Justification::centredLeft, false);
        if (p.note.isNotEmpty())
        {
            g.setColour (Palette::dim());
            g.setFont (font (9.0f, true, 0.12f));
            g.drawText (p.note, p.bounds.withTrimmedLeft (12).withTrimmedTop (24).withHeight (12), juce::Justification::centredLeft, false);
        }
    }

    g.setFont (font (9.5f, true, 0.16f));
    g.setColour (Palette::dim());
    for (const auto& [text, r] : captions)
        g.drawText (text, r, juce::Justification::centred, false);
    for (const auto& [text, r] : leftCaptions)
        g.drawText (text, r, juce::Justification::centredLeft, false);
    g.setFont (font (10.5f, false, 0.02f));
    for (const auto& [text, r] : notes)
        g.drawFittedText (text, r, juce::Justification::topLeft, 12);
}

//==============================================================================
SequencesFacade::SequencesFacade (SequencesAudioProcessor& p)
    : audioProcessor (p),
      presetBar (p.presets, [] (float h, bool bold, float tracking) { return ui::font (h, bold, tracking); })
{
    setLookAndFeel (&lookAndFeel);
    setSize (width, height);

    // --- Onglets ---
    const char* tabNames[] = { "MAIN", "SEQ", "MATRIX", "ARP", "FX" };
    for (int t = 0; t < numTabs; ++t)
    {
        auto& b = tabButtons[(size_t) t];
        b.setButtonText (tabNames[t]);
        b.setClickingTogglesState (false);
        b.setWantsKeyboardFocus (false);
        b.setTitle (juce::String (tabNames[t]) + " tab");
        b.setBounds (16 + t * 92, 54, 86, 26);
        b.onClick = [this, t] { showTab (t); };
        addAndMakeVisible (b);

        pages[(size_t) t].setBounds (16, 120, width - 32, height - 120 - 12);
        addChildComponent (pages[(size_t) t]);
    }

    scope = std::make_unique<ui::Scope> (audioProcessor);
    scope->setBounds (width - 16 - 220, 52, 220, 30);
    addAndMakeVisible (*scope);
    headerInfo = { scope->getX() - 150, 52, 140, 30 };

    // --- Sources de modulation (toujours visibles) ---
    {
        const juce::StringArray names { "SEQ", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6", "CH 1", "CH 2",
                                        "ENV 2", "ENV 3", "VELO", "M.LFO", "WHEEL" };
        const int x0 = 16 + 128, w = (width - 16 - x0) / seqs::numModSources;
        for (int s = 0; s < seqs::numModSources; ++s)
            addChip (s, names[s], { x0 + s * w, 88, w - 6, 24 });
    }

    buildMain();
    buildSeq();
    buildMatrix();
    buildArp();
    buildFx();

    addAndMakeVisible (presetBar);
    presetBar.setBounds (width - 96 - phragma::PresetBar::idealWidth, 14, phragma::PresetBar::idealWidth, 28);
    presetBar.attachSavePanelTo (*this);

    showTab (tabMain);
    sendLookAndFeelChange();
    startTimerHz (30);
}

SequencesFacade::~SequencesFacade()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SequencesFacade::showTab (int tab)
{
    currentTab = juce::jlimit (0, numTabs - 1, tab);
    for (int t = 0; t < numTabs; ++t)
    {
        pages[(size_t) t].setVisible (t == currentTab);
        tabButtons[(size_t) t].setToggleState (t == currentTab, juce::dontSendNotification);
    }
}

ui::ModKnob* SequencesFacade::getModKnob (seqs::Dest d)
{
    for (auto* k : modKnobs)
        if (k->getDest() == d)
            return k;
    return nullptr;
}

ui::SourceChip* SequencesFacade::getChip (int source)
{
    for (auto& c : chips)
        if (c->getSource() == source)
            return c.get();
    return nullptr;
}

ui::FmCell* SequencesFacade::getFmCell (int row, int col)
{
    for (auto* c : fmCells)
        if (c->getComponentID() == SequencesAudioProcessor::fmId (row, col))
            return c;
    return nullptr;
}

juce::Component* SequencesFacade::findById (const juce::String& id)
{
    std::function<juce::Component* (juce::Component&)> search = [&] (juce::Component& c) -> juce::Component*
    {
        for (auto* child : c.getChildren())
        {
            if (child->getComponentID() == id)
                return child;
            if (auto* found = search (*child))
                return found;
        }
        return nullptr;
    };
    return search (*this);
}

//==============================================================================
juce::Rectangle<int> SequencesFacade::panel (ui::Page& page, const juce::String& title, juce::Rectangle<int> bounds, const juce::String& note)
{
    page.panels.push_back ({ title, bounds, note });
    return bounds;
}

void SequencesFacade::knobRow (ui::Page& page, juce::Rectangle<int> area, std::initializer_list<K> list, bool small)
{
    const int n = (int) list.size();
    int i = 0;
    for (const auto& k : list)
    {
        const int x0 = area.getX() + area.getWidth() * i / n, x1 = area.getX() + area.getWidth() * (i + 1) / n;
        addKnob (page, k.id, k.label, { x0, area.getY(), x1 - x0, area.getHeight() }, k.bipolar, small);
        ++i;
    }
}

void SequencesFacade::buildMain()
{
    auto& page = pages[tabMain];
    const int g = 12;

    // --- Rangée 1 : WAVETABLE, PHASE DISTORTION, ALGORITHMIC ---
    {
        const auto r = panel (page, "WAVETABLE", { 0, 0, 560, 262 });
        int x = r.getRight() - 12;
        auto right = [&x, &r] (int w) { x -= w; const juce::Rectangle<int> b (x, r.getY() + 6, w, 22); x -= 6; return b; };
        addButton (page, loadButton, "LOAD", "Load a wavetable file", right (52));
        addToggle (page, "wtRandPhase", "RND", right (44));
        addInlineChoice (page, "wtWarpMode", right (96));
        page.leftCaptions.push_back ({ "WARP", right (36) });
        addToggle (page, "wtOn", "ON", right (40));

        tableView = std::make_unique<ui::WavetableView> (audioProcessor);
        tableView->setBounds (r.getX() + 12, r.getY() + 34, 212, 218);
        page.addAndMakeVisible (*tableView);
        loadButton.onClick = [this] { tableView->openChooser(); };

        const juce::Rectangle<int> knobArea (r.getX() + 236, r.getY() + 34, r.getWidth() - 248, 216);
        knobRow (page, knobArea.withHeight (108), { { "wtPos", "POS" }, { "wtWarp", "WARP" }, { "wtOct", "OCT", true },
                                                 { "wtSemi", "SEMI", true }, { "wtFine", "FINE", true }, { "wtLevel", "LEVEL" } });
        knobRow (page, knobArea.withTrimmedTop (110), { { "unison", "UNISON" }, { "wtDetune", "DETUNE" }, { "wtSpread", "SPREAD" },
                                                     { "wtPan", "PAN", true }, { "wtPhase", "PHASE" } });
    }
    {
        const auto r = panel (page, "PHASE DISTORTION", { 560 + g, 0, 330, 262 });
        addInlineChoice (page, "pdWave", { r.getRight() - 12 - 108, r.getY() + 6, 108, 22 });
        addToggle (page, "pdOn", "ON", { r.getRight() - 12 - 108 - 6 - 40, r.getY() + 6, 40, 22 });
        auto view = std::make_unique<ui::PdView> (audioProcessor);
        view->setBounds (r.getX() + 12, r.getY() + 34, r.getWidth() - 24, 44);
        page.addAndMakeVisible (*view);
        owned.push_back (std::move (view));
        knobRow (page, { r.getX() + 8, r.getY() + 82, r.getWidth() - 16, 88 },
                 { { "pdAmount", "PD AMT" }, { "pdShape", "SHAPE" }, { "pdCrush", "CRUSH" }, { "pdFold", "FOLD" } }, true);
        knobRow (page, { r.getX() + 8, r.getY() + 172, r.getWidth() - 16, 88 },
                 { { "pdLevel", "LEVEL" }, { "pdOct", "OCT", true }, { "pdSemi", "SEMI", true }, { "pdFine", "FINE", true } }, true);
    }
    {
        const auto r = panel (page, "ALGORITHMIC (PLAITS)", { 560 + 330 + 2 * g, 0, 374, 262 });
        addToggle (page, "algoLpg", "LPG", { r.getRight() - 12 - 44, r.getY() + 6, 44, 22 });
        addToggle (page, "algoOn", "ON", { r.getRight() - 12 - 44 - 6 - 40, r.getY() + 6, 40, 22 });
        addLabelledChoice (page, "algoModel", "MODEL", { r.getX() + 12, r.getY() + 34, r.getWidth() - 24, 22 }, 50);
        knobRow (page, { r.getX() + 6, r.getY() + 60, r.getWidth() - 12, 100 },
                 { { "algoHarmonics", "HARM" }, { "algoTimbre", "TIMBRE" }, { "algoMorph", "MORPH" },
                   { "algoDecay", "DECAY" }, { "algoColour", "COLOUR" }, { "algoAux", "AUX" } });
        knobRow (page, { r.getX() + 6, r.getY() + 160, r.getWidth() - 12, 100 },
                 { { "algoOct", "OCT", true }, { "algoSemi", "SEMI", true }, { "algoFine", "FINE", true },
                   { "algoLevel", "LEVEL" }, { "algoPan", "PAN", true } });
    }

    // --- Rangée 2 : SUB + NOISE, FM MATRIX, BITE!, FILTER ---
    const int y2 = 262 + g;
    const int subW = 210, fmW = 384, biteW = 206;
    {
        const auto r = panel (page, "SUB + NOISE", { 0, y2, subW, 214 });
        page.leftCaptions.push_back ({ "SUB", { r.getX() + 12, r.getY() + 44, 80, 14 } });
        addInlineChoice (page, "subWave", { r.getX() + 12, r.getY() + 62, 84, 22 });
        knobRow (page, { r.getX() + 100, r.getY() + 30, r.getWidth() - 106, 88 }, { { "subOct", "OCT" }, { "subLevel", "LEVEL" } }, true);
        page.leftCaptions.push_back ({ "NOISE", { r.getX() + 12, r.getY() + 136, 80, 14 } });
        addInlineChoice (page, "noiseType", { r.getX() + 12, r.getY() + 154, 84, 22 });
        knobRow (page, { r.getX() + 100 + (r.getWidth() - 106) / 2, r.getY() + 122, (r.getWidth() - 106) / 2, 88 },
                 { { "noiseLevel", "LEVEL" } }, true);
    }
    {
        const auto r = panel (page, "FM MATRIX", { subW + g, y2, fmW, 214 }, "ROW MODULATES COLUMN, DIAGONAL = FEEDBACK");
        const char* rows[] = { "WT", "PD", "ALGO", "NOISE" };
        const char* cols[] = { "TO WT", "TO PD", "TO ALGO" };
        const int cw = 64, ch = 28, x0 = r.getX() + 56, y0 = r.getY() + 58;
        for (int c = 0; c < seqs::numFmCols; ++c)
            page.captions.push_back ({ cols[c], { x0 + c * (cw + 4), r.getY() + 42, cw, 14 } });
        for (int row = 0; row < seqs::numFmRows; ++row)
        {
            page.leftCaptions.push_back ({ rows[row], { r.getX() + 12, y0 + row * (ch + 4), 44, ch } });
            for (int c = 0; c < seqs::numFmCols; ++c)
            {
                auto cell = std::make_unique<ui::FmCell> (audioProcessor, row, c);
                cell->setBounds (x0 + c * (cw + 4), y0 + row * (ch + 4), cw, ch);
                page.addAndMakeVisible (*cell);
                fmCells.push_back (cell.get());
                owned.push_back (std::move (cell));
            }
        }
        addKnob (page, "fmDepth", "DEPTH", { r.getRight() - 12 - 72, r.getY() + 40, 72, 104 });
        addCaptionedChoice (page, "fmMode", "MODE", { r.getRight() - 12 - 76, r.getY() + 150, 76, 44 });
        page.leftCaptions.push_back ({ "ALGO : FM BY PITCH", { r.getX() + 12, r.getY() + 190, 200, 14 } });
    }
    {
        const auto r = panel (page, "BITE!", { subW + fmW + 2 * g, y2, biteW, 214 });
        knobRow (page, { r.getX() + 6, r.getY() + 34, r.getWidth() - 12, 112 }, { { "biteFold", "FOLD" }, { "biteCrush", "CRUSH" }, { "biteSoft", "SOFT" } }, true);
        addToggle (page, "bitePost", "POST FILTER", { r.getX() + 12, r.getY() + 166, 108, 26 });
        addToggle (page, "biteAsym", "ASYM", { r.getX() + 126, r.getY() + 166, r.getWidth() - 138, 26 });
    }
    {
        const int fx = subW + fmW + biteW + 3 * g;
        const auto r = panel (page, "FILTER", { fx, y2, 1288 - fx, 214 });
        int x = r.getRight() - 12;
        auto right = [&x, &r] (int w) { x -= w; const juce::Rectangle<int> b (x, r.getY() + 6, w, 22); x -= 6; return b; };
        addInlineChoice (page, "filterSlope", right (70));
        addInlineChoice (page, "filterType", right (70));
        addInlineChoice (page, "filterModel", right (84));
        knobRow (page, { r.getX() + 8, r.getY() + 44, r.getWidth() - 16, 140 },
                 { { "cutoff", "CUTOFF" }, { "resonance", "RESO" }, { "keyTrack", "KEY TRK" }, { "filterDrive", "DRIVE" }, { "filterEnv", "ENV AMT", true } });
    }

    // --- Rangée 3 : enveloppes, MAIN LFO, VOICE ---
    const int y3 = y2 + 214 + g, h3 = 770 - y3 - 4;
    {
        struct E { const char* title; const char* id; int source; };
        const E envs[] = { { "AMP ENV", "amp", -1 }, { "FILTER ENV  (ENV 2)", "flt", seqs::srcEnv2 }, { "MOD ENV  (ENV 3)", "mod", seqs::srcEnv3 } };
        for (int i = 0; i < 3; ++i)
        {
            const auto r = panel (page, envs[i].title, { i * (236 + g), y3, 236, h3 });
            auto view = std::make_unique<ui::EnvView> (audioProcessor, envs[i].id,
                                                       envs[i].source < 0 ? Palette::ink() : Palette::modSource (envs[i].source));
            view->setBounds (r.getX() + 12, r.getY() + 34, r.getWidth() - 24, 72);
            page.addAndMakeVisible (*view);
            owned.push_back (std::move (view));
            const juce::String id (envs[i].id);
            knobRow (page, { r.getX() + 6, r.getY() + 112, r.getWidth() - 12, juce::jmin (110, h3 - 116) },
                     { { id + "Attack", "ATK" }, { id + "Decay", "DEC" }, { id + "Sustain", "SUS" }, { id + "Release", "REL" } }, true);
        }
    }
    {
        const auto r = panel (page, "MAIN LFO", { 3 * (236 + g), y3, 300, h3 });
        addToggle (page, "mlRetrig", "RETRIG", { r.getRight() - 12 - 64, r.getY() + 6, 64, 22 });
        addToggle (page, "mlSync", "SYNC", { r.getRight() - 12 - 64 - 6 - 54, r.getY() + 6, 54, 22 });
        auto view = std::make_unique<ui::MainLfoView> (audioProcessor);
        view->setBounds (r.getX() + 12, r.getY() + 34, r.getWidth() - 24, 46);
        page.addAndMakeVisible (*view);
        owned.push_back (std::move (view));
        addLabelledChoice (page, "mlShape", "SHAPE", { r.getX() + 12, r.getY() + 88, 140, 22 }, 44);
        addLabelledChoice (page, "mlRate", "RATE", { r.getX() + 160, r.getY() + 88, r.getWidth() - 172, 22 }, 38);
        mlRateBox = &choices.back()->box;
        addInlineChoice (page, "mlDest1", { r.getX() + 12, r.getY() + 118, 132, 22 });
        addBar (page, "mlAmt1", "AMT", { r.getX() + 150, r.getY() + 118, r.getWidth() - 162, 22 }, true);
        addInlineChoice (page, "mlDest2", { r.getX() + 12, r.getY() + 146, 132, 22 });
        addBar (page, "mlAmt2", "AMT", { r.getX() + 150, r.getY() + 146, r.getWidth() - 162, 22 }, true);
        mlHzKnob = &addKnob (page, "mlHz", "FREE RATE", { r.getX() + 12, r.getY() + 172, 90, juce::jmin (84, h3 - 176) }, false, true);
        page.notes.push_back ({ "DRAG THE MAIN LFO CHIP ONTO ANY KNOB FOR MORE DESTINATIONS.", { r.getX() + 112, r.getY() + 186, r.getWidth() - 124, 60 } });
    }
    {
        const int x = 3 * (236 + g) + 300 + g;
        const auto r = panel (page, "VOICE", { x, y3, 1288 - x, h3 });
        addCaptionedChoice (page, "voiceMode", "MODE", { r.getX() + 12, r.getY() + 32, (r.getWidth() - 30) / 2, 44 });
        addCaptionedChoice (page, "oversampling", "QUALITY", { r.getX() + 18 + (r.getWidth() - 30) / 2, r.getY() + 32, (r.getWidth() - 30) / 2, 44 });
        knobRow (page, { r.getX() + 6, r.getY() + 84, r.getWidth() - 12, juce::jmin (100, h3 - 88) },
                 { { "glide", "GLIDE" }, { "bend", "BEND" }, { "velocity", "VELO" }, { "volume", "VOLUME" } }, true);
    }
}

void SequencesFacade::buildSeq()
{
    auto& page = pages[tabSeq];

    // --- Banque de LFO ---
    {
        const auto area = panel (page, "LFO BANK", { 0, 0, 1288, 236 });
        const int columns = seqs::numSources;
        const int colWidth = (area.getWidth() - 24) / columns;
        const int y0 = area.getY() + 34;

        for (int c = 0; c < columns; ++c)
        {
            const int x = area.getX() + 12 + c * colWidth;
            const int w = colWidth - 12;
            const bool isLfo = c < seqs::numLfos;
            page.leftCaptions.push_back ({ SequencesAudioProcessor::sourceNames()[c + 1], { x, y0 - 2, 70, 20 } });

            if (isLfo)
            {
                auto preview = std::make_unique<ui::LfoPreview> (audioProcessor, c);
                preview->setBounds (x + w - 80, y0, 80, 18);
                page.addAndMakeVisible (*preview);
                previews.push_back (preview.get());
                owned.push_back (std::move (preview));

                addLabelledChoice (page, SequencesAudioProcessor::lfoId (c, "Rate"), "RATE", { x, y0 + 26, w, 22 }, 40);
                addLabelledChoice (page, SequencesAudioProcessor::lfoId (c, "Shape"), "SHAPE", { x, y0 + 52, w, 22 }, 40);
                addLabelledChoice (page, SequencesAudioProcessor::lfoId (c, "Step"), "STEP", { x, y0 + 78, w / 2 + 14, 22 }, 40);
                addInlineChoice (page, SequencesAudioProcessor::lfoId (c, "Pol"), { x + w / 2 + 18, y0 + 78, w - w / 2 - 18, 22 });
                addKnob (page, SequencesAudioProcessor::lfoId (c, "Amt"), "AMT", { x, y0 + 106, w / 2, 86 }, false, true);
                addKnob (page, SequencesAudioProcessor::lfoId (c, "Phase"), "PHASE", { x + w / 2, y0 + 106, w - w / 2, 86 }, false, true);
            }
            else
            {
                const int k = c - seqs::numLfos;
                addLabelledChoice (page, SequencesAudioProcessor::chaosId (k, "Rate"), "RATE", { x, y0 + 26, w, 22 }, 40);
                addKnob (page, SequencesAudioProcessor::chaosId (k, "Amt"), "AMT", { x, y0 + 62, w / 2, 120 });
                addKnob (page, SequencesAudioProcessor::chaosId (k, "Smooth"), "SMOOTH", { x + w / 2, y0 + 62, w - w / 2, 120 });
            }
        }
    }

    // --- Séquenceur ---
    const auto area = panel (page, "SEQUENCER", { 0, 248, 1288, 770 - 248 - 4 }, "EACH STEP PICKS WHICH LFO THE SEQ CHIP OUTPUTS");
    const int top = area.getY() + 40, h = 24;
    int x = area.getX() + 12;
    addToggle (page, "seqOn", "ON", { x, top, 44, h });                                   x += 44 + 8;
    addBar (page, "seqSteps", "STEPS", { x, top, 112, h }, false);                       x += 112 + 8;
    addLabelledChoice (page, "seqDiv", "DIV", { x, top, 92, h }, 30);                    x += 92 + 8;
    addInlineChoice (page, "seqFeel", { x, top, 94, h });                                x += 94 + 8;
    addLabelledChoice (page, "seqSync", "SYNC", { x, top, 106, h }, 36);                 x += 106 + 8;
    addBar (page, "seqSwing", "SWING", { x, top, 112, h }, false);                       x += 112 + 8;
    addToggle (page, "seqRestart", "RESTART", { x, top, 76, h });                         x += 76 + 8;
    addBar (page, "seqXfade", "XFADE", { x, top, 124, h }, false);                        x += 124 + 8;
    addLabelledChoice (page, "seqManual", "WHEN OFF", { x, top, area.getRight() - 12 - x, h }, 66);

    const int gridWidth = ui::StepMatrix::labelWidth + SequencesAudioProcessor::numSteps * 26;
    matrix = std::make_unique<ui::StepMatrix> (audioProcessor);
    matrix->setBounds (area.getX() + 12, top + 36, gridWidth, ui::StepMatrix::numRows * 22);
    page.addAndMakeVisible (*matrix);

    circuit = std::make_unique<ui::CircuitView> (audioProcessor);
    circuit->setBounds (area.getX() + 12, matrix->getBottom() + 10, gridWidth, area.getBottom() - 12 - (matrix->getBottom() + 10));
    page.addAndMakeVisible (*circuit);

    const int rx = matrix->getRight() + 16, rw = area.getRight() - 12 - rx;
    randomAllButton.setComponentID ("random");
    randomButton.setComponentID ("random");
    addButton (page, randomButton, "RANDOM", "Random pattern", { rx, top + 36, (rw - 6) / 2, 30 });
    addButton (page, randomAllButton, "RANDOM ALL", "Random LFO bank and pattern", { rx + (rw - 6) / 2 + 6, top + 36, rw - (rw - 6) / 2 - 6, 30 });
    addButton (page, leftButton, juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x80")), "Shift left", { rx, top + 72, 40, 24 });
    addButton (page, rightButton, juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6")), "Shift right", { rx + 46, top + 72, 40, 24 });
    addButton (page, clearButton, "CLEAR", "Clear pattern", { rx + 92, top + 72, rw - 92, 24 });

    randomButton.onClick = [this] { audioProcessor.randomizePattern(); repaint(); };
    randomAllButton.onClick = [this] { audioProcessor.randomizeAll(); repaint(); };
    clearButton.onClick = [this] { audioProcessor.fillPattern (0); repaint(); };
    leftButton.onClick = [this] { audioProcessor.rotatePattern (-1); repaint(); };
    rightButton.onClick = [this] { audioProcessor.rotatePattern (+1); repaint(); };

    seqModListArea = { rx, top + 108, rw, area.getBottom() - 12 - (top + 108) };
    page.leftCaptions.push_back ({ "MODULATIONS", seqModListArea.withHeight (14) });
    seqModList = std::make_unique<ui::ModList> (audioProcessor);
    seqModViewport.setViewedComponent (seqModList.get(), false);
    seqModViewport.setScrollBarsShown (true, false);
    seqModViewport.setScrollBarThickness (6);
    seqModViewport.setBounds (seqModListArea.withTrimmedTop (18));
    seqModList->setBounds (0, 0, seqModViewport.getMaximumVisibleWidth(), seqModList->idealHeight());
    page.addAndMakeVisible (seqModViewport);
}

void SequencesFacade::buildMatrix()
{
    auto& page = pages[tabMatrix];
    const auto area = panel (page, "MODULATION MATRIX", { 0, 0, 1288, 766 },
                             "DRAG A SOURCE CHIP ONTO A KNOB, OR ADD A ROUTING HERE. CLICK A NAME TO CHANGE IT, DRAG THE BAR FOR DEPTH.");
    addButton (page, addModButton, "+ ADD ROUTING", "Add a modulation routing", { area.getRight() - 12 - 140, area.getY() + 8, 140, 24 });
    addModButton.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel (&lookAndFeel);
        const auto& sources = SequencesAudioProcessor::modSourceNames();
        const auto& dests = SequencesAudioProcessor::destinationNames();
        for (int s = 0; s < sources.size(); ++s)
        {
            juce::PopupMenu sub;
            for (int d = 1; d < dests.size(); ++d)
                sub.addItem (1 + s * 100 + d, dests[d]);
            menu.addSubMenu (sources[s], sub);
        }
        juce::Component::SafePointer<SequencesFacade> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addModButton), [safe] (int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            safe->audioProcessor.addModulation ((result - 1) / 100, (seqs::Dest) ((result - 1) % 100));
        });
    };

    modListArea = area.withTrimmedTop (48).reduced (12, 0).withTrimmedBottom (12);
    modList = std::make_unique<ui::ModList> (audioProcessor);
    modViewport.setViewedComponent (modList.get(), false);
    modViewport.setScrollBarsShown (true, false);
    modViewport.setScrollBarThickness (6);
    modViewport.setBounds (modListArea);
    modList->setBounds (0, 0, modViewport.getMaximumVisibleWidth(), modList->idealHeight());
    page.addAndMakeVisible (modViewport);
}

void SequencesFacade::buildArp()
{
    auto& page = pages[tabArp];
    const auto r = panel (page, "ARPEGGIATOR", { 0, 0, 760, 250 }, "PLAYS THE HELD NOTES ONE BY ONE, LOCKED TO THE TEMPO");
    addToggle (page, "arpLatch", "LATCH", { r.getRight() - 12 - 64, r.getY() + 8, 64, 24 });
    addToggle (page, "arpOn", "ON", { r.getRight() - 12 - 64 - 6 - 48, r.getY() + 8, 48, 24 });
    addCaptionedChoice (page, "arpMode", "MODE", { r.getX() + 12, r.getY() + 60, 130, 44 });
    addCaptionedChoice (page, "arpRate", "RATE", { r.getX() + 150, r.getY() + 60, 100, 44 });
    knobRow (page, { r.getX() + 270, r.getY() + 44, r.getWidth() - 282, 130 },
             { { "arpOct", "OCTAVES" }, { "arpGate", "GATE" }, { "arpSwing", "SWING" } });
    page.notes.push_back ({ "MODES : UP, DOWN, UP/DOWN, DOWN/UP, RANDOM, AS PLAYED (in the order the keys were pressed), CHORD (all notes each step). "
                            "LATCH keeps the chord after you let go ; a new chord replaces it. "
                            "When the host plays, steps line up with the song position.",
                            { r.getX() + 12, r.getY() + 186, r.getWidth() - 24, 56 } });
}

void SequencesFacade::buildFx()
{
    auto& page = pages[tabFx];
    {
        const auto r = panel (page, "CHORUS", { 0, 0, 300, 250 });
        addToggle (page, "chorusOn", "ON", { r.getRight() - 12 - 48, r.getY() + 8, 48, 24 });
        knobRow (page, { r.getX() + 8, r.getY() + 50, r.getWidth() - 16, 140 }, { { "chorusRate", "RATE" }, { "chorusDepth", "DEPTH" }, { "chorusMix", "MIX" } });
    }
    {
        const auto r = panel (page, "DELAY", { 312, 0, 480, 250 }, "SYNC OFF = FREE TIME IN MS");
        addToggle (page, "delayPingPong", "PING-PONG", { r.getRight() - 12 - 96, r.getY() + 8, 96, 24 });
        addToggle (page, "delaySync", "SYNC", { r.getRight() - 12 - 96 - 6 - 58, r.getY() + 8, 58, 24 });
        addToggle (page, "delayOn", "ON", { r.getRight() - 12 - 96 - 6 - 58 - 6 - 48, r.getY() + 8, 48, 24 });
        addCaptionedChoice (page, "delayTime", "SYNC TIME", { r.getX() + 12, r.getY() + 54, 90, 44 });
        delayTimeBox = &choices.back()->box;
        delayMsKnob = &addKnob (page, "delayMs", "FREE TIME", { r.getX() + 12, r.getY() + 106, 90, 100 }, false, true);
        knobRow (page, { r.getX() + 110, r.getY() + 50, r.getWidth() - 122, 140 }, { { "delayFeedback", "FEEDBACK" }, { "delayTone", "TONE" }, { "delayMix", "MIX" } });
    }
    {
        const auto r = panel (page, "REVERB", { 804, 0, 484, 250 });
        addToggle (page, "reverbOn", "ON", { r.getRight() - 12 - 48, r.getY() + 8, 48, 24 });
        knobRow (page, { r.getX() + 8, r.getY() + 50, r.getWidth() - 16, 140 },
                 { { "reverbSize", "SIZE" }, { "reverbDecay", "DECAY" }, { "reverbDamping", "DAMP" }, { "reverbPredelay", "PRE" }, { "reverbMix", "MIX" } });
    }
    page.notes.push_back ({ "CHAIN : SYNTH -> CHORUS -> DELAY -> REVERB -> OUT.   EVERY FX KNOB CAN BE MODULATED : DRAG A SOURCE CHIP ONTO IT "
                            "(THE FX FOLLOW THE LAST NOTE PLAYED, AND KEEP ITS MODULATION DURING THE TAILS).", { 12, 266, 1200, 40 } });
}

//==============================================================================
juce::Slider& SequencesFacade::addKnob (ui::Page& page, const juce::String& id, const juce::String& label, juce::Rectangle<int> cell, bool bipolar, bool small)
{
    auto k = std::make_unique<Knob>();
    const auto dest = SequencesAudioProcessor::destForParam (id);
    if (dest != seqs::Dest::off)
    {
        auto mk = std::make_unique<ui::ModKnob> (audioProcessor, dest);
        modKnobs.push_back (mk.get());
        k->slider = std::move (mk);
    }
    else
        k->slider = std::make_unique<juce::Slider>();

    auto& s = *k->slider;
    const int captionHeight = small ? 13 : 16;
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, cell.getWidth(), small ? 14 : 18);
    s.setMouseDragSensitivity (220);
    s.setVelocityBasedMode (false);
    s.getProperties().set ("bipolar", bipolar);
    s.setTitle (label);
    s.setComponentID (id);
    s.setBounds (cell.withTrimmedTop (captionHeight));
    page.addAndMakeVisible (s);
    k->attachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, id, s);

    if (auto* param = audioProcessor.apvts.getParameter (id))
        s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    page.captions.push_back ({ label, cell.withHeight (captionHeight - 2) });
    knobs.push_back (std::move (k));
    return *knobs.back()->slider;
}

void SequencesFacade::addBar (ui::Page& page, const juce::String& id, const juce::String& label, juce::Rectangle<int> area, bool bipolar)
{
    auto k = std::make_unique<Knob>();
    k->slider = std::make_unique<juce::Slider>();
    auto& s = *k->slider;
    s.setSliderStyle (juce::Slider::LinearBar);
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setVelocityBasedMode (false);
    s.getProperties().set ("bipolar", bipolar);
    s.getProperties().set ("label", label);
    s.setTitle (label);
    s.setComponentID (id);
    s.setBounds (area);
    page.addAndMakeVisible (s);
    k->attachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, id, s);

    if (auto* param = audioProcessor.apvts.getParameter (id))
        s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    knobs.push_back (std::move (k));
}

ui::SourceChip& SequencesFacade::addChip (int source, const juce::String& text, juce::Rectangle<int> area)
{
    chips.push_back (std::make_unique<ui::SourceChip> (source, text));
    chips.back()->setBounds (area);
    addAndMakeVisible (*chips.back());
    return *chips.back();
}

juce::ComboBox& SequencesFacade::addInlineChoice (ui::Page& page, const juce::String& id, juce::Rectangle<int> area)
{
    auto c = std::make_unique<Choice>();
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (audioProcessor.apvts.getParameter (id)))
        c->box.addItemList (param->choices, 1);
    c->box.setTitle (id);
    c->box.setComponentID (id);
    c->box.setBounds (area);
    page.addAndMakeVisible (c->box);
    c->attachment = std::make_unique<ComboAttachment> (audioProcessor.apvts, id, c->box);
    choices.push_back (std::move (c));
    return choices.back()->box;
}

void SequencesFacade::addCaptionedChoice (ui::Page& page, const juce::String& id, const juce::String& label, juce::Rectangle<int> area)
{
    page.captions.push_back ({ label, area.withHeight (14) });
    addInlineChoice (page, id, area.withTrimmedTop (18).withHeight (24));
}

void SequencesFacade::addLabelledChoice (ui::Page& page, const juce::String& id, const juce::String& label, juce::Rectangle<int> area, int labelWidth)
{
    page.leftCaptions.push_back ({ label, area.withWidth (labelWidth) });
    addInlineChoice (page, id, area.withTrimmedLeft (labelWidth));
}

juce::TextButton& SequencesFacade::addToggle (ui::Page& page, const juce::String& id, const juce::String& text, juce::Rectangle<int> area)
{
    auto t = std::make_unique<Toggle>();
    t->button.setButtonText (text);
    t->button.setClickingTogglesState (true);
    t->button.setTitle (id);
    t->button.setComponentID (id);
    t->button.setWantsKeyboardFocus (false);
    t->button.setBounds (area);
    page.addAndMakeVisible (t->button);
    t->attachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, id, t->button);
    toggles.push_back (std::move (t));
    return toggles.back()->button;
}

void SequencesFacade::addButton (juce::Component& parent, juce::TextButton& b, const juce::String& text, const juce::String& title, juce::Rectangle<int> area)
{
    b.setButtonText (text);
    b.setTitle (title);
    b.setWantsKeyboardFocus (false);
    b.setBounds (area);
    parent.addAndMakeVisible (b);
}

void SequencesFacade::timerCallback()
{
    ++tick;
    const bool playing = audioProcessor.getPlayheadBeats() >= 0.0;

    scope->repaint();
    const int voices = audioProcessor.getActiveVoices();
    if (voices != shownVoices)
    {
        shownVoices = voices;
        repaint (headerInfo);
    }

    // RATE (synchro) ou FREE RATE (Hz) : celui qui ne sert pas est grisé
    const bool sync = audioProcessor.apvts.getRawParameterValue ("mlSync")->load() > 0.5f;
    if (mlHzKnob != nullptr) mlHzKnob->setAlpha (sync ? 0.35f : 1.0f);
    if (mlRateBox != nullptr) mlRateBox->setAlpha (sync ? 1.0f : 0.35f);
    const bool delaySync = audioProcessor.apvts.getRawParameterValue ("delaySync")->load() > 0.5f;
    if (delayMsKnob != nullptr) delayMsKnob->setAlpha (delaySync ? 0.35f : 1.0f);
    if (delayTimeBox != nullptr) delayTimeBox->setAlpha (delaySync ? 1.0f : 0.35f);

    if (currentTab == tabFx && (playing || tick % 6 == 0))
        for (auto* k : modKnobs)
            if (k->isShowing())
                k->repaint();

    if (currentTab == tabMain && (playing || tick % 6 == 0))
    {
        for (auto* k : modKnobs)
            if (k->isShowing())
                k->repaint();
        tableView->repaint();
        for (auto& o : owned)
            if (o->isShowing())
                o->repaint();
    }

    if (currentTab == tabSeq)
    {
        const int step = audioProcessor.getPlayheadStep();
        if (step != shownStep || matrix->isMouseButtonDown())
        {
            shownStep = step;
            matrix->repaint();
        }
        if (playing || tick % 6 == 0)
            circuit->repaint();
        if (tick % 6 == 0)
            for (auto* p : previews)
                p->repaint();
        for (auto* k : modKnobs)
            if (k->isShowing() && (playing || tick % 6 == 0))
                k->repaint();
    }

    if (tick % 3 == 0)
    {
        const int rows = (int) modList->rows().size();
        if (rows != shownRows)
        {
            shownRows = rows;
            modList->setBounds (0, 0, modViewport.getMaximumVisibleWidth(), modList->idealHeight());
            seqModList->setBounds (0, 0, seqModViewport.getMaximumVisibleWidth(), seqModList->idealHeight());
            repaint();
        }
        if (currentTab == tabMatrix) modList->repaint();
        if (currentTab == tabSeq)    seqModList->repaint();
    }
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

    g.setFont (ui::font (10.0f, true, 0.16f));
    g.drawText ("VOICES " + juce::String (juce::jmax (0, shownVoices)) + " / " + juce::String (seqs::maxVoices)
                    + "    MOD " + juce::String (audioProcessor.usedSlotCount()) + " / " + juce::String (seqs::numSlots),
                headerInfo, juce::Justification::centredRight, false);

    g.setColour (Palette::ink());
    g.setFont (ui::font (9.5f, true, 0.16f));
    g.drawText ("DRAG TO MODULATE", juce::Rectangle<int> (16, 88, 124, 24), juce::Justification::centredLeft, false);
}

//==============================================================================
SequencesAudioProcessorEditor::SequencesAudioProcessorEditor (SequencesAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), facade (p)
{
    addAndMakeVisible (facade);

    // Taille gardée avec le projet (lue avant de régler les limites : les régler redimensionne déjà la fenêtre).
    const int savedWidth = (int) audioProcessor.apvts.state.getProperty ("editorWidth", juce::roundToInt (SequencesFacade::width * 0.9));

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
