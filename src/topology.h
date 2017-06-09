#pragma once

#define JSON_TAG_CAPS "caps"
#define JSON_TAG_LINKS "links"
#define JSON_TAG_PIPES "pipes"
#define JSON_TAG_RTSP "rtsp"
#define JSON_TAG_CONNECTIONS "connections"

#include "gst/gst.h"
#include <string>
#include <map>
#include <vector>

using namespace std;

class Topology {
public:

  Topology();
  ~Topology();


  // Element access functions
  // ------------------------

  // Caps
  bool HasCap(const string &cap_name);
  GstCaps* GetCaps(const string& name);

  // Acces functions for elements
  bool HasElement(const string &elemname);
  GstElement* GetElement(const string& name);
  bool SetElement(const string& name, GstElement *element);
  const map<string, GstElement*>& GetElements();

  // Pipes
  bool HasPipe(const string &elem_name);
  GstElement* GetPipe(const string& name);
  bool SetPipe(const string& name, GstElement* element);
  const map<string, GstElement*>& GetPipes();

  // Rtsp pipes are listed between pipes too, but
  // they are currently handled by the RTSP module
  bool HasRtspPipe(const string &elem_name);
  GstElement* GetRtspPipe(const string& name);
  bool SetRtspPipe(const string& name, GstElement* element);
  const map<string, GstElement*>& GetRtspPipes();


  // Element manipulation functions
  // ------------------------------

  // Handles caps
  bool CreateCap(const char *cap_name, const char *cap_def);
  bool AssignCap(const char *filter_name, const char *cap_name);

  // Elements
  bool CreateElement(const char* elem_name, const char* elem_type);
  bool CreateElement(const string& elem_name, const string& elem_type);
  bool ConnectElements(const string& src_name, const string& dst_name);
  bool SetProperty(const char* elem_name, const char* prop_name, const char* prop_value);

  // Pipes
  bool CreatePipeline(const char* elem_name);
  bool AddElementToBin (const string& elem_name, const string& pipe_name);

  // Connects an element to a tee, creating a new branch on it
  static gboolean LinkToTee(GstElement* tee, GstElement* element);

  // Converts a pipe to use with the Gst RTSP Server
  bool ConnectPipe(const char *pipe,
                   const char *start_point,
                   const char *source_pipe,
                   const char *source_end_point);

  map<string, GstElement*> intersinks;
  map<string, GstElement*> queues;

private:
  map<string, GstElement*> elements;
  map<string, GstElement*> pipes;
  map<string, GstElement*> rtsp_pipes;
  map<string, GstCaps*> caps;

};
