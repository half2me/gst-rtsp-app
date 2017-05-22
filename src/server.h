//
// Created by pszekeres on 2017.05.18..
//

#ifndef GST_RTSP_APP_SERVER_H
#define GST_RTSP_APP_SERVER_H


#define MAX_RTSP_PIPES 8

class RtspServer {
public:

  RtspServer();
  ~RtspServer();

  gboolean Start();

  void Stop();

  gboolean ConnectPipe(
    GstElement* main_pipe,
    GstElement* src_end_point,
    GstElement* rtsp_pipe,
    GstElement* dst_start_point
  );
  

  // Override default rtsp gst_rtsp_server mediafactory implementation
  static GstElement * ImportPipeline (GstRTSPMediaFactory * factory, const GstRTSPUrl * url);
  static GstElement * CreateMediaPipe(GstRTSPMediaFactory *factory, GstRTSPMedia *media);

  static GstElement* rtsp_pipes[MAX_RTSP_PIPES];

private:

  // this timeout is periodically run to clean up the expired rtsp sessions from the pool.
  static gboolean SessionPoolTimeout(GstRTSPServer *server);

  static gpointer ThreadLoopFunc(gpointer data);

  GstRTSPServer *gst_rtsp_server;
  GThread *gst_rtsp_thread;
  unsigned int pipe_count;
  GstRTSPMountPoints *mounts[MAX_RTSP_PIPES];
  GstRTSPMediaFactory *factory[MAX_RTSP_PIPES];
  GstElement *intersink[MAX_RTSP_PIPES], *intersrc[MAX_RTSP_PIPES];

};

#endif //GST_RTSP_APP_SERVER_H
