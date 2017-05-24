//
// Created by pszekeres on 2017.05.23..
//
#ifndef GST_RTSP_APP_TOPOLOGY_H
#define GST_RTSP_APP_TOPOLOGY_H

#include "gst/gst.h"
#include <string>
#include <map>
#include <vector>
#include <tuple>
#include <utility>

using namespace std;

class Topology {
public:

  Topology();
  ~Topology();

  bool LoadJson(string json);

  GstElement* GetPipe(string name);
  map<string, GstElement*>& GetPipes();

  GstElement* GetRtspPipe(string name);
  map<string, GstElement*>& GetRtspPipes();

  GstElement* GetElement(string name);
  map<string, GstElement*>& GetElements();

private:

  // Connects an element to a tee, creating a new branch on it
  static gboolean LinkToTee(GstElement* tee, GstElement* element);

  // Converts a pipe to use with the Gst RTSP Server
  gboolean ConnectRtspPipe(
    GstElement *source_pipe,
    GstElement *source_end_point,
    GstElement *rtsp_pipe,
    GstElement *rtsp_start_point
  );

  void InitPipe(std::string name);

  map<string, vector<string>> raw_pipes;
  map<string, vector<string>> raw_rtsp_pipes;
  vector<pair<string, string>> raw_links;
  vector<tuple<string, string, string, string>> raw_rtsp_connections;

  vector<pair<string, string>> raw_properties;

  map<string, GstElement*> pipes;
  map<string, GstElement*> rtsp_pipes;
  map<string, GstElement*> elements;

};

#endif //GST_RTSP_APP_TOPOLOGY_H