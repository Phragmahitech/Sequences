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
    g.setFont (font (10.5f, true, 0.18f));
    g.drawText (text, area.withTrimmedLeft (18.0f).withTrimmedRight (14.0f), juce::Justification::centredLeft, false);

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
        menu.addItem (1, "ALL 16 MODULATION SLOTS ARE IN USE", false);
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

juce::Rectangle<float> ui::ModList::sourceArea (int row) const  { return { 0.0f, (float) (row * rowHeight), 84.0f, (float) rowHeight }; }
juce::Rectangle<float> ui::ModList::destArea (int row) const    { return { 98.0f, (float) (row * rowHeight), 84.0f, (float) rowHeight }; }
juce::Rectangle<float> ui::ModList::amountArea (int row) const
{
    return { 188.0f, (float) (row * rowHeight) + 3.0f, (float) getWidth() - 188.0f - 26.0f, (float) rowHeight - 6.0f };
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
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92")), juce::Rectangle<float> (84.0f, src.getY(), 14.0f, src.getHeight()),
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
}

ui::WavetableView::WavetableView (SequencesAudioProcessor& p, int o) : processor (p), osc (o)
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
    const auto info = processor.getTableInfo (osc);
    if (name.getButtonText() != info.name)
        name.setButtonText (info.name);

    auto area = getLocalBounds().toFloat().withTrimmedTop (26.0f);
    g.setColour (juce::Colours::white);
    g.fillRoundedRectangle (area, 4.0f);

    const auto* table = processor.getTable (osc);
    if (table != nullptr && table->numFrames > 0)
    {
        // Pile de frames en perspective, la frame jouée par-dessus
        const auto plot = area.reduced (10.0f, 12.0f).withTrimmedBottom (12.0f);
        const float depthX = plot.getWidth() * 0.22f, depthY = plot.getHeight() * 0.32f;
        const auto front = plot.withTrimmedRight (depthX).withTrimmedTop (depthY);
        const int level = 4;   // 64 harmoniques suffisent pour dessiner
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
            const double framePos = (double) t * (table->numFrames - 1);
            drawFrame (framePos, { depthX * t, -depthY * t }, Palette::faint().withAlpha (0.55f), 1.0f);
        }

        const auto posDest = osc == 0 ? seqs::Dest::aPos : seqs::Dest::bPos;
        const auto* posParam = processor.apvts.getParameter (SequencesAudioProcessor::paramForDest (posDest));
        const float pos = juce::jlimit (0.0f, 1.0f, posParam->getValue()
                                                   + (processor.getPlayheadBeats() >= 0.0 ? processor.getLiveOffset (posDest) : 0.0f));
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
    error = processor.loadTableFile (osc, file);
    errorTime = juce::Time::getMillisecondCounter();
    if (error.isEmpty())
        lastTableFolder = file.getParentDirectory();
    repaint();
}

void ui::WavetableView::step (int delta)
{
    const auto info = processor.getTableInfo (osc);
    if (info.builtin >= 0)
    {
        const int n = seqs::BuiltinTables::count();
        processor.setBuiltinTable (osc, ((info.builtin + delta) % n + n) % n);
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

void ui::WavetableView::openChooser (const juce::File& folder)
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
    const auto info = processor.getTableInfo (osc);
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
                                safe->processor.setBuiltinTable (safe->osc, result - 1);
                                safe->error = {};
                            }
                            else if (result == 500)
                            {
                                auto folder = lastTableFolder.isDirectory() ? lastTableFolder
                                            : ! serum.isEmpty() ? serum[0]
                                            : juce::File::getSpecialLocation (juce::File::userHomeDirectory);
                                safe->openChooser (folder);
                            }
                            else if (result >= 600 && result < 700)
                                safe->openChooser (serum[result - 600]);
                            else if (result == 700)
                                info.file.revealToUser();
                            safe->repaint();
                        });
}

