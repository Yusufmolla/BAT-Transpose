#include "plugin/Processor.h"
#include "plugin/Ids.h"
#include "plugin/State.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <array>

namespace bat {
using namespace Steinberg;
using namespace Steinberg::Vst;
Processor::Processor() { setControllerClass(controllerId); }
tresult PLUGIN_API Processor::initialize(FUnknown* context) {
    auto result=AudioEffect::initialize(context);
    if(result!=kResultOk) return result;
    addAudioInput(STR16("Guitar"),SpeakerArr::kStereo);
    addAudioOutput(STR16("Transposed"),SpeakerArr::kStereo);
    return kResultOk;
}
tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement* in,int32 ni,SpeakerArrangement* out,int32 no) {
    if(ni!=1||no!=1||!in||!out||in[0]!=out[0]||(in[0]!=SpeakerArr::kMono&&in[0]!=SpeakerArr::kStereo)) return kResultFalse;
    auto result=AudioEffect::setBusArrangements(in,ni,out,no);
    if(result==kResultOk) channels=SpeakerArr::getChannelCount(in[0]);
    return result;
}
tresult PLUGIN_API Processor::canProcessSampleSize(int32 size) { return size==kSample32?kResultTrue:kResultFalse; }
tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup) {
    if(setup.symbolicSampleSize!=kSample32||setup.maxSamplesPerBlock<1) return kResultFalse;
    try {
        engine.setSemitones(semitones.load()); engine.setBypassed(bypassed.load());
        engine.prepare(setup.sampleRate,channels);
    } catch(...) { return kResultFalse; }
    return AudioEffect::setupProcessing(setup);
}
tresult PLUGIN_API Processor::setActive(TBool state) {
    if(state) engine.reset();
    return AudioEffect::setActive(state);
}
tresult PLUGIN_API Processor::setProcessing(TBool state) {
    if(state) engine.reset();
    return kResultOk;
}
tresult PLUGIN_API Processor::setState(IBStream* stream) {
    State state;
    if(!state.read(stream)) return kResultFalse;
    semitones.store(state.semitones); bypassed.store(state.bypass!=0);
    return kResultOk;
}
tresult PLUGIN_API Processor::getState(IBStream* stream) {
    return State{semitones.load(),bypassed.load()?1:0}.write(stream)?kResultOk:kResultFalse;
}
tresult PLUGIN_API Processor::process(ProcessData& data) {
    // Two fixed cursors honour all parameter offsets without allocating/sorting.
    struct Cursor { IParamValueQueue* queue=nullptr; int32 index=0,offset=0; ParamValue value=0; bool valid=false;
        void next() { valid=queue&&index<queue->getPointCount()&&queue->getPoint(index++,offset,value)==kResultOk; }
    } cursors[2];
    if(data.inputParameterChanges) for(int32 q=0;q<data.inputParameterChanges->getParameterCount();++q) {
        auto* queue=data.inputParameterChanges->getParameterData(q);
        if(queue&&(queue->getParameterId()==transposeId||queue->getParameterId()==bypassId)) {
            auto& c=cursors[queue->getParameterId()==transposeId?0:1]; c.queue=queue; c.next();
        }
    }
    engine.setSemitones(semitones.load()); engine.setBypassed(bypassed.load());
    const bool buffers=data.numOutputs>0&&data.outputs&&data.outputs[0].numChannels==channels
        &&data.outputs[0].channelBuffers32&&data.symbolicSampleSize==kSample32;
    int offset=0;
    do {
        for(int k=0;k<2;++k) {
            auto& c=cursors[k];
            while(c.valid&&c.offset<=offset) {
                if(std::isfinite(c.value)) {
                    if(k==0) { int st=decodeTranspose(c.value); semitones.store(st); engine.setSemitones(st); }
                    else { bool bp=c.value>=0.5; bypassed.store(bp); engine.setBypassed(bp); }
                }
                c.next();
            }
        }
        int end=data.numSamples;
        for(auto& c:cursors) if(c.valid) end=std::min(end,std::max(offset,c.offset));
        if(end<=offset) break;
        if(buffers) {
            const float* in[2]{}; float* out[2]{};
            for(int c=0;c<channels;++c) {
                out[c]=data.outputs[0].channelBuffers32[c]+offset;
                if(data.numInputs>0&&data.inputs&&data.inputs[0].numChannels>c
                    &&data.inputs[0].channelBuffers32&&data.inputs[0].channelBuffers32[c]
                    && !(data.inputs[0].silenceFlags&(uint64(1)<<c)))
                    in[c]=data.inputs[0].channelBuffers32[c]+offset;
            }
            engine.process(in,out,end-offset);
        }
        offset=end;
    } while(offset<data.numSamples);
    if(buffers) {
        uint64 silent=0;
        for(int c=0;c<channels;++c) {
            bool allZero=true;
            for(int i=0;i<data.numSamples;++i) if(data.outputs[0].channelBuffers32[c][i]!=0.0f) { allZero=false; break; }
            if(allZero) silent|=uint64(1)<<c;
        }
        data.outputs[0].silenceFlags=silent;
    }
    return kResultOk;
}
}
