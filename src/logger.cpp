#include "logger.h"

void Logger::Init() {
  if (!g_getenv("GST_DEBUG")) {
    gst_debug_set_default_threshold(GST_LOG_LEVEL);
    gst_debug_set_threshold_for_name(GCF_APP_LOG_FILTER, GCF_APP_LOG_LEVEL);
    gst_debug_set_threshold_for_name(GCF_PLUGIN_LOG_FILTER, GCF_PLUGIN_LOG_LEVEL);
  }
}
