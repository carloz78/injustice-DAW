#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include <atomic>
namespace te = tracktion;

// Limitador de picos para el Master: la señal nunca pasa del techo (-1 dBFS por defecto).
// Ataque instantáneo (sin latencia) y relajación suave, con ganancia común para los dos canales.
class InjLimiter : public te::Plugin
{
public:
    static inline const char* xmlTypeName = "injlimiter";
    static const char* getPluginName() { return "Limitador"; }
    std::atomic<float> ceiling { 0.8913f };     // techo lineal (-1 dB)

    InjLimiter (te::PluginCreationInfo info) : te::Plugin (info) {}
    ~InjLimiter() override { notifyListenersOfDeletion(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Lim"; }
    juce::String getSelectableDescription() override { return getName(); }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    void initialise (const te::PluginInitialisationInfo& i) override { release = (float) std::exp (-1.0 / (0.08 * std::max (8000.0, i.sampleRate))); gain = 1.f; }
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); const float ceil = ceiling.load();
        float* ch[2] { nc > 0 ? fc.destBuffer->getWritePointer (0, fc.bufferStartSample) : nullptr, nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float pk = 0.f; for (int c = 0; c < nc; ++c) pk = std::max (pk, std::abs (ch[c][i]));
            const float need = pk * gain > ceil ? ceil / pk : 1.f;                 // lo que hay que bajar para no pasar el techo
            if (need < gain) gain = need; else gain = 1.f - (1.f - gain) * release; // ataque instantáneo, relajación lenta
            for (int c = 0; c < nc; ++c) ch[c][i] *= gain;
        }
    }
private:
    float gain = 1.f, release = 0.999f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjLimiter)
};
