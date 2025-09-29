#include "settings.h"
#include <fstream>
#include <stdexcept>
#include <cstdlib>


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

std::vector<Settings::SourceItem> Settings::sources() const
{
  std::vector<SourceItem> res;
  for (const auto &item : config_["sources"]) {
    SourceItem si;
    si.type = item["type"];
    if (si.type == "directory" || si.type == "file") {
      si.path = item["path"];
    }
    if (si.type == "directory") {
      si.recursive = item.value("recursive", true);
      si.extensions = item.value("extensions", std::vector<std::string>{});
      si.exclude = item.value("exclude", std::vector<std::string>{});
      auto f = filesDefaultExtensions();
      if (si.extensions.empty() && !f.empty()) {
        si.extensions = f;
      }
      auto x = filesGlobalExclusions();
      if (!x.empty()) {
        si.exclude.insert(si.exclude.end(), x.begin(), x.end());
      }
    }
    if (si.type == "url") {
      si.url = item["url"];
      if (item.contains("headers")) {
        for (const auto &[key, value] : item["headers"].items()) {
          std::string headerValue = value;
          // Simple ${VAR} substitution
          if (headerValue.starts_with("${") && headerValue.ends_with("}")) {
            std::string envVar = headerValue.substr(2, headerValue.length() - 3);
            const char *envValue = nullptr;
            envValue = getenv(envVar.c_str());
            if (envValue) {
              headerValue = std::string(envValue);
            }
          }
          si.headers[key] = headerValue;
        }
      }
      si.urlTimeoutMs = item.value("timeout_ms", 10000);
    }
    res.push_back(si);
  }
  return res;
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
