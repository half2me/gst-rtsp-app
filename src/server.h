//
// Created by pszekeres on 2017.05.18..
//

#ifndef GST_RTSP_APP_SERVER_H
#define GST_RTSP_APP_SERVER_H

#include <gst/rtsp-server/rtsp-server.h>
#include <vector>

class RtspServer {

public:

  RtspServer();
  ~RtspServer();

  gboolean Start();
  void Stop();

  gboolean RegisterRtspPipes(const std::vector<GstElement*>& pipes);

private:

  GstRTSPServer *gst_rtsp_server;


// Override default rtsp gst_rtsp_server mediafactory implementation
// -----------------------------------------------------------------

public:
  static GstElement * ImportPipeline (GstRTSPMediaFactory * factory, const GstRTSPUrl * url);
  static GstElement * CreateMediaPipe(GstRTSPMediaFactory *factory, GstRTSPMedia *media);
  static std::vector<GstElement*> rtsp_pipes;

private:
  // this timeout is periodically run to clean up the expired rtsp sessions from the pool.
  static gboolean SessionPoolTimeout(GstRTSPServer *server);
  static void StateChange(GstRTSPMedia *gstrtspmedia, gint arg1, gpointer user_data);
};

#endif //GST_RTSP_APP_SERVER_H