//==============================================================================
SequencesFacade::SequencesFacade (SequencesAudioProcessor& p)
    : audioProcessor (p),
      presetBar (p.presets, [] (float h, bool bold, float tracking) { return ui::font (h, bold, tracking); })
{
    setLookAndFeel (&lookAndFeel);
    setSize (width, height);

    constexpr int margin = 16, gap = 12, pad = 12, titleHeight = 24, cellHeight = 112;

    // --- Rangée 1 : OSC A, OSC B ---
    {
        const int w = (width - 2 * margin - gap) / 2;
        buildOsc (0, { margin, 56, w, 172 });
        buildOsc (1, { margin + w + gap, 56, w, 172 });
    }

    // --- Rangée 2 : VOICE, FILTER, DRIVE, ENV 1, ENV 2 ---
    {
        // Largeur de chaque colonne : les menus prennent plus de place que les potards
        constexpr int menuWidth = 74;
        const std::vector<std::pair<const char*, std::vector<bool>>> layout {
            { "VOICE",  { true, false, false, false, false, false, false } },
            { "FILTER", { true, false, false, false } },
            { "DRIVE",  { true, false } },
            { "ENV 1",  { false, false, false, false } },
            { "",       { false, false, false, false } } };

        int knobCells = 0, menuCells = 0;
        for (auto& l : layout) for (bool isMenu : l.second) (isMenu ? menuCells : knobCells) += 1;
        const int n = (int) layout.size();
        const int cell = (width - 2 * margin - n * 2 * pad - (n - 1) * gap - menuCells * menuWidth) / knobCells;

        std::vector<std::vector<juce::Rectangle<int>>> cellsOf;
        std::vector<juce::Rectangle<int>> r;
        int x = margin;
        for (auto& [name, columns] : layout)
        {
            int w = 0;
            for (bool isMenu : columns) w += isMenu ? menuWidth : cell;
            const juce::Rectangle<int> rect (x, 236, w + 2 * pad, 150);
            panels.push_back ({ name, rect });
            r.push_back (rect);

            std::vector<juce::Rectangle<int>> cs;
            int cx = rect.getX() + pad;
            for (bool isMenu : columns)
            {
                const int cw = isMenu ? menuWidth : cell;
                cs.push_back ({ cx, rect.getY() + titleHeight + 6, cw, cellHeight });
                cx += cw;
            }
            cellsOf.push_back (cs);
            x += rect.getWidth() + gap;
        }
        // Le dernier panneau prend le reste de la largeur (arrondis de la division)
        panels.back().bounds.setRight (width - margin);
        r.back().setRight (width - margin);
        auto at = [&] (int panel, int i) { return cellsOf[(size_t) panel][(size_t) i]; };

        auto stacked = [&] (juce::Rectangle<int> c, const char* firstLabel, const char* firstId, const char* secondLabel, const char* secondId)
        {
            captions.push_back ({ firstLabel, c.withHeight (14) });
            addInlineChoice (firstId, c.withTrimmedTop (18).withHeight (26).reduced (4, 0));
            captions.push_back ({ secondLabel, c.withTrimmedTop (56).withHeight (14) });
            addInlineChoice (secondId, c.withTrimmedTop (74).withHeight (26).reduced (4, 0));
        };

        captions.push_back ({ "MODE", at (0, 0).withHeight (14) });
        addInlineChoice ("voiceMode", at (0, 0).withTrimmedTop (34).withHeight (26).reduced (4, 0));
        addKnob ("glide", "GLIDE", at (0, 1));
        addKnob ("velocity", "VELO", at (0, 2));
        addKnob ("pitch", "PITCH", at (0, 3), true);
        addKnob ("noise", "NOISE", at (0, 4));
        addKnob ("pan", "PAN", at (0, 5), true);
        addKnob ("volume", "VOLUME", at (0, 6));
        addChip (seqs::srcVelocity, "VELO", { r[0].getRight() - 12 - 78, r[0].getY() + 6, 78, 20 });

        stacked (at (1, 0), "TYPE", "filterType", "SLOPE", "filterSlope");
        addKnob ("cutoff", "CUTOFF", at (1, 1));
        addKnob ("resonance", "RESO", at (1, 2));
        addKnob ("keyTrack", "KEY TRK", at (1, 3));

        stacked (at (2, 0), "TYPE", "driveType", "POS", "drivePos");
        addKnob ("drive", "AMOUNT", at (2, 1));

        addKnob ("ampAttack", "ATK", at (3, 0));
        addKnob ("ampDecay", "DEC", at (3, 1));
        addKnob ("ampSustain", "SUS", at (3, 2));
        addKnob ("ampRelease", "REL", at (3, 3));

        addChip (seqs::srcEnv2, "ENV 2", { r[4].getX() + 10, r[4].getY() + 6, 82, 20 });
        addKnob ("modAttack", "ATK", at (4, 0));
        addKnob ("modDecay", "DEC", at (4, 1));
        addKnob ("modSustain", "SUS", at (4, 2));
        addKnob ("modRelease", "REL", at (4, 3));
    }

    bankPanel = { margin, 394, width - 2 * margin, 208 };
    seqPanel = { margin, 610, width - 2 * margin, 282 };
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

void SequencesFacade::buildOsc (int osc, juce::Rectangle<int> area)
{
    panels.push_back ({ osc == 0 ? "OSC A" : "OSC B", area });

    addLabelledChoice (SequencesAudioProcessor::oscId (osc, "WarpMode"), "WARP", { area.getRight() - 12 - 186, area.getY() + 6, 186, 22 }, 44);

    tableViews[(size_t) osc] = std::make_unique<ui::WavetableView> (audioProcessor, osc);
    tableViews[(size_t) osc]->setBounds (area.getX() + 12, area.getY() + 34, 232, area.getHeight() - 44);
    addAndMakeVisible (*tableViews[(size_t) osc]);

    const int x0 = area.getX() + 12 + 232 + 10;
    const int cell = (area.getRight() - 12 - x0) / 6;
    auto at = [&] (int i) { return juce::Rectangle<int> (x0 + i * cell, area.getY() + 40, cell, 120); };

    addKnob (SequencesAudioProcessor::oscId (osc, "Level"), "LEVEL", at (0));
    addKnob (SequencesAudioProcessor::oscId (osc, "Pos"), "POS", at (1));
    addKnob (SequencesAudioProcessor::oscId (osc, "Oct"), "OCT", at (2), true);
    addKnob (SequencesAudioProcessor::oscId (osc, "Coarse"), "COARSE", at (3), true);
    addKnob (SequencesAudioProcessor::oscId (osc, "Fine"), "FINE", at (4), true);
    addKnob (SequencesAudioProcessor::oscId (osc, "Warp"), "WARP", at (5));
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
        addChip (c + 1, SequencesAudioProcessor::sourceNames()[c + 1], { x, y0 - 2, isLfo ? w - 74 : w, 22 });

        if (isLfo)
        {
            auto preview = std::make_unique<ui::LfoPreview> (audioProcessor, c);
            preview->setBounds (x + w - 70, y0, 70, 18);
            addAndMakeVisible (*preview);
            previews.push_back (std::move (preview));

            addLabelledChoice (SequencesAudioProcessor::lfoId (c, "Rate"), "RATE", { x, y0 + 26, w, 22 }, 40);
            addLabelledChoice (SequencesAudioProcessor::lfoId (c, "Shape"), "SHAPE", { x, y0 + 52, w, 22 }, 40);
            addLabelledChoice (SequencesAudioProcessor::lfoId (c, "Step"), "STEP", { x, y0 + 78, w / 2 + 14, 22 }, 40);
            addInlineChoice (SequencesAudioProcessor::lfoId (c, "Pol"), { x + w / 2 + 18, y0 + 78, w - w / 2 - 18, 22 });
            addKnob (SequencesAudioProcessor::lfoId (c, "Amt"), "AMT", { x, y0 + 104, w / 2, 70 }, false, true);
            addKnob (SequencesAudioProcessor::lfoId (c, "Phase"), "PHASE", { x + w / 2, y0 + 104, w - w / 2, 70 }, false, true);
        }
        else
        {
            const int k = c - seqs::numLfos;
            addLabelledChoice (SequencesAudioProcessor::chaosId (k, "Rate"), "RATE", { x, y0 + 26, w, 22 }, 40);
            addKnob (SequencesAudioProcessor::chaosId (k, "Amt"), "AMT", { x, y0 + 62, w / 2, 112 });
            addKnob (SequencesAudioProcessor::chaosId (k, "Smooth"), "SMOOTH", { x + w / 2, y0 + 62, w - w / 2, 112 });
        }
    }
}

