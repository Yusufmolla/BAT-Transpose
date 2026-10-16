#pragma once
#include "public.sdk/source/vst/vsteditcontroller.h"
namespace bat {
class Controller final : public Steinberg::Vst::EditControllerEx1 {
public:
    static Steinberg::FUnknown* createInstance(void*) { return static_cast<Steinberg::Vst::IEditController*>(new Controller); }
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) override;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream*) override;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString) override;
};
Steinberg::IPlugView* createEditor(Controller*);
}
