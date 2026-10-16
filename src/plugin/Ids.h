#pragma once
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"
#include <algorithm>
#include <cmath>
namespace bat {
inline const Steinberg::FUID processorId(0xBABA1047,0x628349AE,0xA76E9128,0x133083F1);
inline const Steinberg::FUID controllerId(0x95A01C27,0xCDA74C24,0x8C465DE9,0xEF8E3F50);
enum : Steinberg::Vst::ParamID { transposeId=0, bypassId=1 };
inline int decodeTranspose(double value) {
    return std::isfinite(value) ? int(std::lround(std::clamp(value,0.0,1.0)*24))-12 : 0;
}
inline double encodeTranspose(int value) { return (std::clamp(value,-12,12)+12)/24.0; }
}
