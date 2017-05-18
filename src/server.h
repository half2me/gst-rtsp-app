//
// Created by pszekeres on 2017.05.18..
//

#ifndef GST_RTSP_APP_SERVER_H
#define GST_RTSP_APP_SERVER_H


#define MAX_RTSP_PIPES 8
#define TEST_TYPE_RTSP_MEDIA_FACTORY      (test_rtsp_media_factory_get_type ())

GType test_rtsp_media_factory_get_type (void);

class RtspServer {
public:

  RtspServer();
  ~RtspServer();

  gboolean Init();

  void UnInit();

  GstElement *UsePipe(int i);

  static GstElement * ImportPipeline (GstRTSPMediaFactory * factory, const GstRTSPUrl * url);

  static GstElement* rtsp_pipes[MAX_RTSP_PIPES];

private:

  GstRTSPMediaFactory* CreateMediaFactory();

  // this timeout is periodically run to clean up the expired rtsp sessions from the pool.
  static gboolean SessionPoolTimeout(GstRTSPServer *server);

  static gpointer ThreadLoopFunc(gpointer data);

  GThread *rtsp_thread;

};

#endif //GST_RTSP_APP_SERVER_H
