#include "MacKeyboard.h"
#import <AppKit/AppKit.h>

namespace chordharp {
NativeKeyboardEvent currentMacKeyboardEvent() {
    NSEvent* event=[NSApp currentEvent];
    if(event==nil || ([event type]!=NSEventTypeKeyDown && [event type]!=NSEventTypeKeyUp)) return {};
    NativeKeyboardEvent result;
    result.physical=static_cast<int>([event keyCode]);
    result.isUp=[event type]==NSEventTypeKeyUp;
    // Spanish QWERTY positions stay stable when Control or Shift changes the text character.
    switch(result.physical) {
        case 0x0c: result.character=U'Q'; break; case 0x0d: result.character=U'W'; break;
        case 0x0e: result.character=U'E'; break; case 0x0f: result.character=U'R'; break;
        case 0x11: result.character=U'T'; break; case 0x10: result.character=U'Y'; break;
        case 0x20: result.character=U'U'; break; case 0x22: result.character=U'I'; break;
        case 0x1f: result.character=U'O'; break; case 0x23: result.character=U'P'; break;
        case 0x00: result.character=U'A'; break; case 0x01: result.character=U'S'; break;
        case 0x02: result.character=U'D'; break; case 0x03: result.character=U'F'; break;
        case 0x05: result.character=U'G'; break; case 0x04: result.character=U'H'; break;
        case 0x26: result.character=U'J'; break; case 0x28: result.character=U'K'; break;
        case 0x25: result.character=U'L'; break; case 0x29: result.character=U'Ñ'; break;
        case 0x06: result.character=U'Z'; break; case 0x07: result.character=U'X'; break;
        case 0x08: result.character=U'C'; break; case 0x09: result.character=U'V'; break;
        case 0x0b: result.character=U'B'; break; case 0x2d: result.character=U'N'; break;
        case 0x2e: result.character=U'M'; break; case 0x2b: result.character=U','; break;
        case 0x2f: result.character=U'.'; break; case 0x2c: result.character=U'-'; break;
        case 0x12: case 0x53: result.harp=0; break; case 0x13: case 0x54: result.harp=1; break;
        case 0x14: case 0x55: result.harp=2; break; case 0x15: case 0x56: result.harp=3; break;
        case 0x17: case 0x57: result.harp=4; break; case 0x16: case 0x58: result.harp=5; break;
        case 0x1a: case 0x59: result.harp=6; break; case 0x1c: case 0x5b: result.harp=7; break;
        case 0x19: case 0x5c: result.harp=8; break; case 0x1d: case 0x52: result.harp=9; break;
        case 0x1b: result.harp=10; break; case 0x18: result.harp=11; break;
        default: break;
    }
    return result;
}
}
