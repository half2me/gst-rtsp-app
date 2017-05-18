//
// Created by pszekeres on 2017.05.18..
//

#include <gst/rtsp-server/rtsp-media-factory.h>
#include <stdlib.h>
#include <cstdio>
#include "server.h"

typedef struct TestRTSPMediaFactoryClass TestRTSPMediaFactoryClass;
typedef struct TestRTSPMediaFactory TestRTSPMediaFactory;

struct TestRTSPMediaFactoryClass
{
    GstRTSPMediaFactoryClass parent;
};

struct TestRTSPMediaFactory
{
    GstRTSPMediaFactory parent;
};

GstElement* RtspServer::rtsp_pipes[MAX_RTSP_PIPES] = {};

RtspServer::RtspServer() {
  RtspServer::rtsp_thread = NULL;
}

RtspServer::~RtspServer() {
}

GstElement *
RtspServer::ImportPipeline(GstRTSPMediaFactory *factory, const GstRTSPUrl *url) {

  char *ptr;
  long index = strtol(gst_rtsp_media_factory_get_launch(factory), &ptr, 2);

  return rtsp_pipes[index];
}

gpointer
RtspServer::ThreadLoopFunc(gpointer data) {
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(loop);
  g_print("RTSP server stopped.\n");
  g_main_loop_unref(loop);
}

gboolean
RtspServer::SessionPoolTimeout(GstRTSPServer *server)
{
  GstRTSPSessionPool *pool = gst_rtsp_server_get_session_pool (server);
  gst_rtsp_session_pool_cleanup (pool);
  g_object_unref (pool);

  return TRUE;
}

GstRTSPMediaFactory *
RtspServer::CreateMediaFactory (void)
{
  return
    (GstRTSPMediaFactory*)
      g_object_new (TEST_TYPE_RTSP_MEDIA_FACTORY, NULL);
}

gboolean
RtspServer::Init() {

  GstRTSPServer *server;
  GstRTSPMountPoints *mounts[MAX_RTSP_PIPES];
  GstRTSPMediaFactory *factory[MAX_RTSP_PIPES];

  // create a tweaked server instance
  server = gst_rtsp_server_new ();
  gst_rtsp_server_set_service(server, "8554");
  mounts[0] = gst_rtsp_server_get_mount_points (server);
  factory[0] = CreateMediaFactory();

  // use the launch string to identify pipe index
  gst_rtsp_media_factory_set_launch (factory[0], "0");

  // Set this shitty pipeline to shared between all the fucked up clients so they won't mess up the driver's state
  gst_rtsp_media_factory_set_shared (factory[0], TRUE);

  /* attach the test factory to the /test url */
  gst_rtsp_mount_points_add_factory (mounts[0], "/test", factory[0]);

  /* don't need the ref to the mapper anymore */
  g_object_unref (mounts[0]);

  /* attach the server to the default maincontext */
  if (gst_rtsp_server_attach (server, NULL) == 0) {
    g_print ("Failed to attach the server\n");
    return FALSE;
  }
  else {
    g_print ("Stream ready at rtsp://127.0.0.1:8554/test\n");
  }

  /* add a timeout for the session cleanup */
  g_timeout_add_seconds(1, (GSourceFunc) SessionPoolTimeout, server);

  // Start thread for rtsp server
  rtsp_thread = g_thread_new("rtsp-thread", ThreadLoopFunc, NULL);

  return TRUE;
}

void
RtspServer::UnInit()
{
  g_print("Stopping RTSP Server\n");
  g_thread_join(rtsp_thread);

  for (int for_i=0; for_i<MAX_RTSP_PIPES; for_i++) {
    if (rtsp_pipes[for_i]) {
      gst_object_unref (rtsp_pipes[for_i]);
    }
  }

}

GstElement*
RtspServer::UsePipe(int i){

  if (i<0 || i>=MAX_RTSP_PIPES) return NULL;

  char buf[16];
  sprintf(buf, "rtsppipe%d", i);
  rtsp_pipes[i] = gst_pipeline_new(buf);

  if (!rtsp_pipes[i]) return NULL;

  return rtsp_pipes[i];
}


G_DEFINE_TYPE (TestRTSPMediaFactory, test_rtsp_media_factory,
  GST_TYPE_RTSP_MEDIA_FACTORY);

static void
test_rtsp_media_factory_class_init (TestRTSPMediaFactoryClass * test_klass)
{
  g_print("Custom lofaszmediafactoryClass \n");

  GstRTSPMediaFactoryClass *mf_klass =
    (GstRTSPMediaFactoryClass *) (test_klass);
  mf_klass->create_element = RtspServer::ImportPipeline;
}

static void
test_rtsp_media_factory_init (TestRTSPMediaFactory * factory)
{
  g_print("Custom lofaszmediafactory :D \n");
}
