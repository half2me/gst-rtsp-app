#pragma once
#include <stdexcept>

#define GCF_ASSERT(B, E, S) if (!(B)) throw E(S)

struct GcfException : std::runtime_error {
  GcfException(const std::string& message = "GStreamer Camera Firmware exception.")
      : std::runtime_error(message) {}
};
