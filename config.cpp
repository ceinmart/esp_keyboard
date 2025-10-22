#include "config.h"

OutputBuffer outputBuffer;
Config config;
RsyslogState rsyslog;
// Mutex used to guard access to USB HID Keyboard
SemaphoreHandle_t keyboardMutex = NULL;
