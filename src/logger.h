#pragma once
#include <gst/gst.h>

// Global log settings

#define GST_LOG_LEVEL GST_LEVEL_WARNING

#define GCF_APP_LOG_FILTER "GCF_APP_*"
#define GCF_APP_LOG_LEVEL GST_LEVEL_DEBUG

#define GCF_PLUGIN_LOG_FILTER "GCF_PLUGIN_*"
#define GCF_PLUGIN_LOG_LEVEL GST_LEVEL_DEBUG

class Logger {
public:

  static void Init();

};
