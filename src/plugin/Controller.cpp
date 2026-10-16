#include "plugin/Controller.h"
#include "plugin/Ids.h"
#include "plugin/State.h"
namespace bat {
using namespace Steinberg;
using namespace Steinberg::Vst;
tresult PLUGIN_API Controller::initialize(FUnknown* context) {
    auto result=EditControllerEx1::initialize(context);
    if(result!=kResultOk) return result;
    auto* transpose=new RangeParameter(STR16("Transpose"),transposeId,STR16("st"),-12,12,0,24,ParameterInfo::kCanAutomate);
    transpose->setPrecision(0);
    parameters.addParameter(transpose);
    parameters.addParameter(STR16("Bypass"),nullptr,1,0,ParameterInfo::kCanAutomate|ParameterInfo::kIsBypass,bypassId);
    return kResultOk;
}
tresult PLUGIN_API Controller::setComponentState(IBStream* stream) {
    State s; if(!s.read(stream)) return kResultFalse;
    setParamNormalized(transposeId,encodeTranspose(s.semitones));
    setParamNormalized(bypassId,s.bypass?1:0);
    return kResultOk;
}
IPlugView* PLUGIN_API Controller::createView(FIDString name) {
    return FIDStringsEqual(name,ViewType::kEditor)?createEditor(this):nullptr;
}
}
