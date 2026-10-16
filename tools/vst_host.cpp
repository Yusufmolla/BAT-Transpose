#include <cmath>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstcomponent.h"

#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/base/keycodes.h"
#if defined(_WIN32)
#include <windows.h>
#include <objbase.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif
#include <thread>
#include <iostream>
#include <string>

#include <chrono>


using namespace Steinberg;
using namespace Steinberg::Vst;
class Handler : public U::Implements<U::Directly<IComponentHandler>> {
public:
    tresult PLUGIN_API beginEdit(ParamID) override { return kResultOk; }
    tresult PLUGIN_API performEdit(ParamID,ParamValue) override { return kResultOk; }
    tresult PLUGIN_API endEdit(ParamID) override { return kResultOk; }
    tresult PLUGIN_API restartComponent(int32) override { return kResultOk; }
};
void pumpMessages() {
#if defined(_WIN32)
    MSG msg{};
    while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
#elif defined(__APPLE__)
    CFRunLoopRunInMode(kCFRunLoopDefaultMode,0,true);
#endif
}
std::string narrow(const TChar* text) { char b[512]{}; UString(const_cast<TChar*>(text),128).toAscii(b,512); return b; }
int main(int argc,char** argv) try {
    if(argc<2) { std::cerr<<"bat_host module.vst3 [--state-test | --editor-test]\nWith no option, lists parameters.\n"; return 2; }
#if defined(_WIN32)
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
#endif
    auto host=owned(new HostApplication);
    PluginContextFactory::instance().setPluginContext(host);
    std::string error;
    auto module=VST3::Hosting::Module::create(std::filesystem::absolute(argv[1]).lexically_normal().string(),error);
    if(!module) throw std::runtime_error(error);
    module->getFactory().setHostContext(host);
    for(auto& info:module->getFactory().classInfos()) {
        if(info.category()!=kVstAudioEffectClass) continue;
        PlugProvider provider(module->getFactory(),info,true);
        if(!provider.initialize()) throw std::runtime_error("Plugin initialization failed");
        auto component=provider.getComponentPtr();
        auto controller=provider.getControllerPtr();
        FUnknownPtr<IAudioProcessor> processor(component);
        if(!controller||!processor) throw std::runtime_error("Missing audio processor/controller");
        auto handler=owned(new Handler);
        controller->setComponentHandler(handler);
        std::cout<<"Plugin: "<<info.name()<<" "<<info.version()<<"\n";
        if(argc>=3&&std::string(argv[2])=="--state-test") {
            ParameterChanges edits(2); int32 index=0;
            edits.addParameterData(0,index)->addPoint(0,5.0/24,index);
            edits.addParameterData(1,index)->addPoint(0,1,index);
            ProcessData flush{}; flush.inputParameterChanges=&edits;
            processor->process(flush);
            MemoryStream saved;
            if(component->getState(&saved)!=kResultOk) throw std::runtime_error("Cannot save state");
            edits.clearQueue(); edits.addParameterData(0,index)->addPoint(0,.5,index);
            edits.addParameterData(1,index)->addPoint(0,0,index); processor->process(flush);
            saved.seek(0,IBStream::kIBSeekSet,nullptr);
            if(component->setState(&saved)!=kResultOk) throw std::runtime_error("Cannot restore state");
            saved.seek(0,IBStream::kIBSeekSet,nullptr);
            if(controller->setComponentState(&saved)!=kResultOk) throw std::runtime_error("Controller state sync failed");
            if(std::abs(controller->getParamNormalized(0)-5.0/24)>1e-8||controller->getParamNormalized(1)!=1)
                throw std::runtime_error("State values differ");
            MemoryStream invalid; int32 corrupt=123;
            invalid.write(&corrupt,4,nullptr); invalid.seek(0,IBStream::kIBSeekSet,nullptr);
            if(component->setState(&invalid)==kResultOk) throw std::runtime_error("Corrupt state accepted");
            MemoryStream check; component->getState(&check);
            if(check.getSize()!=saved.getSize()||std::memcmp(check.getData(),saved.getData(),size_t(saved.getSize())))
                throw std::runtime_error("Corrupt state changed parameters");
            std::cout<<"Transpose/bypass state round-trip and corrupt state rejection passed\n";
            return 0;
        }
#if defined(_WIN32)
        if(argc>=3&&std::string(argv[2])=="--editor-test") {
            auto view=owned(controller->createView(ViewType::kEditor));
            if(!view) throw std::runtime_error("No editor");
            HWND parent=CreateWindowExW(0,L"STATIC",L"BAT editor test",WS_OVERLAPPED,0,0,420,300,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
            if(!parent||view->attached(parent,kPlatformTypeHWND)!=kResultOk) throw std::runtime_error("Cannot attach editor");
            HWND child=GetWindow(parent,GW_CHILD);
            if(!child) throw std::runtime_error("Editor child missing");
            if(view->onKeyDown(0,KEY_DOWN,0)!=kResultTrue || std::abs(controller->getParamNormalized(0)-11.0/24)>1e-8)
                throw std::runtime_error("Editor key/parameter failed");
            if(view->onKeyDown(0,KEY_HOME,0)!=kResultTrue || controller->getParamNormalized(0)!=.5)
                throw std::runtime_error("Editor reset failed");
            if(view->onKeyDown('A',0,0)==kResultTrue) throw std::runtime_error("Editor swallows unhandled keys");
            SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(310,139));
            SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(310,139));
            if(std::abs(controller->getParamNormalized(0)-13.0/24)>1e-8) throw std::runtime_error("Editor plus click failed");
            SendMessageW(child,WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(210,139));
            SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(210,139));
            if(controller->getParamNormalized(0)!=.5) throw std::runtime_error("Editor double-click failed");
            SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(210,139));
            SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(210,121));
            SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(210,121));
            if(std::abs(controller->getParamNormalized(0)-15.0/24)>1e-8) throw std::runtime_error("Editor drag failed");
            view->onKeyDown(0,KEY_HOME,0);
            view->onWheel(-1);
            if(std::abs(controller->getParamNormalized(0)-11.0/24)>1e-8) throw std::runtime_error("Editor wheel failed");
            controller->setParamNormalized(0,5.0/24);
            for(int i=0;i<5;++i) { pumpMessages(); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
            view->removed();
            // Cover both reuse of the same IPlugView and creation of a new view.
            if(view->attached(parent,kPlatformTypeHWND)!=kResultOk) throw std::runtime_error("Cannot reattach editor");
            view->removed(); view=nullptr;
            view=owned(controller->createView(ViewType::kEditor));
            if(view->attached(parent,kPlatformTypeHWND)!=kResultOk) throw std::runtime_error("Cannot reopen editor");
            view->removed(); view=nullptr; DestroyWindow(parent);
            std::cout<<"Editor attach, host keys, click/drag, reset, wheel, reattach and reopen passed\n";
            return 0;
        }
#else
        if(argc>=3&&std::string(argv[2])=="--editor-test")
            throw std::runtime_error("The native editor smoke test currently requires Windows");
#endif
        if(argc==2) {
            for(int32 i=0;i<controller->getParameterCount();++i) {
                ParameterInfo p{}; controller->getParameterInfo(i,p);
                auto v=controller->getParamNormalized(p.id); String128 label{};
                controller->getParamStringByValue(p.id,v,label);
                std::cout<<p.id<<"\t"<<narrow(p.title)<<"\t"<<v<<"\t"<<narrow(label)<<"\tsteps="<<p.stepCount<<"\n";
            }
            std::cout<<"Initial reported latency: "<<processor->getLatencySamples()<<"\n";
            return 0;
        }
        throw std::runtime_error("Unknown test option");
    }
    throw std::runtime_error("No audio class found");
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
