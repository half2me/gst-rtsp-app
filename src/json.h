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

  void SetCaps(Topology *topology);
  void SetPipelineStructure(Topology *topology);
  void SetRtspPipes(Topology *topology);
  void SetInterConnections(Topology *topology);
  void SetConnections(Topology *topology);

 private:

  rapidjson::Document json_src;
};

// Exceptions
#include <stdexcept>               // std::runtime_error
#include "rapidjson/error/error.h" // rapidjson::ParseResult

struct JsonParseException : std::runtime_error, rapidjson::ParseResult {
  JsonParseException(rapidjson::ParseErrorCode code, const char *msg, size_t offset);
};

struct JsonInvalidTypeException : std::runtime_error {
  JsonInvalidTypeException(const std::string& message = "Error in json format!");
};
