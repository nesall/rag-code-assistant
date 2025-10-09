#include "sourceproc.h"
#include <iostream>
#include <fstream>
#include <exception>
#include <filesystem>
#include "settings.h"
#include <httplib.h>


std::vector<SourceProcessor::Data> SourceProcessor::collectSources()
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
  for (const auto &a : allContent) {
    sources_.insert(a.source);
  }
  return allContent;
}

SourceProcessor::Data SourceProcessor::fetchSource(const std::string &uri) const
{
  std::vector<SourceProcessor::Data> res;
  auto sources = settings_.sources();
  for (const auto &source : sources) {
    if (source.type == "file" && source.path == uri) {
      processFile(source.path, res);
    } else if (source.type == "url" && source.url == uri) {
      processUrl(source, res);
    } else if (source.type == "directory") {
      processDirItem(source, uri, res);
    }
    if (!res.empty()) break;
  }
  return res.empty() ? Data{} : res[0];
}

std::vector<std::string> SourceProcessor::filterRelatedSources(const std::vector<std::string> &sources, const std::string &uri)
{
  std::vector<std::string> res;
  std::string base = std::filesystem::path(uri).stem().string();
  for (const auto &s : sources) {
    auto t = std::filesystem::path(s).stem().string();
    if (t == base || t.find(base) != std::string::npos) {
      res.push_back(s);
    }
  }
  return res;
}

void SourceProcessor::processDirectory(const Settings::SourceItem &source, std::vector<SourceProcessor::Data> &content) const
{
  namespace fs = std::filesystem;
  std::string path = source.path;
  const auto &extensions = source.extensions;
  const auto &exclude = source.exclude;
  try {
    for (const auto &entry : fs::directory_iterator(path)) {
      const auto entryPath = entry.path().string();
      if (isExcluded(entryPath, exclude))
        continue;
      if (entry.is_directory() && source.recursive) {
        Settings::SourceItem subSource = source;
        subSource.path = entryPath;
        processDirectory(subSource, content);
      } else if (entry.is_regular_file()) {
        processDirItem(source, entryPath, content);
      }
    }
  } catch (const std::exception &) {
    std::cout << "Unable to process resource " << path << ". Skipped.\n";
  }
}

bool SourceProcessor::processDirItem(const Settings::SourceItem &source, const std::string &filepath, std::vector<SourceProcessor::Data> &content) const
{
  if (isExcluded(filepath, source.exclude)) return false;
  if (!source.extensions.empty() && !hasValidExtension(filepath, source.extensions)) return false;
  processFile(filepath, content);
  return true;
}

void SourceProcessor::processFile(const std::string &filepath, std::vector<SourceProcessor::Data> &content) const
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
    content.push_back({ buffer.str(), std::filesystem::path(filepath).lexically_normal().generic_string() });
  } else {
    std::cout << "Unable to process resource " << filepath << ". Skipped.\n";
  }
}

void SourceProcessor::processUrl(const Settings::SourceItem &source, std::vector<SourceProcessor::Data> &content) const
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
  auto normalize = [](std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
    };

  auto path = std::filesystem::path(filepath).lexically_normal().generic_string();

  if (std::filesystem::is_directory(path) && !path.ends_with('/')) {
    path += '/';
  }

  for (const auto &p : patterns) {
    std::string pattern = normalize(p);
    if (pattern.empty()) continue;
    if (pattern == "*") return true; // exclude all
    if (pattern.front() == '*' && pattern.back() == '*') {
      auto core = pattern.substr(1, pattern.length() - 2);
      if (!core.empty() && path.find(core) != std::string::npos) return true;
    } else if (pattern.front() == '*') {
      auto core = pattern.substr(1);
      if (!core.empty() && path.ends_with(core)) return true;
    } else if (pattern.back() == '*') {
      auto core = pattern.substr(0, pattern.length() - 1);
      if (!core.empty() && path.starts_with(core)) return true;
    } else {
      if (path == pattern || std::filesystem::path(path).filename() == pattern) return true;
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
