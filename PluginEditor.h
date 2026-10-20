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
    void saveToSlot (int i);
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
    juce::Image bg, x2On, syncOn, ledsOn, lensIce, lensRed, lensGreen, lensOrange;
    std::array<juce::Image, 5> pointers;
    std::vector<Knob> knobs;
    std::unique_ptr<juce::ParameterAttachment> longAtt, syncAtt;
    bool longOn = false, syncParam = false;

    // etat souris
    int dragKnob = -1; float dragStartY = 0, dragStartVal = 0; int dragStartIndex = 0;
    enum Pressed { NONE, TAPFS, BYPASSFS };
    Pressed pressed = NONE; double tapPressTime = 0; bool twistSent = false;
    // memoires : clic court = rappel, clic maintenu 1 s = sauvegarde
    int slotPress = -1; double slotPressTime = 0; bool slotSaved = false;
    int flashSlot = -1; float flashStart = 0;
    float anim = 0;

    // geometrie (pixels de l'image)
    const juce::Point<float> cTap { 143, 775 }, cBypass { 438, 775 }, cX2 { 290, 778 };
    const juce::Point<float> cSync { 204, 491.5f }, cLedTempo { 144.5f, 621.1f }, cLedCheck { 434.2f, 620.2f };
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

// Contrainte de taille stricte : jamais plus petit que la taille par defaut, quel que soit
// le chemin du redimensionnement (coin de la fenetre, bord, hote VST3/AU, restauration du projet).
// JUCE applique le rapport largeur/hauteur APRES les limites, ce qui peut faire passer sous le minimum :
// on recadre donc une seconde fois, en gardant le rapport.
class StrictMinConstrainer : public juce::ComponentBoundsConstrainer
{
public:
    void checkBounds (juce::Rectangle<int>& b, const juce::Rectangle<int>& prev, const juce::Rectangle<int>& limits,
                      bool top, bool left, bool bottom, bool right) override
    {
        juce::ComponentBoundsConstrainer::checkBounds (b, prev, limits, top, left, bottom, right);
        const int mw = getMinimumWidth(), mh = getMinimumHeight();
        if (b.getWidth() < mw || b.getHeight() < mh)
        {
            if (left) b.setLeft (b.getRight() - mw); else b.setWidth (mw);
            if (top)  b.setTop (b.getBottom() - mh); else b.setHeight (mh);
        }
        // rapport fixe : la hauteur suit toujours la largeur (jamais en dessous du minimum)
        const int h = juce::roundToInt ((double) b.getWidth() * (double) mh / (double) mw);
        if (top) b.setTop (b.getBottom() - h); else b.setHeight (h);
    }
};

class EightEchoEditor : public juce::AudioProcessorEditor
{
public:
    explicit EightEchoEditor (EightEchoProcessor&);
    ~EightEchoEditor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    // Taille par defaut = taille minimale (impossible de reduire en dessous).
    // Meme gabarit que Textural / Freeze Shimmer : fenetre 1984 x 2000 a 100 %, 489 x 493 par defaut,
    // la pedale occupe ~90 % de la hauteur.
    static constexpr int baseW = 489, baseH = 493;
    static constexpr int refW = 1984, refH = 2000;
    static constexpr float maxScale = (float) refH / (float) baseH;

private:
    EightEchoProcessor& proc;
    PedalView pedal;
    StrictMinConstrainer constrainer;
    void showMenu();
    void setScale (float s);
    juce::Rectangle<float> pedalArea() const;      // emplacement de la pedale (coordonnees taille par defaut)
    float currentScale() const { return juce::jmax (1.0f, (float) getWidth() / (float) baseW); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EightEchoEditor)
};
