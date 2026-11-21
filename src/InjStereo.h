#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include <atomic>
namespace te = tracktion;

// Separación estéreo por canal (como la de FL Studio): 0 % = mono, 100 % = tal cual, hasta 200 % = más ancho.
// Trabaja en mid/side; el valor se guarda en el estado del plugin (se conserva al guardar el proyecto).
class InjStereo : public te::Plugin
{
public:
    static inline const char* xmlTypeName = "injstereo";
    static const char* getPluginName() { return "Estéreo"; }
    std::atomic<float> width { 1.f };

    InjStereo (te::PluginCreationInfo info) : te::Plugin (info) { width.store ((float) (double) state.getProperty ("width", 1.0)); }
    ~InjStereo() override { notifyListenersOfDeletion(); }
    void setWidth (float w) { w = juce::jlimit (0.f, 2.f, w); width.store (w); state.setProperty ("width", (double) w, nullptr); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Est"; }
    juce::String getSelectableDescription() override { return getName(); }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    void initialise (const te::PluginInitialisationInfo&) override {}
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.destBuffer->getNumChannels() < 2) return;
        const float w = width.load(); if (std::abs (w - 1.f) < 0.001f) return;
        float* l = fc.destBuffer->getWritePointer (0, fc.bufferStartSample); float* r = fc.destBuffer->getWritePointer (1, fc.bufferStartSample);
        for (int i = 0; i < fc.bufferNumSamples; ++i) { const float m = (l[i] + r[i]) * .5f, s = (l[i] - r[i]) * .5f * w; l[i] = m + s; r[i] = m - s; }
    }
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjStereo)
};
