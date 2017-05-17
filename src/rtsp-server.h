//
// Created by pszekeres on 2017.05.17..
//
#ifndef GST_RTSP_APP_RTSP_SERVER_H
#define GST_RTSP_APP_RTSP_SERVER_H


G_BEGIN_DECLS

gboolean rtsp_server_init();
void rtsp_server_deinit();

GstElement* use_rtsp_pipeline(int i);

G_END_DECLS

#endif //GST_RTSP_APP_RTSP_SERVER_H
