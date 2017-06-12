struct JsonParseException;
#define RAPIDJSON_PARSE_ERROR_NORETURN(parseErrorCode,offset) \
   throw JsonParseException(parseErrorCode, #parseErrorCode, offset)

#include <fstream>
#include "json.h"

GST_DEBUG_CATEGORY_STATIC (log_app_json);  // define debug category (statically)
#define GST_CAT_DEFAULT log_app_json       // set as default

Json::Json(const char* json_file) {

  // Set up local debug category
  GST_DEBUG_CATEGORY_INIT (
      GST_CAT_DEFAULT, "GCF_APP_JSON", GST_DEBUG_FG_MAGENTA, "Json utils"
  );

  // Get json string from file
  std::ifstream ifs(json_file);
  std::string content(
      (std::istreambuf_iterator<char>(ifs)),
      (std::istreambuf_iterator<char>()));

  // Parse the assigned JSON source
  json_src.Parse(content.c_str());
}

Json::~Json() {
}

// Load, create and store caps
void Json::GetCaps(Topology *topology) {

  if (json_src.HasMember(JSON_TAG_CAPS)) {
    GST_DEBUG("Reading caps from JSON...");

    const rapidjson::Value &json_caps_obj = json_src[JSON_TAG_CAPS];
    if (!json_caps_obj.IsObject()) {
      throw JsonInvalidTypeException("Object to store cap attributes is not a valid object!");
    }

    for (rapidjson::Value::ConstMemberIterator itr = json_caps_obj.MemberBegin();
         itr != json_caps_obj.MemberEnd(); ++itr) {

      if (!itr->name.IsString() || !itr->value.IsString()) {
        throw JsonInvalidTypeException("Invalid cap found!");
      }

      topology->CreateCap(itr->name.GetString(), itr->value.GetString());
    }
  } else {
    GST_DEBUG("No caps are defined.");
  }
}

// Read and save list of RTSP Pipes
void Json::GetRtspPipes(Topology *topology) {

  if (json_src.HasMember(JSON_TAG_RTSP)) {
    GST_DEBUG("Reading RTSP Pipes from JSON...");

    const rapidjson::Value &json_rtsp_arr = json_src[JSON_TAG_RTSP];
    if (!json_rtsp_arr.IsArray()) {
      throw JsonInvalidTypeException("Object to store RTSP attribute is not an array!");
    }

    for (rapidjson::Value::ConstValueIterator itr = json_rtsp_arr.Begin();
         itr != json_rtsp_arr.End(); ++itr) {

      if (!itr->IsString()) {
        throw JsonInvalidTypeException("RTSP Pipe name is not a string value!");
      }
      const char *pipe_name = itr->GetString();

      topology->SetRtspPipe(pipe_name, topology->GetPipe(pipe_name));

      GST_DEBUG("\"%s\" is marked as RTSP Pipe.", pipe_name);
    }
  } else {
    GST_DEBUG("No RTSP pipes are defined.");
  }
}

// Intervideo powered pipe connections
void Json::GetInterConnections(Topology *topology) {

  if (json_src.HasMember(JSON_TAG_CONNECTIONS)) {
    GST_DEBUG("Reading intervideo connections from JSON...");

    const rapidjson::Value &json_interr_links_obj = json_src[JSON_TAG_CONNECTIONS];
    if (!json_interr_links_obj.IsObject()) {
      throw JsonInvalidTypeException("Object to store intervideo connections is not a valid object!");
    }

    for (rapidjson::Value::ConstMemberIterator pipe_itr = json_interr_links_obj.MemberBegin();
         pipe_itr != json_interr_links_obj.MemberEnd(); ++pipe_itr) {

      if (!pipe_itr->name.IsString()) {
        throw JsonInvalidTypeException("Invalid source pipe specified for interconnections!");
      }
      const char *pipe_name = pipe_itr->name.GetString();

      // CValidate function parameters
      if (!pipe_itr->value.HasMember("first_elem") || !pipe_itr->value["first_elem"].IsString()) {
        throw JsonInvalidTypeException(
            std::string("Invalid first element specified for \"") + pipe_name + "\" in interconnections!");
      }
      if (!pipe_itr->value.HasMember("src_pipe") || !pipe_itr->value["src_pipe"].IsString()) {
        throw JsonInvalidTypeException(
            std::string("Invalid source pipe specified for \"") + pipe_name + "\" in interconnections!");
      }
      if (!pipe_itr->value.HasMember("src_last_elem") || !pipe_itr->value["src_last_elem"].IsString()) {
        throw JsonInvalidTypeException(
            std::string("Invalid element is specified for \"") + pipe_name + "\" in interconnections!");
      }

      // Connect the pipes
      topology->ConnectPipe(
          pipe_name,
          pipe_itr->value["first_elem"].GetString(),
          pipe_itr->value["src_pipe"].GetString(),
          pipe_itr->value["src_last_elem"].GetString()
      );
    }
  } else {
    GST_DEBUG("No intervideo connections are defined.");
  }
}

// Read, create, and save pipes and elements. Set element attributes, then fill them into the pipe
void Json::GetPipelineStructure(Topology *topology) {
  if (json_src.HasMember(JSON_TAG_PIPES)) {
    GST_DEBUG("Reading pipelines from json");

    // Get and validate root of pipe definitions
    const rapidjson::Value &json_pipes_obj = json_src[JSON_TAG_PIPES];
    if (!json_pipes_obj.IsObject()) {
      throw JsonInvalidTypeException("Object to store pipeline topology is not a valid object!");
    }

    // Iterate through the root object
    for (rapidjson::Value::ConstMemberIterator pipe_itr = json_pipes_obj.MemberBegin();
         pipe_itr != json_pipes_obj.MemberEnd(); ++pipe_itr) {

      // Validate pipenames
      if (!pipe_itr->name.IsString())
        throw JsonInvalidTypeException("Pipe name is not a string value!");
      const char *pipe_name = pipe_itr->name.GetString();

      // Create the pipe
      topology->CreatePipeline(pipe_name);

      // Check whether the pipe is assigned to a valid object
      if (!pipe_itr->value.IsObject()) {
        throw JsonInvalidTypeException("Object to store elements in pipe \"%s\" is not a valid object!");
      }
      const rapidjson::Value &elements_obj = pipe_itr->value;

      // Process elements contained in the pipe
      for (rapidjson::Value::ConstMemberIterator elem_itr = elements_obj.MemberBegin();
           elem_itr != elements_obj.MemberEnd(); ++elem_itr) {

        // Check whether the element has a correct name
        if (!elem_itr->name.IsString()) {
          throw JsonInvalidTypeException(std::string("Element name in pipe \"") + pipe_name + "\" is not a valid string!");
        }
        const char *elem_name = elem_itr->name.GetString();

        // Validate that the element contains proper attributes
        if (!elem_itr->value.IsObject())
          throw JsonInvalidTypeException(std::string("Object assigned to element \"") + elem_name + "\" is not a valid object!");
        const rapidjson::Value &json_properties_obj = elem_itr->value;

        // Check whether it has a type defined and it's a correct string
        if (!json_properties_obj.HasMember("type")) {
          throw JsonInvalidTypeException(std::string("Element \"") + elem_name + "\" in pipe \"" + pipe_name + "\" does not have a type");
        }
        if (!json_properties_obj["type"].IsString()) {
          throw JsonInvalidTypeException(std::string("Type assigned to \"") + elem_name + "\" is not a valid string!");
        }
        const char *type_name = json_properties_obj["type"].GetString();

        // Try to create the element
        topology->CreateElement(elem_name, type_name);

        // Iterate through its properties
        for (rapidjson::Value::ConstMemberIterator prop_itr = json_properties_obj.MemberBegin();
             prop_itr != json_properties_obj.MemberEnd(); ++prop_itr) {

          // Validate property name-value pairs
          if (!prop_itr->name.IsString() || !prop_itr->value.IsString()) {
            throw JsonInvalidTypeException(std::string("Definition of \"") + elem_name + "\" properties are incorrect!");
          }

          const char
              *prop_name = prop_itr->name.GetString(),
              *prop_value = prop_itr->value.GetString();

          // Type is already handled, continue processing with the next property
          if (!strcmp("type", prop_name)) {
            continue;
          }

          // Attach property as pre-defined filtercaps and continue processing
          if (!strcmp("filter", prop_name)) {
            topology->AssignCap(elem_name, prop_value);
            continue;
          }

          // Finally, it's not a reserved keyword so set it as a generic property
          topology->SetProperty(elem_name, prop_name, prop_value);

          // End of processing properties
        }

        // Try to add this element to the pipe
        topology->AddElementToBin(elem_name, pipe_name);

        // End of processing pipe elements
      }

      // End of processing pipelines
    }

    // End of processing root pipe object
  } else {
    GST_DEBUG("There are no pipelines defined in the json data!");
  }
}

// Load connections and link elements
void Json::GetConnections(Topology *topology) {

  if (json_src.HasMember(JSON_TAG_LINKS)) {
    GST_DEBUG("Reading element connections from json");

    const rapidjson::Value &json_links_arr = json_src[JSON_TAG_LINKS];
    if (!json_links_arr.IsArray()) {
      throw JsonInvalidTypeException("Object to store element connections is not a valid array!");
    }

    // Iterate through the outer array storing the arrays of the connections
    for (rapidjson::Value::ConstValueIterator itr = json_links_arr.Begin();
         itr != json_links_arr.End(); ++itr) {

      // Validate inner arrays
      if (!itr->IsArray()) {
        throw JsonInvalidTypeException("Object contained in element connections is not a valid array!");
      }
      rapidjson::Value::ConstValueIterator element_itr = itr->Begin();

      // Process property name-value pairs in those arrays
      while (element_itr != itr->End()) {

        // Validate the first element of the connection
        if (!element_itr->IsString()) {
          throw JsonInvalidTypeException("There is an invalid element specified in connections!");
        }
        const char *src_name = element_itr->GetString();

        // Get the second element of the connection
        if (++element_itr != itr->End()) {
          // Validate the second element of the connection
          if (!element_itr->IsString()) {
            throw JsonInvalidTypeException(
                std::string("There is an invalid element specified after \"") + src_name + "\" in connections!"
            );
          }
          // Now connect them
          topology->ConnectElements(src_name, element_itr->GetString());
        }
      }
    }
  } else {
    GST_DEBUG("There are no links defined in the json data!");
  }
}

void Json::CreateTopology(Topology* topology) {
  GetCaps(topology);
  GetPipelineStructure(topology);
  GetRtspPipes(topology);
  GetInterConnections(topology);
  GetConnections(topology);
}

JsonParseException::JsonParseException(rapidjson::ParseErrorCode code, const char *msg, size_t offset)
    : std::runtime_error(msg), ParseResult(code, offset) {
  GST_ERROR("Loading JSON is falied at char %ld: %s", Offset(), what());
}

JsonInvalidTypeException::JsonInvalidTypeException(const std::string &message)
    : std::runtime_error(message) {
  GST_ERROR("Processing JSON is failed: %s", message.c_str());
}
