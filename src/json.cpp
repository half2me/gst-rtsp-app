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
void Json::ReadCaps(Topology* topology) {
  if (json_src.HasMember(JSON_TAG_CAPS)) {
    GST_LOG("Reading caps from json");

    const rapidjson::Value &json_caps_obj = json_src[JSON_TAG_CAPS];

    for (rapidjson::Value::ConstMemberIterator itr = json_caps_obj.MemberBegin();
         itr != json_caps_obj.MemberEnd(); ++itr) {

      if (!itr->name.IsString() || !itr->value.IsString())
        throw JsonFormatException();

      topology->CreateCap(itr->name.GetString(), itr->value.GetString());
    }
  }
}

bool Json::CreateTopology(Topology* topology) {

  ReadCaps(topology);


  // Read, create, and save pipes and elements. Set
  // element attributes, then fill them into the pipe
  // ------------------------------------------------
  if (json_src.HasMember(JSON_TAG_PIPES)) {
    GST_LOG("Reading pipelines from json");

    const rapidjson::Value &json_pipes_obj = json_src[JSON_TAG_PIPES];

    for (rapidjson::Value::ConstMemberIterator pipe_itr = json_pipes_obj.MemberBegin();
         pipe_itr != json_pipes_obj.MemberEnd(); ++pipe_itr) {

      const char *pipe_name = pipe_itr->name.GetString();

      if (!topology->CreatePipeline(pipe_name)) {
        return false;
      }

      const rapidjson::Value &elements_obj = pipe_itr->value;

      for (rapidjson::Value::ConstMemberIterator elem_itr = elements_obj.MemberBegin();
           elem_itr != elements_obj.MemberEnd(); ++elem_itr) {

        const char *elem_name = elem_itr->name.GetString();
        const rapidjson::Value &json_properties_obj = elem_itr->value;

        // First check if the element can be created
        if (!json_properties_obj.HasMember("type")) {
          GST_ERROR("Element \"%s\" in pipe \"%s\" does not have a type!", elem_name, pipe_name);
          return false;
        }

        const char *type_name = json_properties_obj["type"].GetString();

        if (!topology->CreateElement(elem_name, type_name)) {
          return false;
        }

        for (rapidjson::Value::ConstMemberIterator prop_itr = json_properties_obj.MemberBegin();
             prop_itr != json_properties_obj.MemberEnd(); ++prop_itr) {

          const char
              *prop_name = prop_itr->name.GetString(),
              *prop_value = prop_itr->value.GetString();

          // It's already handled
          if (!strcmp("type", prop_name)) {
            continue;
          }

          // attach pre-defined filtercaps
          if (!strcmp("filter", prop_name)) {

            if (!topology->AssignCap(elem_name, prop_value)) {
              return false;
            }
            continue;
          }

          // Not a reserved keyword, set it as a generic attribute
          if (!topology->SetProperty(elem_name, prop_name, prop_value)) {
            return false;
          }

        }


        // Try to add this element to the pipe
        if (!topology->AddElementToBin(elem_name, pipe_name)) {
          return false;
        }

      }
    }
  } else {
    GST_ERROR("There are no pipelines defined in the json data!");
    return false;
  }

  // Read and save list of RTSP Pipes
  // --------------------------------
  if (json_src.HasMember(JSON_TAG_RTSP)) {
    GST_LOG("Reading RTSP Pipes from JSON...");

    const rapidjson::Value &json_rtsp_arr = json_src[JSON_TAG_RTSP];

    for (rapidjson::Value::ConstValueIterator itr = json_rtsp_arr.Begin(); itr != json_rtsp_arr.End(); ++itr) {
      const char *pipe_name = itr->GetString();

      if (!topology->HasPipe(pipe_name)) {
        GST_ERROR("Can't create RTSP pipe from \"%s\": Pipe does not exists", pipe_name);
        return false;
      }

      topology->SetRtspPipe(pipe_name, topology->GetPipe(pipe_name));
      GST_LOG("\"%s\" is marked as RTSP Pipe.", pipe_name);
    }
  }

  // Intervideo powered pipe connections
  // -----------------------------------
  if (json_src.HasMember(JSON_TAG_CONNECTIONS)) {
    const rapidjson::Value &json_rtsp_pipes_obj = json_src[JSON_TAG_CONNECTIONS];

    for (rapidjson::Value::ConstMemberIterator pipe_itr = json_rtsp_pipes_obj.MemberBegin();
         pipe_itr != json_rtsp_pipes_obj.MemberEnd(); ++pipe_itr) {

      const char
          *pipe_name = pipe_itr->name.GetString(),
          *first_elem_name = pipe_itr->value["first_elem"].GetString(),
          *dst_pipe_name = pipe_itr->value["src_pipe"].GetString(),
          *dst_last_elem_name = pipe_itr->value["src_last_elem"].GetString();

      if (!topology->ConnectPipe(pipe_name, first_elem_name, dst_pipe_name, dst_last_elem_name)) {
        GST_ERROR ("Unable to link pipe %s to %s", pipe_name, dst_pipe_name);
        return false;
      }
    }
  }

  // Load connections and link elements
  // ----------------------------------
  if (json_src.HasMember(JSON_TAG_LINKS)) {
    GST_LOG("Reading element connections from json");

    const rapidjson::Value &json_links_arr = json_src[JSON_TAG_LINKS];

    for (rapidjson::Value::ConstValueIterator itr = json_links_arr.Begin();
         itr != json_links_arr.End(); ++itr) {


      rapidjson::Value::ConstValueIterator inner_itr = itr->Begin();
      while (inner_itr != itr->End()) {
        const char *src_name = inner_itr->GetString();

        if (++inner_itr != itr->End()) {
          const char *dst_name = inner_itr->GetString();

          if (!topology->ConnectElements(src_name, dst_name)) {
            return false;
          }
        }
      }
    }
  } else {
    GST_ERROR("There are no links defined in the json data!");
    return false;
  }

  return true;
}

JsonParseException::JsonParseException(rapidjson::ParseErrorCode code, const char *msg, size_t offset)
    : std::runtime_error(msg), ParseResult(code, offset) {
  GST_ERROR("Loading JSON is falied at char %ld: %s", Offset(), what());
}

JsonFormatException::JsonFormatException(const std::string &message)
    : std::runtime_error(message) {
  GST_ERROR("%s", message.c_str());
}
