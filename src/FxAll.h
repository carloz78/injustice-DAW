#pragma once
#include "FxDynamics.h"
#include "FxTone.h"
#include "FxSpace.h"
#include "FxMix.h"
#include "FxDist.h"
#include "FxMotion.h"
#include "FxVoice.h"
#include "FxGlitch.h"
#include "FxUtil.h"
// Catálogo de los efectos integrados: familia, nombre visible, tipo interno y registro en el motor.
namespace injfx
{
struct Entry { const char* type; const char* label; const char* family; };
inline const std::vector<Entry>& catalog()
{
    static const std::vector<Entry> c {
        { InjEq::xmlTypeName, "Ecualizador", "Dinámica y mezcla" }, { InjComp::xmlTypeName, "Compresor", "Dinámica y mezcla" }, { InjLimit::xmlTypeName, "Limitador", "Dinámica y mezcla" }, { InjMulti::xmlTypeName, "Multibanda", "Dinámica y mezcla" },
        { InjGate::xmlTypeName, "Puerta de ruido", "Dinámica y mezcla" }, { InjDeEss::xmlTypeName, "De-esser", "Dinámica y mezcla" }, { InjTransient::xmlTypeName, "Transientes", "Dinámica y mezcla" },
        { InjDucker::xmlTypeName, "Bombeo (sidechain)", "Dinámica y mezcla" }, { InjWidener::xmlTypeName, "Ensanchador estéreo", "Dinámica y mezcla" },
        { InjSat::xmlTypeName, "Saturador", "Distorsión y color" }, { InjClipper::xmlTypeName, "Clipper", "Distorsión y color" }, { InjCrusher::xmlTypeName, "Bitcrusher", "Distorsión y color" }, { InjShaper::xmlTypeName, "Waveshaper", "Distorsión y color" },
        { InjKickDrive::xmlTypeName, "Drive de kick gabber", "Distorsión y color" }, { InjFilter::xmlTypeName, "Filtro", "Distorsión y color" }, { InjEqDj::xmlTypeName, "EQ de DJ", "Distorsión y color" },
        { InjAutoFilter::xmlTypeName, "Auto-filtro", "Distorsión y color" }, { InjFormant::xmlTypeName, "Filtro de formantes", "Distorsión y color" },
        { InjReverb::xmlTypeName, "Reverb", "Espacio y movimiento" }, { InjDelay::xmlTypeName, "Delay", "Espacio y movimiento" }, { InjTape::xmlTypeName, "Delay de cinta", "Espacio y movimiento" }, { InjMod::xmlTypeName, "Modulación", "Espacio y movimiento" },
        { InjReverse::xmlTypeName, "Reverso", "Espacio y movimiento" }, { InjAutoPan::xmlTypeName, "Panner automático", "Espacio y movimiento" }, { InjTremolo::xmlTypeName, "Trémolo", "Espacio y movimiento" }, { InjRing::xmlTypeName, "Modulación en anillo", "Espacio y movimiento" },
        { InjPitch::xmlTypeName, "Cambio de tono", "Voz y tono" }, { InjTune::xmlTypeName, "Afinador", "Voz y tono" }, { InjVocoder::xmlTypeName, "Vocoder", "Voz y tono" },
        { InjStutter::xmlTypeName, "Cortador rítmico", "Creativos" }, { InjBeatRepeat::xmlTypeName, "Repetidor de ritmo", "Creativos" }, { InjTapeStop::xmlTypeName, "Parada de cinta", "Creativos" }, { InjGranular::xmlTypeName, "Granular", "Creativos" }, { InjGlitch::xmlTypeName, "Glitch", "Creativos" },
        { InjGainPhase::xmlTypeName, "Ganancia y fase", "Utilidad" }, { InjMidSide::xmlTypeName, "Mid/Side", "Utilidad" }, { InjSpectrum::xmlTypeName, "Analizador de espectro", "Utilidad" }, { InjLufs::xmlTypeName, "Medidor LUFS", "Utilidad" } };
    return c;
}
inline void registerAll (te::Engine& e)
{
    auto& pm = e.getPluginManager();
    pm.createBuiltInType<InjEq>(); pm.createBuiltInType<InjComp>(); pm.createBuiltInType<InjLimit>(); pm.createBuiltInType<InjReverb>();
    pm.createBuiltInType<InjDelay>(); pm.createBuiltInType<InjSat>(); pm.createBuiltInType<InjFilter>(); pm.createBuiltInType<InjMod>();
    pm.createBuiltInType<InjDucker>(); pm.createBuiltInType<InjMulti>(); pm.createBuiltInType<InjGate>(); pm.createBuiltInType<InjDeEss>(); pm.createBuiltInType<InjTransient>(); pm.createBuiltInType<InjWidener>();
    pm.createBuiltInType<InjClipper>(); pm.createBuiltInType<InjCrusher>(); pm.createBuiltInType<InjShaper>(); pm.createBuiltInType<InjKickDrive>(); pm.createBuiltInType<InjEqDj>(); pm.createBuiltInType<InjAutoFilter>(); pm.createBuiltInType<InjFormant>();
    pm.createBuiltInType<InjTape>(); pm.createBuiltInType<InjReverse>(); pm.createBuiltInType<InjAutoPan>(); pm.createBuiltInType<InjTremolo>(); pm.createBuiltInType<InjRing>();
    pm.createBuiltInType<InjPitch>(); pm.createBuiltInType<InjTune>(); pm.createBuiltInType<InjVocoder>();
    pm.createBuiltInType<InjStutter>(); pm.createBuiltInType<InjBeatRepeat>(); pm.createBuiltInType<InjTapeStop>(); pm.createBuiltInType<InjGranular>(); pm.createBuiltInType<InjGlitch>();
    pm.createBuiltInType<InjGainPhase>(); pm.createBuiltInType<InjMidSide>(); pm.createBuiltInType<InjSpectrum>(); pm.createBuiltInType<InjLufs>();
}
} // namespace injfx
