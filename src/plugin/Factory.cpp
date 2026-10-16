#include "plugin/Processor.h"
#include "plugin/Controller.h"
#include "plugin/Ids.h"
#include "public.sdk/source/main/pluginfactory.h"
BEGIN_FACTORY_DEF("BAT", "", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(bat::processorId),Steinberg::PClassInfo::kManyInstances,
    kVstAudioEffectClass,"BAT-transpose",Steinberg::Vst::kDistributable,"Fx|Pitch Shift",
    BAT_VERSION,kVstVersionString,bat::Processor::createInstance)
DEF_CLASS2(INLINE_UID_FROM_FUID(bat::controllerId),Steinberg::PClassInfo::kManyInstances,
    kVstComponentControllerClass,"BAT-transpose Controller",0,"",BAT_VERSION,kVstVersionString,bat::Controller::createInstance)
END_FACTORY
