#pragma once
namespace chordharp {
struct NativeKeyboardEvent {
    int physical=-1;
    char32_t character=0;
    int harp=-1;
    bool isUp=false;
};
#if defined(__APPLE__)
NativeKeyboardEvent currentMacKeyboardEvent();
#else
inline NativeKeyboardEvent currentMacKeyboardEvent() { return {}; }
#endif
}
