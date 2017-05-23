//
// Created by pszekeres on 2017.05.18..
//

#ifndef GST_RTSP_APP_SERVER_H
#define GST_RTSP_APP_SERVER_H

#include <gst/rtsp-server/rtsp-server.h>

#define MAX_RTSP_PIPES 4

class RtspServer {

public:
  RtspServer();
  ~RtspServer();

  gboolean Start();

  void Stop();

  // Convert pipe to use with Gst RTSP Server
  gboolean ConnectPipe(
    GstElement* main_pipe,
    GstElement* src_end_point,
    GstElement* rtsp_pipe,
    GstElement* dst_start_point
  );

private:
  GstRTSPServer *gst_rtsp_server;
  unsigned int pipe_count;
  GstRTSPMountPoints *mounts[MAX_RTSP_PIPES];
  GstRTSPMediaFactory *factory[MAX_RTSP_PIPES];
  GstElement *intersink[MAX_RTSP_PIPES], *intersrc[MAX_RTSP_PIPES];



// Override default rtsp gst_rtsp_server mediafactory implementation
// -----------------------------------------------------------------

public:
  static GstElement * ImportPipeline (GstRTSPMediaFactory * factory, const GstRTSPUrl * url);
  static GstElement * CreateMediaPipe(GstRTSPMediaFactory *factory, GstRTSPMedia *media);
  static GstElement* rtsp_pipes[MAX_RTSP_PIPES];

private:
  // this timeout is periodically run to clean up the expired rtsp sessions from the pool.
  static gboolean SessionPoolTimeout(GstRTSPServer *server);
  static void StateChange(GstRTSPMedia *gstrtspmedia, gint arg1, gpointer user_data);
};

#endif //GST_RTSP_APP_SERVER_H
