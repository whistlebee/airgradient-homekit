#pragma once
#ifdef AG_HOMEKIT
#include "AgValue.h"
namespace homekit {
void begin(const String &serial, const char *version, void (*onConnected)(int));
void publish(Measurements &measurements);
void reset();
}
#endif
