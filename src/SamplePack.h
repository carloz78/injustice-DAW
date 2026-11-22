#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <mutex>
#include "PackDSP.h"

// Paquete de sonidos propio de INJUSTICE DAW (v8): ~500 sonidos estilo 909 / 808 / gabber / hardcore.
// Se genera por código la primera vez (sin derechos de autor), con sobremuestreo x2 y sin aliasing (ver PackDSP.h).
namespace samplepack
{
    inline juce::File packsDir() { return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("INJUSTICE DAW").getChildFile ("Injustice Packs"); }
    inline juce::File packDir (const juce::File& packs) { return packs.getChildFile ("INJUSTICE Core Pack"); }
    inline bool ready (const juce::File& packs) { return packDir (packs).getChildFile (".pack_v23").existsAsFile(); }

    inline void save (const juce::File& f, const packdsp::Buf& b)
    {
        const int n = (int) b.size(); if (n < 16) return;
        juce::AudioBuffer<float> buf (1, n); buf.copyFrom (0, 0, b.data(), n);
        f.getParentDirectory().createDirectory(); f.deleteFile(); juce::WavAudioFormat wav;
        if (auto os = f.createOutputStream()) if (std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), packdsp::OUT, 1, 24, {}, 0)); w) { os.release(); w->writeFromAudioSampleBuffer (buf, 0, n); }
    }

    // Crea el paquete si falta (una sola vez; seguro con varios hilos). Borra versiones anteriores. Devuelve la carpeta.
    inline juce::File ensure (const juce::File& packs)
    {
        static std::mutex m; std::lock_guard<std::mutex> lock (m);
        auto d = packDir (packs); auto marker = d.getChildFile (".pack_v23");
        if (marker.existsAsFile()) return d;
        d.deleteRecursively();
        // se genera en una carpeta oculta y se mueve al terminar: el navegador nunca ve un paquete a medias
        auto tmp = d.getSiblingFile (".INJUSTICE Core Pack.tmp"); tmp.deleteRecursively(); tmp.createDirectory();
        packdsp::catalog ([&] (const std::string& rel, packdsp::Buf&& b) { save (tmp.getChildFile (juce::String::fromUTF8 (rel.c_str())), packdsp::finalize (b, packdsp::peakFor (rel))); });
        tmp.getChildFile (".pack_v23").replaceWithText ("23");
        if (! tmp.moveFileTo (d)) { tmp.copyDirectoryTo (d); tmp.deleteRecursively(); }
        return d;
    }
}
