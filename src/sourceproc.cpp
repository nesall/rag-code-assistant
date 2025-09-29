#include "sourceproc.h"
#include <iostream>
#include <fstream>
#include <exception>
#include "settings.h"
#include <httplib.h>


std::vector<SourceProcessor::Data> SourceProcessor::getSources()
{
  std::vector<SourceProcessor::Data> allContent;
  auto sources = settings_.sources();
  for (const auto &source : sources) {
    std::string type = source.type;
    if (type == "directory") {
      processDirectory(source, allContent);
    } else if (type == "file") {
      processFile(source.path, allContent);
    } else if (type == "url") {
      processUrl(source, allContent);
    }
  }
  return allContent;
}

void SourceProcessor::processDirectory(const Settings::SourceItem &source, std::vector<SourceProcessor::Data> &content)
{
  std::string path = source.path;
  bool recursive = source.recursive;
  const auto &extensions = source.extensions;
  const auto &exclude = source.exclude;
  try {
    namespace fs = std::filesystem;
    if (source.recursive) {
      for (const auto &entry : fs::recursive_directory_iterator(path)) {
        if (!entry.is_regular_file()) continue;
        processDirItem(source, entry.path().string(), content);
      }
    } else {
      for (const auto &entry : fs::directory_iterator(path)) {
        if (!entry.is_regular_file()) continue;
        processDirItem(source, entry.path().string(), content);
      }
    }
  } catch (const std::exception &) {
    std::cout << "Unable to process resource " << path << ". Skipped.\n";
  }
}

bool SourceProcessor::processDirItem(const Settings::SourceItem &source, const std::string &filepath, std::vector<SourceProcessor::Data> &content)
{
  if (isExcluded(filepath, source.exclude)) return false;
  if (!source.extensions.empty() && !hasValidExtension(filepath, source.extensions)) return false;
  processFile(filepath, content);
  return true;
}

void SourceProcessor::processFile(const std::string &filepath, std::vector<SourceProcessor::Data> &content)
{
  if (auto maxSize = settings_.filesMaxFileSizeMb(); 0 < maxSize) {
    try {
      auto fsize = std::filesystem::file_size(filepath);
      if (maxSize * 1024 * 1024 < fsize) {
        std::cout << "File " << filepath << " exceeds max allowed size of " << maxSize << " MB. Skipped.\n";
        return;
      }
    } catch (const std::exception &) {
      std::cout << "Unable to process resource " << filepath << ". Skipped.\n";
      return;
    }
  }
  std::ifstream file(filepath);
  if (file.is_open()) {
    std::stringstream buffer;
    buffer << file.rdbuf();
    content.push_back({ buffer.str(), filepath });
  } else {
    std::cout << "Unable to process resource " << filepath << ". Skipped.\n";
  }
}

void SourceProcessor::processUrl(const Settings::SourceItem &source, std::vector<SourceProcessor::Data> &content)
{
  std::string url = source.url;
  time_t timeout = static_cast<time_t>(source.urlTimeoutMs);
  try {
    size_t pos = url.find("://");
    if (pos == std::string::npos) return;
    size_t host_start = pos + 3;
    size_t path_start = url.find("/", host_start);
    std::string host = url.substr(host_start, path_start - host_start);
    std::string path = url.substr(path_start);
    httplib::Client client(("https://" + host).c_str());
    client.set_connection_timeout(0, timeout * 1000);
    httplib::Headers headers;
    for (auto &[key, value] : source.headers) {
      headers.emplace(key, value);
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
