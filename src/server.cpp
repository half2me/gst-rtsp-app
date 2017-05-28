#include <gst/rtsp-server/rtsp-media-factory.h>
#include <stdlib.h>
#include <cstdio>

#include "server.h"

#define GST_CAT_DEFAULT log_app_rtsp
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define APP_TYPE_RTSP_MEDIA_FACTORY (app_rtsp_media_factory_get_type ())
GType app_rtsp_media_factory_get_type(void);

struct AppRTSPMediaFactoryClass {
  GstRTSPMediaFactoryClass parent;
};

struct AppRTSPMediaFactory {
  GstRTSPMediaFactory parent;
};

std::map<std::string, GstElement *> RtspServer::rtsp_pipes = std::map<std::string, GstElement *>();

RtspServer::RtspServer() {

  GST_DEBUG_CATEGORY_INIT (log_app_rtsp, "GST_APP_RTSP",
                           GST_DEBUG_FG_CYAN, "RTSP Server");

  gst_rtsp_server = gst_rtsp_server_new();
  gst_rtsp_server_set_service(gst_rtsp_server, "8554");
  gst_rtsp_server_source = 0;

  // add a timeout for the session cleanup
  g_timeout_add_seconds(2, (GSourceFunc) SessionPoolTimeout, gst_rtsp_server);
}

RtspServer::~RtspServer() {
  GST_INFO("Stop RTSP Server");
  g_source_remove(gst_rtsp_server_source);
  g_object_unref(gst_rtsp_server);
}

gboolean
RtspServer::Start() {

  GST_INFO("RTSP Server init...");

  gst_rtsp_server_source = gst_rtsp_server_attach(gst_rtsp_server, NULL);
  if (gst_rtsp_server_source == 0) {
    GST_ERROR("Failed to attach the server!");
    return FALSE;
  }

  return TRUE;
}

gboolean RtspServer::RegisterRtspPipes(const std::map<std::string, GstElement *> &pipes) {
  for (auto iter = pipes.begin(); iter != pipes.end(); ++iter) {
    auto pipe_name = iter->first;

    GstRTSPMediaFactory *factory = (GstRTSPMediaFactory *) g_object_new(APP_TYPE_RTSP_MEDIA_FACTORY, NULL);
    GstRTSPMountPoints *mount = gst_rtsp_server_get_mount_points(gst_rtsp_server);

    // use the launch string for the mediafactory to identify the pipe
    gst_rtsp_media_factory_set_launch(factory, pipe_name.c_str());

    // Set this shitty pipeline to shared between all the fucked up clients so they won't mess up the driver's state
    gst_rtsp_media_factory_set_shared(factory, TRUE);

    // attach the test factory to the /testN url
    gst_rtsp_mount_points_add_factory(mount, std::string('/' + pipe_name).c_str(), factory);

    // don't need the ref to the mapper anymore
    g_object_unref(mount);

    rtsp_pipes[pipe_name] = iter->second;
  }

  return TRUE;
}

GstElement *
RtspServer::ImportPipeline(GstRTSPMediaFactory *factory, const GstRTSPUrl *url) {

  auto pipe_name = std::string(gst_rtsp_media_factory_get_launch(factory));
  auto url_path = std::string("rtsp://") + url->host + ":" + std::to_string(url->port) + url->abspath;

  GST_INFO("Created media \"%s\" from pipe \"%s\".",
          url_path.c_str(),
          pipe_name.c_str());

  return rtsp_pipes[pipe_name];
}

GstElement *
RtspServer::CreateMediaPipe(GstRTSPMediaFactory *factory, GstRTSPMedia *media) {

  auto pipe_name = gst_rtsp_media_factory_get_launch(factory);
  if (!pipe_name) {
    GST_ERROR("Error creating media pipe for \"%s\"!",
               gst_element_get_name(gst_rtsp_media_get_element(media)));

    return NULL;
  }

  auto ext_pipename = "e_" + std::string(pipe_name);
  GstElement *pipeline = gst_pipeline_new(ext_pipename.c_str());
  gst_rtsp_media_take_pipeline(media, GST_PIPELINE_CAST (pipeline));

  // This way the media will not be reinitialized - our created pipe is not lost
  gst_rtsp_media_set_reusable(media, TRUE);

  // Watch state changes
  g_signal_connect (media, "new-state", G_CALLBACK(StateChange), NULL);

  return pipeline;
}

gboolean
RtspServer::SessionPoolTimeout(GstRTSPServer *server) {
  GstRTSPSessionPool *pool = gst_rtsp_server_get_session_pool(server);
  gst_rtsp_session_pool_cleanup(pool);
  g_object_unref(pool);

  return TRUE;
}

void
RtspServer::StateChange(GstRTSPMedia *media, gint arg1, gpointer user_data) {
  GstElement *element = gst_rtsp_media_get_element(media);
  GstState state;
  gst_element_get_state(element, &state, NULL, 0);
  GST_INFO("%s: %s\n", gst_element_get_name(element), gst_element_state_get_name(state));
}

G_DEFINE_TYPE (AppRTSPMediaFactory, app_rtsp_media_factory,
               GST_TYPE_RTSP_MEDIA_FACTORY);

static void
app_rtsp_media_factory_class_init(AppRTSPMediaFactoryClass *test_klass) {
  GstRTSPMediaFactoryClass *mf_klass =
      (GstRTSPMediaFactoryClass *) (test_klass);
  mf_klass->create_element = RtspServer::ImportPipeline;
  mf_klass->create_pipeline = RtspServer::CreateMediaPipe;

  GST_DEBUG("Custom MediaFactory initialized.\n");
}

static void
app_rtsp_media_factory_init(AppRTSPMediaFactory *factory) {
}
