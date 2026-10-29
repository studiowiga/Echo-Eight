#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>
#include "DelayEngine.h"

// =============================================================================
//  Eight Echo (Studio Wiga) - processeur
//  Toute la logique du footswitch (clic, double appui, appui long) est ici,
//  pilotee par le parametre "footswitch" : le footswitch se comporte donc
//  exactement pareil a la souris ou via un controleur MIDI assigne dans le DAW.
// =============================================================================
class EightEchoProcessor : public juce::AudioProcessor,
                           private juce::Timer
{
public:
    EightEchoProcessor();
    ~EightEchoProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // ---- etat expose a l'interface (LED, ligne d'etat) ----
    enum LedState { LED_OFF = 0, LED_RED, LED_GREEN, LED_ORANGE, LED_RED_BLINK, LED_GREEN_BLINK, LED_REDGREEN_BLINK };
    std::atomic<int>   ledState { LED_RED };
    std::atomic<float> effectiveDelayMs { 300.0f };
    std::atomic<bool>  tapModeActive { false };
    std::atomic<int>   looperState { 0 };
    std::atomic<float> loopSeconds { 0.0f };
    std::atomic<float> tapBlinkPhase { 0.0f };   // phase dans le temps en cours (0 = debut du temps)
    std::atomic<int>   tapBeat { 0 };            // numero du temps dans la mesure (0 = premier temps)
    std::atomic<bool>  tapAccent { false };      // true : le premier temps est accentue (DAW / tap)
    std::atomic<float> hostBpm { 0.0f };         // tempo du DAW (0 = inconnu)
    std::atomic<bool>  hostLocked { false };     // SYNC actif ET tempo du DAW recu
    std::atomic<float> twistAmount { 0.0f };
    std::atomic<float> warpAmount { 0.0f };
    std::atomic<bool>  glitching { false };

    // Echelle de la fenetre (memorisee avec le projet)
    std::atomic<float> uiScale { 1.0f };

    static const juce::StringArray& tapDivisionNames();

    // ---- 7 memoires (LED 1 a 7 du visuel) ----
    static constexpr int kNumSlots = 7;
    void loadSlot (int i);
    void saveSlot (int i);
    int  getCurrentSlot() const { return currentSlot; }
    bool slotMatchesCurrent (int i) const;
    static const juce::StringArray& slotParamIds();

private:
    wiga::DelayEngine engine;
    double sr = 48000.0;
    int currentProgram = 0;
    std::array<juce::ValueTree, kNumSlots> slots;
    int currentSlot = 0;
    float loopLevel = 0.5f;
    void initSlots();

    // footswitch
    bool fsPrev = false; int64_t fsPressSample = 0; bool fsConsumed = false; bool fsHoldFx = false;
    int64_t lastLoopReleaseSample = -1000000000; int64_t sampleClock = 0;
    bool tapPrev = false, twistPrevForLoop = false;

    // tap tempo
    std::vector<int64_t> taps; float tapQuarterMs = 0.0f; bool tapOverride = false; float knobAtTap = -1.0f;
    int tapSeq = 0;                       // rang du tap dans la serie en cours
    double ledPhase = 0.0; int ledBeat = 0; bool ledResync = false;

    void handleFootswitch (bool pressed, int mode, int blockLen);
    void registerTap();

    juce::AudioParameterBool* pOn = nullptr;
    std::atomic<float>* p(const char* id) { return apvts.getRawParameterValue (id); }

    // Anti-gel v2 : parametres lus par le thread audio via des pointeurs obtenus une fois (constructeur),
    // jamais de recherche par chaine dans processBlock
    std::atomic<float> *rMode = nullptr, *rFootswitch = nullptr, *rTap = nullptr, *rTwist = nullptr,
                       *rExp = nullptr, *rLevel = nullptr, *rFeedback = nullptr, *rTime = nullptr,
                       *rExpLevel = nullptr, *rExpFeedback = nullptr, *rExpTime = nullptr, *rLong = nullptr,
                       *rSync = nullptr, *rDivision = nullptr, *rDryKill = nullptr, *rMix = nullptr,
                       *rStereo = nullptr, *rOn = nullptr, *rCarryover = nullptr;
    std::vector<float> monoTmp;           // tampon mono prepare hors du thread audio

    // Anti-gel v2 : depuis le thread audio, aucun appel a l'hote ; les valeurs sont mises en file
    // et transmises au DAW par un timer du thread de messages (60 Hz).
    void timerCallback() override;
    void setParamFromAudio (const char* id, float plain);
    float queuedOrCurrent (const char* id, std::atomic<float>* current) const;
    static constexpr int numQueued = 1;
    static const char* queuedId (int i);
    std::atomic<float> pendingValue[numQueued];
    std::atomic<bool>  pendingSet[numQueued];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EightEchoProcessor)
};
