#include "plugin/Controller.h"
#include "plugin/Ids.h"
#include "public.sdk/source/vst/vstguieditor.h"
#include "pluginterfaces/base/keycodes.h"
#if SMTG_OS_LINUX
#include "public.sdk/source/vst/vstgui_linux_runloop_support.h"
#include "vstgui/lib/platform/linux/linuxfactory.h"
#endif
#include <cmath>
#include <string>

namespace bat {
using namespace Steinberg;
using namespace VSTGUI;
namespace {
constexpr CColor background{20,23,28,255}, accent{131,235,178,255};

// Drawing and interaction are shared by HWND, NSView and X11 hosts.
class TransposeView final : public CView {
public:
    explicit TransposeView(Controller* c) : CView({0,0,420,300}), controller(c) {
        setWantsFocus(true);
        setWantsIdle(true);
    }
    ~TransposeView() override { endDrag(); }
    int value() const { return decodeTranspose(controller->getParamNormalized(transposeId)); }
    void edit(int st) {
        const auto v=encodeTranspose(st);
        if(v==controller->getParamNormalized(transposeId)) return;
        if(!dragging) controller->beginEdit(transposeId);
        controller->setParamNormalized(transposeId,v);
        controller->performEdit(transposeId,v);
        if(!dragging) controller->endEdit(transposeId);
        invalid();
    }
    void endDrag() {
        if(dragging) { dragging=false; controller->endEdit(transposeId); }
    }
    void onIdle() override {
        const int st=value();
        const bool bypass=controller->getParamNormalized(bypassId)>=.5;
        if(st!=lastValue||bypass!=lastBypass) {
            lastValue=st; lastBypass=bypass; invalid();
        }
    }
    void draw(CDrawContext* dc) override {
        dc->setDrawMode(kAntiAliasing);
        dc->setFillColor(background); dc->drawRect(getViewSize(),kDrawFilled);
        label(dc,"BAT / TRANSPOSE",20,25,16,{178,190,201,255},kBoldFace);
        dc->setFrameColor({54,64,73,255}); dc->setLineWidth(2);
        dc->drawEllipse({139,68,281,210});
        const int st=value();
        const double angle=(st/12.0)*2.35-1.57079632679;
        dc->setFrameColor(accent); dc->setLineWidth(5);
        dc->drawLine({210+std::cos(angle)*57,139+std::sin(angle)*57},
                     {210+std::cos(angle)*67,139+std::sin(angle)*67});
        const auto number=(st>0?"+":"")+std::to_string(st);
        label(dc,number.c_str(),93,67,50,{237,245,240,255},kBoldFace);
        label(dc,"SEMITONES",162,20,11,{143,159,171,255});
        dc->setFont(kNormalFont,22); dc->setFontColor({170,190,183,255});
        dc->drawString("-",{81,117,127,161}); dc->drawString("+",{293,117,339,161});
        label(dc,controller->getParamNormalized(bypassId)>=.5?"BYPASSED":
            (st==0?"ORIGINAL PITCH":"TRANSPOSE ACTIVE"),225,20,12,accent);
        label(dc,"Drag / scroll     Double-click to reset",263,18,11,{131,145,157,255});
        setDirty(false);
    }
    void onMouseDownEvent(MouseDownEvent& e) override {
        if(!e.buttonState.isLeft()) return;
        if(getFrame()) getFrame()->setFocusView(this);
        if(e.clickCount==2) { endDrag(); edit(0); e.consumed=true; return; }
        const auto p=e.mousePosition;
        if(p.y<68||p.y>210) return;
        if(p.x<139) edit(value()-1);
        else if(p.x>281) edit(value()+1);
        else {
            endDrag(); dragging=true; startY=p.y; startValue=value();
            controller->beginEdit(transposeId);
        }
        e.consumed=true;
    }
    void onMouseMoveEvent(MouseMoveEvent& e) override {
        if(dragging) { edit(startValue+int((startY-e.mousePosition.y)/6)); e.consumed=true; }
    }
    void onMouseUpEvent(MouseUpEvent& e) override { endDrag(); e.consumed=true; }
    void onMouseCancelEvent(MouseCancelEvent& e) override { endDrag(); e.consumed=true; }
    void onMouseWheelEvent(MouseWheelEvent& e) override {
        if(e.deltaY!=0) { edit(value()+(e.deltaY>0?1:-1)); e.consumed=true; }
    }
    bool key(int16 code) {
        switch(code) {
        case KEY_UP: case KEY_RIGHT: edit(value()+1); return true;
        case KEY_DOWN: case KEY_LEFT: edit(value()-1); return true;
        case KEY_HOME: edit(0); return true;
        default: return false;
        }
    }
    void onKeyboardEvent(KeyboardEvent& e) override {
        if(e.type==EventType::KeyDown&&e.modifiers.empty()) e.consumed=key(int16(e.virt));
    }
private:
    IPtr<Controller> controller;
    bool dragging=false, lastBypass=false;
    CCoord startY=0;
    int startValue=0, lastValue=99;
    static void label(CDrawContext* dc,const char* text,double top,double height,double size,
                      CColor color,int style=kNormalFace) {
        dc->setFont(kNormalFont,size,style); dc->setFontColor(color);
        dc->drawString(text,{0,top,420,top+height});
    }
};

class Editor final : public Vst::VSTGUIEditor {
public:
    explicit Editor(Controller* c) : VSTGUIEditor(c), owner(c) {
        ViewRect size{0,0,420,300}; setRect(size); setIdleRate(33);
    }
    ~Editor() override { close(); }
    bool PLUGIN_API open(void* parent,const PlatformType& type) override {
        if(!parent||frame) return false;
        frame=new CFrame({0,0,420,300},this);
        frame->setBackgroundColor(background);
        view=new TransposeView(owner);
        frame->addView(view);
        if(!frame->open(parent,type)) { close(); return false; }
        frame->setFocusView(view);
        return true;
    }
    void PLUGIN_API close() override {
        if(view) view->endDrag();
        view=nullptr;
        if(frame) { frame->close(); frame=nullptr; }
    }
    tresult PLUGIN_API attached(void* parent,FIDString type) override {
        if(!parent||frame||!type) return kInvalidArgument;
        const auto result=VSTGUIEditor::attached(parent,type);
        return frame?result:kResultFalse; // SDK base does not propagate open() failure.
    }
    tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override {
        if(!type) return kInvalidArgument;
#if SMTG_OS_LINUX
        return FIDStringsEqual(type,kPlatformTypeX11EmbedWindowID)?kResultTrue:kResultFalse;
#else
        return VSTGUIEditor::isPlatformTypeSupported(type);
#endif
    }
    tresult PLUGIN_API setFrame(IPlugFrame* hostFrame) override {
#if SMTG_OS_LINUX
        // Hosts expose the runloop on IPlugFrame or the factory context.
        const auto* factory=VSTGUI::getPlatformFactory().asLinuxFactory();
        if(hostFrame&&factory&&!factory->getRunLoop()) Steinberg::Linux::setupVSTGUIRunloop(hostFrame);
#endif
        return VSTGUIEditor::setFrame(hostFrame);
    }
    tresult PLUGIN_API onKeyDown(char16,int16 code,int16 modifiers) override {
        return view&&modifiers==0&&view->key(code)?kResultTrue:kResultFalse;
    }
    tresult PLUGIN_API onWheel(float distance) override {
        if(!view||distance==0) return kResultFalse;
        view->edit(view->value()+(distance>0?1:-1)); return kResultTrue;
    }
private:
    IPtr<Controller> owner;
    TransposeView* view=nullptr; // Owned by CFrame.
};
}
IPlugView* createEditor(Controller* controller) { return new Editor(controller); }
}
