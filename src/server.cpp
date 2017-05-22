//
// Created by pszekeres on 2017.05.18..
//

#include <gst/rtsp-server/rtsp-media-factory.h>
#include <stdlib.h>
#include <cstdio>
#include "server.h"

#define TEST_TYPE_RTSP_MEDIA_FACTORY      (test_rtsp_media_factory_get_type ())
GType test_rtsp_media_factory_get_type(void);

typedef struct TestRTSPMediaFactoryClass TestRTSPMediaFactoryClass;
typedef struct TestRTSPMediaFactory TestRTSPMediaFactory;

struct TestRTSPMediaFactoryClass {
  GstRTSPMediaFactoryClass parent;
};

struct TestRTSPMediaFactory {
  GstRTSPMediaFactory parent;
};

GstElement* RtspServer::rtsp_pipes[MAX_RTSP_PIPES] = {};

RtspServer::RtspServer() {
  // TODO NULL init each f*ckin element
  gst_rtsp_thread = NULL;
  pipe_count = 0;

  // create a tweaked gst_rtsp_server instance
  gst_rtsp_server = gst_rtsp_server_new ();
  gst_rtsp_server_set_service(gst_rtsp_server, "8554");
}

RtspServer::~RtspServer() {
}

gpointer
RtspServer::ThreadLoopFunc(gpointer data) {
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  g_print("Server thread started.\n");
  g_main_loop_run(loop);
  g_print("Server thread stopped.\n");
  g_main_loop_unref(loop);
}

gboolean
RtspServer::Init() {

  g_print ("RTSP Server init...\n");

  /* attach the gst_rtsp_server to the default maincontext */
  if (gst_rtsp_server_attach (gst_rtsp_server, NULL) == 0) {
    g_print ("Failed to attach the server\n");
    return FALSE;
  }

  // add a timeout for the session cleanup
  g_timeout_add_seconds(2, (GSourceFunc) SessionPoolTimeout, gst_rtsp_server);

  // Start thread for rtsp gst_rtsp_server
  gst_rtsp_thread = g_thread_new("rtsp-thread", ThreadLoopFunc, NULL);

  return TRUE;
}

void
RtspServer::UnInit()
{
  g_print("Stopping RTSP Server\n");
  g_thread_join(gst_rtsp_thread);

  for (int for_i=0; for_i<MAX_RTSP_PIPES; for_i++) {
    if (rtsp_pipes[for_i]) {
      gst_object_unref (rtsp_pipes[for_i]);
    }
  }

}

gboolean
RtspServer::ConnectPipe(
  GstElement* pipe_to_use,
  GstElement* pipe_to_connect,
  GstElement* src_end_point,
  GstElement* dst_start_point
)
{
  if (!GST_IS_PIPELINE(pipe_to_use)) {
    return FALSE;
  }
  if (pipe_count >= MAX_RTSP_PIPES) {
    return FALSE;
  }

  rtsp_pipes[pipe_count] = pipe_to_use;

  factory[pipe_count] = (GstRTSPMediaFactory*)g_object_new (TEST_TYPE_RTSP_MEDIA_FACTORY, NULL);
  mounts[pipe_count] = gst_rtsp_server_get_mount_points (gst_rtsp_server);

  // use the launch string to identify pipe index
  char buf[16];
  sprintf(buf, "%d", pipe_count);
  gst_rtsp_media_factory_set_launch (factory[pipe_count], buf);

  // Set this shitty pipeline to shared between all the fucked up clients so they won't mess up the driver's state
  gst_rtsp_media_factory_set_shared (factory[pipe_count], TRUE);

  // attach the test factory to the /testN url
  sprintf(buf, "/test%d", pipe_count);
  gst_rtsp_mount_points_add_factory (mounts[pipe_count], buf, factory[pipe_count]);

  // don't need the ref to the mapper anymore
  g_object_unref (mounts[pipe_count]);

  // create gateway pairs
  sprintf(buf, "intersink%d", pipe_count);
  intersink[pipe_count] = gst_element_factory_make ("intervideosink", buf);
  sprintf(buf, "intersrc%d", pipe_count);
  intersrc[pipe_count] = gst_element_factory_make ("intervideosrc", buf);
  if (!intersink[pipe_count] || !intersrc[pipe_count]) {
    g_printerr("Error creating intervideo pair %d.\n", pipe_count);
    return FALSE;
  }

  sprintf(buf, "gateway%d", pipe_count);
  g_object_set(intersink[pipe_count], "channel", buf, NULL);
  g_object_set(intersrc[pipe_count], "channel", buf, NULL);

  gst_bin_add ( GST_BIN (pipe_to_connect), intersink[pipe_count]);
  gst_bin_add ( GST_BIN (pipe_to_use), intersrc[pipe_count]);

  // link the portals
  if (!gst_element_link(intersrc[pipe_count], dst_start_point))
  {
    return FALSE;
  }
  if (!gst_element_link(src_end_point, intersink[pipe_count]))
  {
    return FALSE;
  }

  pipe_count++;

  return TRUE;
}

GstElement *
RtspServer::ImportPipeline(GstRTSPMediaFactory *factory, const GstRTSPUrl *url) {
  char *ptr;
  long index = strtol(gst_rtsp_media_factory_get_launch(factory), &ptr, 2);

  g_print("Media #%ld is initialized to use pipe%ld.\n", index, index);
  return rtsp_pipes[index];
}


GstElement *
RtspServer::CreateMediaPipe(GstRTSPMediaFactory *factory, GstRTSPMedia *media) {

  if (!gst_rtsp_media_factory_get_launch(factory)) {

  }

  GstElement *pipeline;
  char *ptr;
  long index = strtol(gst_rtsp_media_factory_get_launch(factory), &ptr, 2);
  char buf[16];
  sprintf(buf, "ext-pipe%ld", index);

  pipeline = gst_pipeline_new (buf);
  gst_rtsp_media_take_pipeline (media, GST_PIPELINE_CAST (pipeline));

  // This way the media will not be reinitialized - our created pipe is not lost
  gst_rtsp_media_set_reusable(media, TRUE);
  g_print("RTSP Media #%ld is reusable.\n", index);

  return pipeline;
}


gboolean
RtspServer::SessionPoolTimeout(GstRTSPServer *server)
{
  GstRTSPSessionPool *pool = gst_rtsp_server_get_session_pool (server);
  gst_rtsp_session_pool_cleanup (pool);
  g_object_unref (pool);

  return TRUE;
}

G_DEFINE_TYPE (TestRTSPMediaFactory, test_rtsp_media_factory,
               GST_TYPE_RTSP_MEDIA_FACTORY);

static void
test_rtsp_media_factory_class_init (TestRTSPMediaFactoryClass * test_klass)
{
  GstRTSPMediaFactoryClass *mf_klass =
    (GstRTSPMediaFactoryClass *) (test_klass);
  mf_klass->create_element = RtspServer::ImportPipeline;
  mf_klass->create_pipeline = RtspServer::CreateMediaPipe;

  g_print("Custom MediaFactory initialized.\n");
}

static void
test_rtsp_media_factory_init (TestRTSPMediaFactory * factory)
{
}
