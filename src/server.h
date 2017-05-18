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

  gboolean Init();

  void UnInit();

  GstElement *UsePipe(int i);

  static GstElement * ImportPipeline (GstRTSPMediaFactory * factory, const GstRTSPUrl * url);
  static GstElement * CreateMediaPipe(GstRTSPMediaFactory *factory, GstRTSPMedia *media);

  static GstElement* rtsp_pipes[MAX_RTSP_PIPES];

private:

  GstRTSPMediaFactory* CreateMediaFactory();

  // this timeout is periodically run to clean up the expired rtsp sessions from the pool.
  static gboolean SessionPoolTimeout(GstRTSPServer *server);

  static gpointer ThreadLoopFunc(gpointer data);

  GstRTSPMountPoints *mounts[MAX_RTSP_PIPES];
  GstRTSPMediaFactory *factory[MAX_RTSP_PIPES];
  GstRTSPServer *server;
  GThread *rtsp_thread;

};

#endif //GST_RTSP_APP_SERVER_H
