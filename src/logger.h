#pragma once
#include <gst/gst.h>

// Global log settings

#define GST_LOG_LEVEL GST_LEVEL_WARNING

#define GCF_APP_LOG_FILTER "GCF_APP_*"
#define GCF_APP_LOG_LEVEL GST_LEVEL_INFO

#define GCF_PLUGIN_LOG_FILTER "GCF_PLUGIN_*"
#define GCF_PLUGIN_LOG_LEVEL GST_LEVEL_INFO

#define GCF_ERROR_RETURN(B, ...) if (B) { GST_ERROR(__VA_ARGS__); return;}
#define GCF_WARNING_RETURN(B, ...) if (B) { GST_WARNING(__VA_ARGS__); return;}

class Logger {
public:

  static void Init();

};
