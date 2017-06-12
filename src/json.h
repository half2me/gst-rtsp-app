#pragma once

#include "rapidjson/document.h"
#include "topology.h"

#define JSON_TAG_CAPS "caps"
#define JSON_TAG_PIPES "pipes"
#define JSON_TAG_RTSP "rtsp"
#define JSON_TAG_CONNECTIONS "connections"
#define JSON_TAG_LINKS "links"

class Json {
 public:

  Json(const char* json_file);
  ~Json();

  void CreateTopology(Topology* topology);

  void GetCaps(Topology *topology);
  void GetPipelineStructure(Topology *topology);
  void GetRtspPipes(Topology *topology);
  void GetInterConnections(Topology *topology);
  void GetConnections(Topology *topology);

 private:

  rapidjson::Document json_src;
};

// Exceptions
#include "exception.h"
#include "rapidjson/error/error.h" // rapidjson::ParseResult

struct JsonParseException : GcfException, rapidjson::ParseResult {
  JsonParseException(rapidjson::ParseErrorCode code, const char *msg, size_t offset);
};

struct JsonInvalidTypeException : GcfException {
  JsonInvalidTypeException(const std::string& message = "Error in json format!");
};
