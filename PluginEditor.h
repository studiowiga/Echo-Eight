#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <array>

// =============================================================================
//  Interface Eight Echo (Studio Wiga) sur le visuel fourni.
//  L'image de fond (background.png) est la pedale sans potards ni LED :
//  tous les elements actifs sont redessines en vectoriel par-dessus.
//  Coordonnees de dessin = pixels de l'image (578 x 896), mises a l'echelle.
// =============================================================================

class PedalView : public juce::Component, private juce::Timer
{
public:
    explicit PedalView (EightEchoProcessor&);
    ~PedalView() override;

    static constexpr float imgW = 578.0f, imgH = 896.0f;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;

    std::function<void (const juce::MouseEvent&)> onBackgroundRightClick;

private:
    struct Knob
    {
        juce::String id; juce::Point<float> c; float r; bool isMode;
        std::unique_ptr<juce::ParameterAttachment> att; float value = 0.5f; double def = 0.5;
    };

    EightEchoProcessor& proc;
    juce::Image bg, x2On, ledsOn, lensIce, lensRed, lensGreen, lensOrange;
    std::array<juce::Image, 5> pointers;
    std::vector<Knob> knobs;
    std::unique_ptr<juce::ParameterAttachment> longAtt;
    bool longOn = false;

    // etat souris
    int dragKnob = -1; float dragStartY = 0, dragStartVal = 0; int dragStartIndex = 0;
    enum Pressed { NONE, TAPFS, BYPASSFS };
    Pressed pressed = NONE; double tapPressTime = 0; bool twistSent = false;
    bool saveMode = false;
    float anim = 0;

    // geometrie (pixels de l'image)
    const juce::Point<float> cTap { 143, 775 }, cBypass { 438, 775 }, cX2 { 290, 778 };
    const juce::Point<float> cSave { 204, 488 }, cLedTempo { 144.5f, 621.1f }, cLedCheck { 434.2f, 620.2f };
    const juce::Point<float> cPrev { 141, 679 }, cNext { 436, 676 };
    float slotY (int i) const { const float ys[] = { 124, 175, 228, 279, 331, 383, 435 }; return ys[i]; }

    void timerCallback() override;
    int  knobAt (juce::Point<float>) const;
    int  modeLabelAt (juce::Point<float>) const;
    int  slotAt (juce::Point<float>) const;
    juce::Point<float> modeLabelPos (int i) const;
    juce::Rectangle<float> modeLabelRect (int i, juce::Justification& just) const;
    static float modeAngle (int i) { return juce::degreesToRadians (-135.0f + 27.0f * (float) i); }

    void setParam (const char* id, float v);
    void drawKnob (juce::Graphics&, const Knob&);
    void drawLedPipe (juce::Graphics&, int slot, float brightness);
    void drawLens (juce::Graphics&, bool left, const juce::Image* lit, float amount);
    juce::String statusLine() const;
    juce::Colour checkColour() const;
};

class EightEchoEditor : public juce::AudioProcessorEditor
{
public:
    explicit EightEchoEditor (EightEchoProcessor&);
    ~EightEchoEditor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    // Taille par defaut = taille minimale (impossible de reduire en dessous)
    static constexpr int baseW = 380, baseH = 493;

private:
    EightEchoProcessor& proc;
    PedalView pedal;
    juce::ComponentBoundsConstrainer constrainer;
    void showMenu();
    void setScale (float s);
    float currentScale() const { return (float) getWidth() / (float) baseW; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EightEchoEditor)
};
