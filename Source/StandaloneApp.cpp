#include <JuceHeader.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

class OmnichordApplication final : public juce::JUCEApplication {
public:
    OmnichordApplication() {
        juce::PropertiesFile::Options options;
        options.applicationName="Omnichord"; options.filenameSuffix=".settings";
        options.osxLibrarySubFolder="Application Support";
        properties.setStorageParameters(options);
    }
    const juce::String getApplicationName() override { return "Omnichord"; }
    const juce::String getApplicationVersion() override { return "0.5.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted(const juce::String&) override {}
    void initialise(const juce::String&) override {
        // Preserve audio/MIDI settings, but start from the chosen default preset.
        properties.getUserSettings()->removeValue("filterState");
        window=std::make_unique<juce::StandaloneFilterWindow>(getApplicationName(),juce::Colour(0xff292a2d),properties.getUserSettings(),false);
        window->setVisible(true);
    }
    void shutdown() override { window.reset(); properties.getUserSettings()->removeValue("filterState"); properties.saveIfNeeded(); }
    void systemRequestedQuit() override {
        if(juce::ModalComponentManager::getInstance()->cancelAllModalComponents()) {
            juce::Timer::callAfterDelay(100,[] { if(auto* app=juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit(); });
        } else quit();
    }
private:
    juce::ApplicationProperties properties;
    std::unique_ptr<juce::StandaloneFilterWindow> window;
};
JUCE_CREATE_APPLICATION_DEFINE(OmnichordApplication)
