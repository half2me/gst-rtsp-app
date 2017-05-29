#pragma once
#include <gst/gst.h>

// Global log settings
#define GST_APP_LOG_FILTER "GST_APP*"
#define GST_APP_LOG_LEVEL GST_LEVEL_DEBUG
#define GST_LOG_LEVEL GST_LEVEL_WARNING

class Logger {
public:

  static void Init();

};