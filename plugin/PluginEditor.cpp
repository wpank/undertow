#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "ui/Theme.h"

UndertowEditor::UndertowEditor(UndertowProcessor& processor)
    : AudioProcessorEditor(processor),
      processor_(processor) {
    setSize(undertow::ui::theme::editorWidth, undertow::ui::theme::editorHeight);
    setResizable(false, false);
}

void UndertowEditor::paint(juce::Graphics& g) {
    g.fillAll(undertow::ui::theme::bgVoid);

    g.setColour(undertow::ui::theme::border);
    g.drawRect(getLocalBounds(), 1);

    g.setColour(undertow::ui::theme::rose);
    g.setFont(undertow::ui::theme::sansFont(28.0f, juce::Font::bold));
    g.drawFittedText("Metakosmia Undertow 2", getLocalBounds().reduced(48), juce::Justification::centred, 1);
}
