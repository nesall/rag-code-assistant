#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <string>
#include <vector>
#include <map>
#include "nlohmann/json.hpp"


class Settings {
private:
  nlohmann::json config_;

public:
  struct SourceItem {
    std::string type; // "directory", "file", "url"
    std::string path; // for "directory" and "file"
    bool recursive = true; // for "directory"
    std::vector<std::string> extensions; // for "directory"
    std::vector<std::string> exclude; // for "directory"
    std::string url; // for "url"
    std::map<std::string, std::string> headers; // for "url"
    std::size_t urlTimeoutMs; // for "url"
  };

public:
  Settings(const std::string &path = "settings.json");
  std::string tokenizerConfigPath() const { return config_["tokenizer"]["config_path"]; }
  
  int chunkingMaxTokens() const { return config_["chunking"]["nof_max_nokens"]; }
  int chunkingMinTokens() const { return config_["chunking"]["nof_min_tokens"]; }
  float chunkingOverlap() const { return config_["chunking"]["overlap_percentage"]; }
  bool chunkingSemantic() const { return config_["chunking"]["semantic"]; }

  std::string embeddingApiUrl() const { return config_["embedding"]["api_url"]; }
  std::string embeddingApiKey() const { return config_["embedding"]["api_key"]; }
  std::string embeddingModel() const { return config_["embedding"]["model"]; }
  size_t embeddingTimeoutMs() const { return config_["embedding"]["timeout_ms"]; }
  size_t embeddingBatchSize() const { return config_["embedding"]["batch_size"]; }

  std::string generationApiUrl() const { return config_["generation"]["api_url"]; }
  std::string generationApiKey() const { return config_["generation"]["api_key"]; }
  std::string generationModel() const { return config_["generation"]["model"]; }
  size_t generationTimeoutMs() const { return config_["generation"]["timeout_ms"]; }

  std::string databaseSqlitePath() const { return config_["database"]["sqlite_path"]; }
  std::string databaseIndexPath() const { return config_["database"]["index_path"]; }
  size_t databaseVectorDim() const { return config_["database"]["vector_dim"]; }
  size_t databaseMaxElements() const { return config_["database"]["max_elements"]; }
  
  size_t filesMaxFileSizeMb() const { return config_["files"]["max_file_size_mb"]; }
  std::string filesEncoding() const { return config_["files"]["encoding"]; }
  std::vector<std::string> filesGlobalExclusions() const { return config_["files"]["global_exclude"]; }
  std::vector<std::string> filesDefaultExtensions() const { return config_["files"]["default_extensions"]; }
  
  std::vector<SourceItem> sources() const;

private:
  void expandEnvVars();
};


#endif // _SETTINGS_H_
