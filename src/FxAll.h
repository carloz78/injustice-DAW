#pragma once
#include "FxDynamics.h"
#include "FxPunch.h"
#include "FxFet.h"
#include "FxGlue.h"
#include "FxOptical.h"
#include "FxDual.h"
#include "FxStack.h"
#include "FxTubeSat.h"
#include "FxTape.h"
#include "FxEcho.h"
#include "FxMod3.h"
#include "FxAnalyzer.h"
#include "FxMband.h"
#include "FxCobalt.h"
#include "FxAttackShaper.h"
#include "FxDeEsser.h"
#include "FxGateDS.h"
#include "FxMaster.h"
#include "FxBlackBrown.h"
#include "FxAirlift.h"
#include "FxClassic.h"
#include "FxPulso.h"
#include "FxTone.h"
#include "FxSpace.h"
#include "FxMix.h"
#include "FxDist.h"
#include "FxMotion.h"
#include "FxVoice.h"
#include "FxTune.h"
#include "FxGlitch.h"
#include "FxUtil.h"
// Catálogo de los efectos integrados: familia, nombre visible, tipo interno y registro en el motor.
namespace injfx
{
struct Entry { const char* type; const char* label; const char* family; };
inline const std::vector<Entry>& catalog()
{
    static const std::vector<Entry> c {
        { InjPunch::xmlTypeName, "Punch Comp", "Dinámica y mezcla" }, { InjFet::xmlTypeName, "FET Compressor", "Dinámica y mezcla" }, { InjGlue::xmlTypeName, "Glue Comp", "Dinámica y mezcla" }, { InjOptical::xmlTypeName, "Optical Comp", "Dinámica y mezcla" }, { InjDual::xmlTypeName, "Dual Comp", "Dinámica y mezcla" }, { InjStack::xmlTypeName, "Stack Comp", "Dinámica y mezcla" }, { InjMband::xmlTypeName, "Multiband", "Dinámica y mezcla" }, { InjCobalt::xmlTypeName, "Cobalt Limiter", "Dinámica y mezcla" }, { InjAttackShaper::xmlTypeName, "Attack Shaper", "Dinámica y mezcla" }, { InjDeEsser::xmlTypeName, "De-Esser", "Dinámica y mezcla" }, { InjGateDS::xmlTypeName, "Dual Gate", "Dinámica y mezcla" }, { InjMaster::xmlTypeName, "Master EQ", "Dinámica y mezcla" }, { InjBlackBrown::xmlTypeName, "Black Brown EQ", "Dinámica y mezcla" }, { InjAirlift::xmlTypeName, "Airlift EQ", "Dinámica y mezcla" }, { InjClassic::xmlTypeName, "Classic EQ", "Dinámica y mezcla" }, { InjPulso::xmlTypeName, "Pulso EQ", "Dinámica y mezcla" }, 
        
        { InjDucker::xmlTypeName, "Bombeo (sidechain)", "Dinámica y mezcla" }, { InjWidener::xmlTypeName, "Ensanchador estéreo", "Dinámica y mezcla" },
        { InjTubeSat::xmlTypeName, "Tube Saturation", "Distorsión y color" }, { InjTapeSat::xmlTypeName, "Tape Saturation", "Distorsión y color" }, { InjSat::xmlTypeName, "Saturador", "Distorsión y color" }, { InjClipper::xmlTypeName, "Clipper", "Distorsión y color" }, { InjCrusher::xmlTypeName, "Bitcrusher", "Distorsión y color" }, { InjShaper::xmlTypeName, "Waveshaper", "Distorsión y color" },
        { InjKickDrive::xmlTypeName, "Drive de kick gabber", "Distorsión y color" }, { InjFilter::xmlTypeName, "Filtro", "Distorsión y color" }, { InjEqDj::xmlTypeName, "EQ de DJ", "Distorsión y color" },
        { InjAutoFilter::xmlTypeName, "Auto-filtro", "Distorsión y color" }, { InjFormant::xmlTypeName, "Filtro de formantes", "Distorsión y color" },
        { InjPhaserDual::xmlTypeName, "Phaser", "Espacio y movimiento" }, { InjFlangerPro::xmlTypeName, "Flanger", "Espacio y movimiento" }, { InjChorusDim::xmlTypeName, "Chorus", "Espacio y movimiento" }, { InjSpring::xmlTypeName, "Spring Reverb", "Espacio y movimiento" }, { InjTapeEcho::xmlTypeName, "Tape Echo", "Espacio y movimiento" }, { InjReverb::xmlTypeName, "Reverb", "Espacio y movimiento" }, { InjDelay::xmlTypeName, "Delay", "Espacio y movimiento" }, { InjTape::xmlTypeName, "Delay de cinta", "Espacio y movimiento" }, { InjMod::xmlTypeName, "Modulación", "Espacio y movimiento" },
        { InjReverse::xmlTypeName, "Reverso", "Espacio y movimiento" }, { InjAutoPan::xmlTypeName, "Panner automático", "Espacio y movimiento" }, { InjTremolo::xmlTypeName, "Trémolo", "Espacio y movimiento" }, { InjRing::xmlTypeName, "Modulación en anillo", "Espacio y movimiento" },
        { InjPitch::xmlTypeName, "Cambio de tono", "Voz y tono" }, { InjTunePro::xmlTypeName, "Injustice Tune", "Voz y tono" }, { InjVocoder::xmlTypeName, "Vocoder", "Voz y tono" },
        { InjStutter::xmlTypeName, "Cortador rítmico", "Creativos" }, { InjBeatRepeat::xmlTypeName, "Repetidor de ritmo", "Creativos" }, { InjTapeStop::xmlTypeName, "Parada de cinta", "Creativos" }, { InjGranular::xmlTypeName, "Granular", "Creativos" }, { InjGlitch::xmlTypeName, "Glitch", "Creativos" },
        { InjGainPhase::xmlTypeName, "Ganancia y fase", "Utilidad" }, { InjMidSide::xmlTypeName, "Mid/Side", "Utilidad" }, { InjAnalyzer::xmlTypeName, "Injustice Analyzer", "Utilidad" }, };
    return c;
}
inline void registerAll (te::Engine& e)
{
    auto& pm = e.getPluginManager();
    pm.createBuiltInType<InjTubeSat>(); pm.createBuiltInType<InjTapeSat>(); pm.createBuiltInType<InjSpring>(); pm.createBuiltInType<InjPhaserDual>(); pm.createBuiltInType<InjFlangerPro>(); pm.createBuiltInType<InjChorusDim>(); pm.createBuiltInType<InjTapeEcho>(); pm.createBuiltInType<InjEq>(); pm.createBuiltInType<InjComp>(); pm.createBuiltInType<InjPunch>(); pm.createBuiltInType<InjFet>(); pm.createBuiltInType<InjGlue>(); pm.createBuiltInType<InjOptical>(); pm.createBuiltInType<InjDual>(); pm.createBuiltInType<InjStack>(); pm.createBuiltInType<InjMband>(); pm.createBuiltInType<InjCobalt>(); pm.createBuiltInType<InjAttackShaper>(); pm.createBuiltInType<InjDeEsser>(); pm.createBuiltInType<InjGateDS>(); pm.createBuiltInType<InjMaster>(); pm.createBuiltInType<InjBlackBrown>(); pm.createBuiltInType<InjAirlift>(); pm.createBuiltInType<InjClassic>(); pm.createBuiltInType<InjPulso>(); pm.createBuiltInType<InjLimit>(); pm.createBuiltInType<InjReverb>();
    pm.createBuiltInType<InjDelay>(); pm.createBuiltInType<InjSat>(); pm.createBuiltInType<InjFilter>(); pm.createBuiltInType<InjMod>();
    pm.createBuiltInType<InjDucker>(); pm.createBuiltInType<InjMulti>(); pm.createBuiltInType<InjGate>(); pm.createBuiltInType<InjDeEss>(); pm.createBuiltInType<InjTransient>(); pm.createBuiltInType<InjWidener>();
    pm.createBuiltInType<InjClipper>(); pm.createBuiltInType<InjCrusher>(); pm.createBuiltInType<InjShaper>(); pm.createBuiltInType<InjKickDrive>(); pm.createBuiltInType<InjEqDj>(); pm.createBuiltInType<InjAutoFilter>(); pm.createBuiltInType<InjFormant>();
    pm.createBuiltInType<InjTape>(); pm.createBuiltInType<InjReverse>(); pm.createBuiltInType<InjAutoPan>(); pm.createBuiltInType<InjTremolo>(); pm.createBuiltInType<InjRing>();
    pm.createBuiltInType<InjPitch>(); pm.createBuiltInType<InjTune>(); pm.createBuiltInType<InjTunePro>(); pm.createBuiltInType<InjVocoder>();
    pm.createBuiltInType<InjStutter>(); pm.createBuiltInType<InjBeatRepeat>(); pm.createBuiltInType<InjTapeStop>(); pm.createBuiltInType<InjGranular>(); pm.createBuiltInType<InjGlitch>();
    pm.createBuiltInType<InjGainPhase>(); pm.createBuiltInType<InjMidSide>(); pm.createBuiltInType<InjSpectrum>(); pm.createBuiltInType<InjAnalyzer>(); pm.createBuiltInType<InjLufs>();
}
} // namespace injfx
