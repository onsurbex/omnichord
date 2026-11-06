#pragma once
#include <cstddef>

namespace chordharp {
// Callers keep delay within one buffer length. Rounding a tiny negative offset
// after wrapping can produce exactly length, which must address sample zero.
inline double circularReadPosition(std::size_t write,double delay,std::size_t length) {
    double position=static_cast<double>(write)-delay;
    const double size=static_cast<double>(length);
    if(position<0) position+=size;
    return position>=size?0.0:position;
}
}
