#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <stdio.h>
#include <stdlib.h>

#include "rtsp-server.h"

#define MAX_RTSP_PIPES 8
GstElement* rtsp_pipes[MAX_RTSP_PIPES];
GThread *rtsp_thread;

#define TEST_TYPE_RTSP_MEDIA_FACTORY      (test_rtsp_media_factory_get_type ())

GType test_rtsp_media_factory_get_type (void);

static GstElement *create_element (GstRTSPMediaFactory * factory, const GstRTSPUrl * url);

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

GstRTSPMediaFactory *
test_rtsp_media_factory_new (void)
{
  GstRTSPMediaFactory *result;

  result = g_object_new (TEST_TYPE_RTSP_MEDIA_FACTORY, NULL);

  return result;
}

static GstElement *
create_element (GstRTSPMediaFactory * factory, const GstRTSPUrl * url) {

  char *ptr;
  long index = strtol(gst_rtsp_media_factory_get_launch(factory), &ptr, 2);

  return rtsp_pipes[index];
}

gpointer rtsp_thread_loop(gpointer data) {
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(loop);
  g_print("RTSP server stopped.\n");
  g_main_loop_unref(loop);
}

// this timeout is periodically run to clean up the expired rtsp sessions from the pool.
static gboolean
timeout (GstRTSPServer * server)
{
  GstRTSPSessionPool *pool = gst_rtsp_server_get_session_pool (server);
  gst_rtsp_session_pool_cleanup (pool);
  g_object_unref (pool);

  return TRUE;
}

gboolean rtsp_server_init() {

  GstRTSPServer *server;
  GstRTSPMountPoints *mounts[MAX_RTSP_PIPES];
  GstRTSPMediaFactory *factory[MAX_RTSP_PIPES];

  // create a tweaked server instance
  server = gst_rtsp_server_new ();
  gst_rtsp_server_set_service(server, "8554");
  mounts[0] = gst_rtsp_server_get_mount_points (server);
  factory[0] = test_rtsp_media_factory_new ();

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
  g_timeout_add_seconds (1, (GSourceFunc) timeout, server);

  // Start thread for rtsp server
  rtsp_thread = g_thread_new("rtsp-thread", rtsp_thread_loop, NULL);
}

void rtsp_server_deinit()
{
  // stop rtsp
  g_thread_join(rtsp_thread);

  for (int for_i=0; for_i<MAX_RTSP_PIPES; for_i++) {
    if (rtsp_pipes[for_i]) {
      gst_object_unref (rtsp_pipes[for_i]);
    }
  }

}

GstElement* use_rtsp_pipeline(int i){

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
  mf_klass->create_element = create_element;
}

static void
test_rtsp_media_factory_init (TestRTSPMediaFactory * factory)
{
  g_print("Custom lofaszmediafactory :D \n");
}