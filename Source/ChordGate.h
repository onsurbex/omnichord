#pragma once
#include <array>
#include <cstdint>

namespace chordharp {
// Independent MIDI channels, mouse buttons and Mac keys share last-note priority.
class ChordGate {
    struct Held { int chord=-1; std::uint64_t order=0; };
    std::array<Held,864> held{};
    std::uint64_t sequence=0;
public:
    void clear() { held={}; sequence=0; }
    void press(int source,int chord) {
        if(source>=0 && source<static_cast<int>(held.size())) held[static_cast<size_t>(source)]={chord,++sequence};
    }
    void release(int source) {
        if(source>=0 && source<static_cast<int>(held.size())) held[static_cast<size_t>(source)]={};
    }
    int latest() const {
        int chord=-1; std::uint64_t order=0;
        for(const auto& key:held) if(key.chord>=0 && key.order>order) { order=key.order; chord=key.chord; }
        return chord;
    }
};
}
