//
// Created by pszekeres on 2017.05.23..
//
#ifndef GST_RTSP_APP_TOPOLOGY_H
#define GST_RTSP_APP_TOPOLOGY_H
#define MAX_PIPES 8

#include "gst/gst.h"
#include <string>
#include <map>

class Topology {
public:
  Topology();
  ~Topology();

  GstElement* GetPipe(std::string name);
  void InitPipe(std::string name);

  static gboolean LinkToTee(GstElement* tee, GstElement* element);

private:
  GMutex pipe_mutex;
  std::map<std::string, GstElement*> pipes;
  std::map<std::string, GstElement*> elements;
};

#endif //GST_RTSP_APP_TOPOLOGY_H