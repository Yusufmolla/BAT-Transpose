#pragma once
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
namespace bat {
struct State {
    Steinberg::int32 semitones=0, bypass=0;
    bool read(Steinberg::IBStream* stream) {
        if(!stream) return false;
        Steinberg::IBStreamer r(stream,kLittleEndian);
        Steinberg::int32 magic=0,version=0,st=0,bp=0;
        if(!r.readInt32(magic)||magic!=0x42415431||!r.readInt32(version)||version!=1
            ||!r.readInt32(st)||!r.readInt32(bp)||st < -12||st > 12||(bp!=0&&bp!=1)) return false;
        semitones=st; bypass=bp; return true;
    }
    bool write(Steinberg::IBStream* stream) const {
        if(!stream) return false;
        Steinberg::IBStreamer w(stream,kLittleEndian);
        return w.writeInt32(0x42415431)&&w.writeInt32(1)&&w.writeInt32(semitones)&&w.writeInt32(bypass);
    }
};
}