void SequencesFacade::buildSequencer (juce::Rectangle<int> area)
{
    const int top = area.getY() + 10, h = 24;
    addChip (seqs::srcSeq, "SEQ", { area.getX() + 12, top, 92, h });
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

    const int gridWidth = ui::StepMatrix::labelWidth + SequencesAudioProcessor::numSteps * 25;
    matrix = std::make_unique<ui::StepMatrix> (audioProcessor);
    matrix->setBounds (area.getX() + 12, area.getY() + 44, gridWidth, ui::StepMatrix::numRows * 14);
    addAndMakeVisible (*matrix);

    circuit = std::make_unique<ui::CircuitView> (audioProcessor);
    circuit->setBounds (area.getX() + 12, matrix->getBottom() + 8, gridWidth, area.getBottom() - 10 - (matrix->getBottom() + 8));
    addAndMakeVisible (*circuit);

    const int rx = matrix->getRight() + 16, rw = area.getRight() - 12 - rx;
    int bx = area.getRight() - 12;
    auto placeRight = [&] (juce::TextButton& b, const juce::String& text, const juce::String& title, int w)
    {
        bx -= w;
        addButton (b, text, title, { bx, area.getY() + 44, w, 24 });
        bx -= 6;
    };
    randomAllButton.setComponentID ("random");
    randomButton.setComponentID ("random");
    placeRight (randomAllButton, "RANDOM ALL", "Random LFO bank and pattern", 100);
    placeRight (randomButton, "RANDOM", "Random pattern", 84);
    placeRight (clearButton, "CLEAR", "Clear pattern", 58);
    placeRight (rightButton, juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6")), "Shift right", 28);
    placeRight (leftButton, juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x80")), "Shift left", 28);

    randomButton.onClick = [this] { audioProcessor.randomizePattern(); repaint(); };
    randomAllButton.onClick = [this] { audioProcessor.randomizeAll(); repaint(); };
    clearButton.onClick = [this] { audioProcessor.fillPattern (0); repaint(); };
    leftButton.onClick = [this] { audioProcessor.rotatePattern (-1); repaint(); };
    rightButton.onClick = [this] { audioProcessor.rotatePattern (+1); repaint(); };

    // Liste des modulations
    modListArea = { rx, area.getY() + 78, rw, area.getBottom() - 10 - (area.getY() + 78) };
    modList = std::make_unique<ui::ModList> (audioProcessor);
    modViewport.setViewedComponent (modList.get(), false);
    modViewport.setScrollBarsShown (true, false);
    modViewport.setScrollBarThickness (6);
    modViewport.setBounds (modListArea.withTrimmedTop (18));
    modList->setBounds (0, 0, modViewport.getMaximumVisibleWidth(), modList->idealHeight());
    addAndMakeVisible (modViewport);
}

juce::Slider& SequencesFacade::addKnob (const juce::String& id, const juce::String& label, juce::Rectangle<int> cell, bool bipolar, bool small)
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
    return *knobs.back()->slider;
}

void SequencesFacade::addBar (const juce::String& id, const juce::String& label, juce::Rectangle<int> area, bool bipolar)
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
    addAndMakeVisible (s);
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
    addInlineChoice (id, cell.withTrimmedTop (34).withHeight (26).reduced (2, 0));
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
    const bool playing = audioProcessor.getPlayheadBeats() >= 0.0;

    const int step = audioProcessor.getPlayheadStep();
    if (step != shownStep || matrix->isMouseButtonDown())
    {
        shownStep = step;
        matrix->repaint();
    }

    if (playing || tick % 6 == 0)
    {
        circuit->repaint();
        for (auto* k : modKnobs)
            k->repaint();
        for (auto& t : tableViews)
            t->repaint();
    }

    if (tick % 3 == 0)
    {
        const int rows = (int) modList->rows().size();
        if (rows != shownRows)
        {
            shownRows = rows;
            modList->setBounds (0, 0, modViewport.getMaximumVisibleWidth(), modList->idealHeight());
            repaint (modListArea);
        }
        modList->repaint();
    }

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

    for (const auto& p : panels)
        drawPanel (p.bounds, p.title);
    drawPanel (bankPanel, "LFO BANK");
    drawPanel (seqPanel, "");

    g.setColour (Palette::dim());
    g.setFont (ui::font (9.5f, true, 0.16f));
    g.drawText ("DRAG A SOURCE ONTO A KNOB TO MODULATE IT", bankPanel.withTrimmedRight (14).withTrimmedTop (8).withHeight (16),
                juce::Justification::centredRight, false);

    // Séparateurs de colonnes de la banque
    g.setColour (Palette::panelLine());
    const int colWidth = (bankPanel.getWidth() - 24) / seqs::numSources;
    for (int c = 1; c < seqs::numSources; ++c)
        g.fillRect (bankPanel.getX() + 12 + c * colWidth - 6, bankPanel.getY() + 30, 1, bankPanel.getHeight() - 40);

    // En-tête de la liste des modulations
    g.setColour (Palette::ink());
    g.setFont (ui::font (10.0f, true, 0.22f));
    g.drawText ("MODULATION  " + juce::String (audioProcessor.usedSlotCount()) + " / " + juce::String (seqs::numSlots),
                modListArea.withHeight (14), juce::Justification::centredLeft, false);

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
