//
// Created by pszekeres on 2017.05.23..
//
#ifndef GST_RTSP_APP_TOPOLOGY_H
#define GST_RTSP_APP_TOPOLOGY_H

#include "gst/gst.h"
#include <string>
#include <map>
#include <vector>

class Topology {
public:
  Topology();
  ~Topology();

  GstElement* GetPipe(std::string name);
  void InitPipe(std::string name);

  static gboolean LinkToTee(GstElement* tee, GstElement* element);
  // Convert pipe to use with Gst RTSP Server

  gboolean ConnectRtspPipe(
    GstElement *source_pipe,
    GstElement *source_end_point,
    GstElement *rtsp_pipe,
    GstElement *rtsp_start_point
  );

private:
  GMutex pipe_mutex;
  std::map<std::string, GstElement*> pipes;
  std::map<std::string, GstElement*> elements;
  std::vector<GstElement*> rtsp_pipes;
};

#endif //GST_RTSP_APP_TOPOLOGY_H