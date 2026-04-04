#ifndef SD_FUNCTIONS_H
#define SD_FUNCTIONS_H

#include <Arduino.h>

bool initSDCard();
bool writeToSD(const char* path, const String& data, bool append = true);
String readFromSD(const char* path);

#endif