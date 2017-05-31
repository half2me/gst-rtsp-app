#pragma once

#include <gst/rtsp-server/rtsp-server.h>
#include <vector>
#include <map>

class RtspServer {

public:

  RtspServer();
  ~RtspServer();

  gboolean Start();

  gboolean RegisterRtspPipes(const std::map<std::string, GstElement*>& pipes);

private:

  GstRTSPServer *gst_rtsp_server;
  guint gst_rtsp_server_source;


// Override default rtsp gst_rtsp_server mediafactory implementation
// -----------------------------------------------------------------
public:
  static GstElement * ImportPipeline (GstRTSPMediaFactory * factory, const GstRTSPUrl * url);
  static GstElement * CreateMediaPipe(GstRTSPMediaFactory *factory, GstRTSPMedia *media);
  static std::map<std::string, GstElement*> rtsp_pipes;
  static std::map<std::string, GstRTSPMedia*> medias;
  static std::map<std::string, GstElement*> intersinks;
  static std::map<std::string, GstElement*> queues;
  static std::map<std::string, bool> rtsp_active;
  static GstElement* TODO_tee;
  static GstElement* TODO_pipe;

private:
  // this timeout is periodically run to clean up the expired rtsp sessions from the pool.
  static gboolean SessionPoolTimeout(GstRTSPServer *server);
  static void StateChange(GstRTSPMedia *gstrtspmedia, gint arg1, gpointer user_data);
};
