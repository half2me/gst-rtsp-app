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

std::map<std::string, GstElement*> RtspServer::rtsp_pipes = std::map<std::string, GstElement*>();

RtspServer::RtspServer()
{
  gst_rtsp_server = gst_rtsp_server_new();
  gst_rtsp_server_set_service(gst_rtsp_server, "8554");
}

RtspServer::~RtspServer() {
  // TODO Guess what...
}

gboolean
RtspServer::Start() {

  g_print ("RTSP Server init...\n");

  // add a timeout for the session cleanup
  g_timeout_add_seconds(2, (GSourceFunc) SessionPoolTimeout, gst_rtsp_server);

  if (gst_rtsp_server_attach (gst_rtsp_server, NULL) == 0) {
    g_print ("Failed to attach the server\n");
    return FALSE;
  }

  return TRUE;
}

void
RtspServer::Stop()
{
  g_print("Stopping RTSP Server\n");

// TODO Probably RTSP Pipelines are destroyed by gst_server, but who knows...
//  for (int for_i=0; for_i<MAX_RTSP_PIPES; for_i++) {
//    if (rtsp_pipes[for_i]) {
//      gst_object_unref (rtsp_pipes[for_i]);
//    }
//  }
}

gboolean RtspServer::RegisterRtspPipes(const std::map<std::string, GstElement*>& pipes)
{
  for(auto iter = pipes.begin(); iter != pipes.end(); ++iter)
  {
    GstRTSPMediaFactory *factory = (GstRTSPMediaFactory *) g_object_new(TEST_TYPE_RTSP_MEDIA_FACTORY, NULL);
    GstRTSPMountPoints *mount = gst_rtsp_server_get_mount_points(gst_rtsp_server);

    // use the launch string for the mediafactory to identify pipe index
    gst_rtsp_media_factory_set_launch(factory, iter->first.c_str());

    // Set this shitty pipeline to shared between all the fucked up clients so they won't mess up the driver's state
    gst_rtsp_media_factory_set_shared(factory, TRUE);

    // attach the test factory to the /testN url
    gst_rtsp_mount_points_add_factory(mount, iter->first.c_str(), factory);

    // don't need the ref to the mapper anymore
    g_object_unref(mount);

    rtsp_pipes[iter->first] = iter->second;
  }

  return TRUE;
}

GstElement *
RtspServer::ImportPipeline(GstRTSPMediaFactory *factory, const GstRTSPUrl *url) {

  auto pipe_name = std::string(gst_rtsp_media_factory_get_launch(factory));
  auto url_path = std::string(url->abspath);

  g_print("Media \"%s\" is initialized to use pipe \"%s\".\n",
          url_path.c_str(),
          pipe_name.c_str());

  return rtsp_pipes[pipe_name];
}


GstElement *
RtspServer::CreateMediaPipe(GstRTSPMediaFactory *factory, GstRTSPMedia *media) {

  if (!gst_rtsp_media_factory_get_launch(factory)) {
    g_critical("Error creating media pipe!\n");
    return NULL;
  }

  char *ptr;
  long index = strtol(gst_rtsp_media_factory_get_launch(factory), &ptr, 2);
  char buf[16];
  sprintf(buf, "ext-pipe%ld", index);

  GstElement *pipeline = gst_pipeline_new (buf);;
  gst_rtsp_media_take_pipeline (media, GST_PIPELINE_CAST (pipeline));

  // This way the media will not be reinitialized - our created pipe is not lost
  gst_rtsp_media_set_reusable(media, TRUE);
  g_print("RTSP Media #%ld is reusable.\n", index);

  g_signal_connect (media, "new-state", G_CALLBACK(StateChange), NULL);

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

void
RtspServer::StateChange(GstRTSPMedia *media, gint arg1, gpointer user_data) {
  GstElement *element = gst_rtsp_media_get_element(media);
  GstState state;
  gst_element_get_state(element, &state, NULL, 0);
  g_print("%s: %s\n", gst_element_get_name(element), gst_element_state_get_name(state));
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
