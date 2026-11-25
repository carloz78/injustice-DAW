#pragma once
#include "FxCore.h"
#include "AnalyzerDsp.h"
// INJUSTICE ANALYZER (Injustice Audio) dentro del DAW: analizador de espectro y multimetro (LUFS I/S, LRA, niveles L/R). El audio pasa sin cambios.
// Es el mismo motor y la misma pantalla del plugin VST3; aqui sus ajustes son parametros del efecto (se guardan con el proyecto).
namespace anopt
{
    inline constexpr int bandCounts[] { 21, 31, 42, 63, 84, 126, 252 };
    inline constexpr double holdSeconds[] { 0.5, 1.0, 2.0, 3.0, 5.0, 10.0 };
    inline constexpr float rateValues[] { 3.f, 6.f, 11.8f, 20.f, 30.f, 50.f, 100.f };
    inline constexpr float topValues[] { 10.f, 5.f, 0.f, -5.f, -10.f };
    inline constexpr float rangeValues[] { 40.f, 50.f, 60.f, 65.f, 80.f, 100.f };
    inline const juce::StringArray detection { "Left", "Right", "LRmax", "Mono" }, mode { "RMS Slow", "RMS Fast", "Peak" }, bands { "21", "31", "42", "63", "84", "126", "252" },
        holds { "0.5 s", "1 s", "2 s", "3 s", "5 s", "10 s" }, rates { "3 dB/s", "6 dB/s", "11.8 dB/s", "20 dB/s", "30 dB/s", "50 dB/s", "100 dB/s" }, levelMode { "Peak", "RMS", "Peak & RMS" },
        tops { "+10 dB", "+5 dB", "0 dB", "-5 dB", "-10 dB" }, ranges { "40 dB", "50 dB", "60 dB", "65 dB", "80 dB", "100 dB" }, fftSizes { "4096", "8192", "16384", "32768" };
}
namespace injfx
{
class InjAnalyzer : public InjFx
{
public:
    static inline const char* xmlTypeName = "injanalyzer";
    static const char* getPluginName() { return "Injustice Analyzer"; }
    InjAnalyzer (te::PluginCreationInfo i) : InjFx (i) { buildParams(); loud.prepare (44100.0); lvl.prepare (44100.0); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Analyzer"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 14; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;   // en AnalyzerEditor.cpp
    std::vector<Preset> presets() const override
    {   //                 Det Mod Ban SHt SHo SRt LMo LHt LHo LRt Top Rng FFT Cmp
        return { { "Master (65 dB)", { 3.f, 2.f, 3.f, 2.f, 0.f, 2.f, 2.f, 2.f, 0.f, 2.f, 1.f, 3.f, 2.f, 0.f } }, { "Graves y kick (126 bandas)", { 3.f, 1.f, 5.f, 2.f, 1.f, 3.f, 2.f, 2.f, 0.f, 3.f, 1.f, 1.f, 3.f, 0.f } },
                 { "Mezcla (RMS lento)", { 3.f, 0.f, 3.f, 2.f, 0.f, 1.f, 1.f, 2.f, 0.f, 1.f, 1.f, 3.f, 2.f, 0.f } }, { "Compacto (solo espectro)", { 3.f, 2.f, 3.f, 2.f, 0.f, 2.f, 2.f, 2.f, 0.f, 2.f, 1.f, 3.f, 2.f, 1.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sampleRate.store (i.sampleRate); loud.prepare (i.sampleRate); lvl.prepare (i.sampleRate); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; const int n = fc.bufferNumSamples, nch = fc.destBuffer->getNumChannels(); if (nch < 1 || n < 1) return;
        const float* l = fc.destBuffer->getReadPointer (0, fc.bufferStartSample); const float* r = nch > 1 ? fc.destBuffer->getReadPointer (1, fc.bufferStartSample) : l; const float* both[2] { l, r };
        loud.process (l, r, n); lvl.process (both, n); rl.push (l, n); rr.push (r, n);
    }
    // lo que usa la pantalla (mismo nombre que en el plugin VST3)
    static int idxOf (const char* id) { for (int i = 0; i < 14; ++i) if (std::strcmp (S[i].id, id) == 0) return i; return 0; }
    int choice (const char* id) const { return vi (idxOf (id)); }
    bool flag (const char* id) const { return vi (idxOf (id)) != 0; }
    void setChoice (const char* id, int index) { setValue (idxOf (id), (float) index); }
    void setFlag (const char* id, bool on) { setValue (idxOf (id), on ? 1.f : 0.f); }
    double getCurrentSampleRate() const { return sampleRate.load(); }
    an::LoudnessMeter& loudness() { return loud; }
    an::LevelMeter& level() { return lvl; }
    const an::SampleRing& ringL() const { return rl; }
    const an::SampleRing& ringR() const { return rr; }
    struct CompactRef { InjAnalyzer* p; bool load() const { return p->flag ("compact"); } void store (bool b) const { p->setFlag ("compact", b); } };
    CompactRef compactUi { this };
private:
    static inline const Spec S[] = {
        { "detection", "Detection", 0.f, 3.f, 3.f, 0.f, fChoice, "Left|Right|LRmax|Mono" }, { "mode", "Mode", 0.f, 2.f, 2.f, 0.f, fChoice, "RMS Slow|RMS Fast|Peak" },
        { "bands", "Analyzer Bands", 0.f, 6.f, 3.f, 0.f, fChoice, "21|31|42|63|84|126|252" }, { "specHold", "Spectrum Peak Hold Time", 0.f, 5.f, 2.f, 0.f, fChoice, "0.5 s|1 s|2 s|3 s|5 s|10 s" },
        { "specHoldOn", "Spectrum Peak Hold", 0.f, 1.f, 0.f, 0.f, fChoice, "Off|On" }, { "specRate", "Spectrum Return Rate", 0.f, 6.f, 2.f, 0.f, fChoice, "3 dB/s|6 dB/s|11.8 dB/s|20 dB/s|30 dB/s|50 dB/s|100 dB/s" },
        { "levelMode", "Level", 0.f, 2.f, 2.f, 0.f, fChoice, "Peak|RMS|Peak & RMS" }, { "levelHold", "Level Peak Hold Time", 0.f, 5.f, 2.f, 0.f, fChoice, "0.5 s|1 s|2 s|3 s|5 s|10 s" },
        { "levelHoldOn", "Level Peak Hold", 0.f, 1.f, 0.f, 0.f, fChoice, "Off|On" }, { "levelRate", "Level Return Rate", 0.f, 6.f, 2.f, 0.f, fChoice, "3 dB/s|6 dB/s|11.8 dB/s|20 dB/s|30 dB/s|50 dB/s|100 dB/s" },
        { "top", "Top", 0.f, 4.f, 1.f, 0.f, fChoice, "+10 dB|+5 dB|0 dB|-5 dB|-10 dB" }, { "range", "Range", 0.f, 5.f, 3.f, 0.f, fChoice, "40 dB|50 dB|60 dB|65 dB|80 dB|100 dB" },
        { "fft", "FFT Size", 0.f, 3.f, 2.f, 0.f, fChoice, "4096|8192|16384|32768" }, { "compact", "Vista compacta", 0.f, 1.f, 0.f, 0.f, fChoice, "Completa|Compacta" } };
    std::atomic<double> sampleRate { 44100.0 }; an::LoudnessMeter loud; an::LevelMeter lvl; an::SampleRing rl, rr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjAnalyzer)
};
} // namespace injfx
