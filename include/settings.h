#ifndef _SETTINGS_H_
#define _SETTINGS_H_

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
  std::string databaseIndexPath() const { return config_["database"]["index_path"]; }
  size_t databaseVectorDim() const { return config_["database"]["vector_dim"]; }
  nlohmann::json sources() const { return config_["sources"]; }

private:
  void expandEnvVars();
};


#endif // _SETTINGS_H_
