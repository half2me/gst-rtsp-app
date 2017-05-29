#pragma once
#include <gst/gst.h>

// Global log settings
#define GST_APP_LOG_FILTER "GST_APP*"
#define GST_APP_LOG_LEVEL GST_LEVEL_INFO
#define GST_LOG_LEVEL GST_LEVEL_INFO

class Logger {
public:

  static void Init();

};