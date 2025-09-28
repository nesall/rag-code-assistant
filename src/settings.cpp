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


std::vector<SourceProcessor::Data> SourceProcessor::getAllSources()
{
  std::vector<SourceProcessor::Data> allContent;
  auto sources = settings.sources();

  for (const auto &source : sources) {
    std::string type = source["type"];
      if (type == "directory") {
        processDirectory(source, allContent);
      } else if (type == "file") {
        processFile(source["path"], allContent);
      } else if (type == "url") {
        processUrl(source, allContent);
      }
  }
  return allContent;
}

void SourceProcessor::processDirectory(const nlohmann::json &source, std::vector<SourceProcessor::Data> &content)
{
  std::string path = source["path"];
  bool recursive = source.value("recursive", true);
  auto extensions = source.value("extensions", std::vector<std::string>{});
  auto exclude = source.value("exclude", std::vector<std::string>{});
  try {
    for (const auto &entry : std::filesystem::recursive_directory_iterator(path)) {
      if (!entry.is_regular_file()) continue;

      std::string filepath = entry.path().string();

      // Check exclusions
      if (isExcluded(filepath, exclude)) continue;

      // Check extensions
      if (!extensions.empty() && !hasValidExtension(filepath, extensions)) continue;

      processFile(filepath, content);
    }
  } catch (const std::exception &) {
    std::cout << "Unable to process resource " << path << ". Skipped.\n";
  }
}

void SourceProcessor::processFile(const std::string &filepath, std::vector<SourceProcessor::Data> &content)
{
  std::ifstream file(filepath);
  if (file.is_open()) {
    std::stringstream buffer;
    buffer << file.rdbuf();
    content.push_back({ buffer.str(), filepath });
  } else {
    std::cout << "Unable to process resource " << filepath << ". Skipped.\n";
  }
}

void SourceProcessor::processUrl(const nlohmann::json &source, std::vector<SourceProcessor::Data> &content)
{
  std::string url = source["url"];
  time_t timeout = static_cast<time_t>(source.value("timeout_ms", 10000));

  try {
    // Parse URL
    size_t pos = url.find("://");
    if (pos == std::string::npos) return;

    size_t host_start = pos + 3;
    size_t path_start = url.find("/", host_start);

    std::string host = url.substr(host_start, path_start - host_start);
    std::string path = url.substr(path_start);

    httplib::Client client(("https://" + host).c_str());
    client.set_connection_timeout(0, timeout * 1000);

    // Add headers if specified
    httplib::Headers headers;
    if (source.contains("headers")) {
      for (auto &[key, value] : source["headers"].items()) {
        headers.emplace(key, value.get<std::string>());
      }
    }

    auto res = client.Get(path.c_str(), headers);
    if (res && res->status == 200) {
      content.push_back({ res->body, url });
    }
  } catch (const std::exception &) {
    std::cout << "Unable to process resource " << url << ". Skipped.\n";
  }
}

bool SourceProcessor::isExcluded(const std::string &filepath, const std::vector<std::string> &patterns)
{
  for (const auto &pattern : patterns) {
    if (filepath.find(pattern.substr(2)) != std::string::npos) { // Skip "*/"
      return true;
    }
  }
  return false;
}

bool SourceProcessor::hasValidExtension(const std::string &filepath, const std::vector<std::string> &extensions)
{
  for (const auto &ext : extensions) {
    if (filepath.ends_with(ext)) return true;
  }
  return false;
}
