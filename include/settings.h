#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <vector>
#include "nlohmann/json.hpp"


class Settings {
private:
  nlohmann::json config_;

public:
  Settings(const std::string &path = "settings.json");
  std::string tokenizerPath() const { return config_["tokenizer"]["config_path"]; }
  int chunkingMaxTokens() const { return config_["chunking"]["nof_max_nokens"]; }
  int chunkingMinTokens() const { return config_["chunking"]["nof_min_tokens"]; }
  float chunkingOverlap() const { return config_["chunking"]["overlap_percentage"]; }
  bool chunkingSemantic() const { return config_["chunking"]["semantic"]; }
  std::string embeddingApiUrl() const { return config_["embedding"]["api_url"]; }
  std::string embeddingBatchSize() const { return config_["embedding"]["batch_size"]; }
  std::string databaseSqlitePath() const { return config_["database"]["sqlite_path"]; }
  nlohmann::json sources() const { return config_["sources"]; }

private:
  void expandEnvVars();
};


class SourceProcessor {
public:
  struct Data {
    std::string content;
    std::string source;
  };
private:
  Settings &settings;

public:
  SourceProcessor(Settings &s) : settings(s) {}
  std::vector<SourceProcessor::Data> getAllSources();

private:
  void processDirectory(const nlohmann::json &source, std::vector<SourceProcessor::Data> &content);
  void processFile(const std::string &filepath, std::vector<SourceProcessor::Data> &content);
  void processUrl(const nlohmann::json &source, std::vector<SourceProcessor::Data> &content);
  bool isExcluded(const std::string &filepath, const std::vector<std::string> &patterns);  
  bool hasValidExtension(const std::string &filepath, const std::vector<std::string> &extensions);
};

#endif // _SETTINGS_H_
