#pragma once

#include "gst/gst.h"

#include <string>
#include <map>
#include <vector>

#define JSON_TAG_CAPS "caps"
#define JSON_TAG_LINKS "links"
#define JSON_TAG_PIPES "pipes"
#define JSON_TAG_RTSP "rtsp"
#define JSON_TAG_CONNECTIONS "connections"

using namespace std;

class Topology {
 public:

  Topology();
  ~Topology();


  // Caps
  bool HasCap(const string &cap_name);
  GstCaps* GetCaps(const string& name);
  void CreateCap(const char *cap_name, const char *cap_def);
  void AssignCap(const char *filter_name, const char *cap_name);

  // Elements
  // --------
  bool HasElement(const string &elemname);
  GstElement* GetElement(const string& name);
  void SetElement(const string& name, GstElement *element);
  const map<string, GstElement*>& GetElements();
  void CreateElement(const char* elem_name, const char* elem_type);
  void CreateElement(const string& elem_name, const string& elem_type);
  void SetProperty(const char* elem_name, const char* prop_name, const char* prop_value);
  void AddElementToBin (const string& elem_name, const string& pipe_name);
  void ConnectElements(const string& src_name, const string& dst_name);

  // Connects an element to a tee, creating a new branch on it
  static gboolean LinkToTee(GstElement* tee, GstElement* element);

  // Pipes
  // -----
  bool HasPipe(const string &elem_name);
  GstElement* GetPipe(const string& name);
  void SetPipe(const string& name, GstElement* element);
  const map<string, GstElement*>& GetPipes();
  void CreatePipeline(const char* elem_name);

  // Converts a pipe through intervideo tunnels
  void ConnectPipe(const char *pipe,
                   const char *start_point,
                   const char *source_pipe,
                   const char *source_end_point);

  // Rtsp pipes are listed between pipes too, but
  // they are currently handled by the RTSP module
  bool HasRtspPipe(const string &elem_name);
  GstElement* GetRtspPipe(const string& name);
  void SetRtspPipe(const string& name, GstElement* element);
  const map<string, GstElement*>& GetRtspPipes();

  // TEMP
  map<string, GstElement*> intersinks;
  map<string, GstElement*> queues;

 private:
  map<string, GstElement*> elements;
  map<string, GstElement*> pipes;
  map<string, GstElement*> rtsp_pipes;
  map<string, GstCaps*> caps;

};

#include "exception.h"

// Exceptions
struct TopologyInvalidAttributeException : GcfException {
  TopologyInvalidAttributeException(const std::string& message = "Error in topology attribute!");
};

struct TopologyGstreamerException : GcfException {
  TopologyGstreamerException(const std::string& message = "Gst can't make that.");
};
