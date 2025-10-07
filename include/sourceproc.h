#ifndef _SOURCEPROC_H_
#define _SOURCEPROC_H_

#include <vector>
#include <string>
#include "settings.h"


class SourceProcessor {
public:
  struct Data {
    std::string content;
    std::string source;
  };
private:
  Settings &settings_;

public:
  SourceProcessor(Settings &s) : settings_(s) {}
  std::vector<SourceProcessor::Data> getSources() const;

private:
  void processDirectory(const Settings::SourceItem &source, std::vector<SourceProcessor::Data> &content) const;
  bool processDirItem(const Settings::SourceItem &source, const std::string &filepath, std::vector<SourceProcessor::Data> &content) const;
  void processFile(const std::string &filepath, std::vector<SourceProcessor::Data> &content) const;
  void processUrl(const Settings::SourceItem &source, std::vector<SourceProcessor::Data> &content) const;
  static bool isExcluded(const std::string &filepath, const std::vector<std::string> &patterns);
  static bool hasValidExtension(const std::string &filepath, const std::vector<std::string> &extensions);
};

#endif // _SOURCEPROC_H_