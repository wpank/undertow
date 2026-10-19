#include "PluginEditor.h"
#include "PluginProcessor.h"

UndertowEditor::UndertowEditor(UndertowProcessor& p)
    : AudioProcessorEditor(p) {
    setSize(1200, 760);
    setResizable(false, false);
}

void UndertowEditor::paint(juce::Graphics& g) {
    // bg_void: #060608
    g.fillAll(juce::Colour::fromRGB(0x06, 0x06, 0x08));

    auto bounds = getLocalBounds();

    // Hairline border: #181420
    g.setColour(juce::Colour::fromRGB(0x18, 0x14, 0x20));
    g.drawRect(bounds, 1);

    // Product name in rose: #AA7088
    g.setColour(juce::Colour::fromRGB(0xAA, 0x70, 0x88));
    g.setFont(juce::Font(28.0f));
    g.drawText("Metakosmia Undertow", bounds, juce::Justification::centred);
}
