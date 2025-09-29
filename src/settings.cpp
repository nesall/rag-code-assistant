#include "settings.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <httplib.h>


Settings::Settings(const std::string &path)
{
  std::ifstream file(path);
  if (!file.is_open()) {
    file.open("../" + path);
    if (!file.is_open()) {
      file.open("../../" + path);
      if (!file.is_open()) {
        throw std::runtime_error("Cannot open settings file: " + path);
      }
    }
  }
  file >> config_;

  // Environment variable substitution
  expandEnvVars();
}

void Settings::expandEnvVars()
{
  // Simple ${VAR} substitution
  std::string apiKey = config_["embedding"]["api_key"];
  if (apiKey.starts_with("${") && apiKey.ends_with("}")) {
    std::string envVar = apiKey.substr(2, apiKey.length() - 3);
    const char *envValue = nullptr;
    envValue = getenv(envVar.c_str());
    if (envValue) {
      config_["embedding"]["api_key"] = std::string(envValue);
    }
  }
}


//--------------------------------------------------------------------------

