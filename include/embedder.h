#pragma once

#include <vector>
#include <string>

class EmbeddingClient {
public:
  EmbeddingClient(const std::string &url, int timeout = 30000);
  void generateEmbeddings(const std::vector<std::string> &texts, std::vector<float> &embedding);
  std::vector<std::vector<float>> generateBatchEmbeddings(const std::vector<std::string> &texts);

private:
  std::string serverUrl_;
  std::string host_;
  std::string path_;
  int port_;
  int timeoutMs_;

  void parseUrl();
  float calculateL2Norm(const std::vector<float> &vec);
};
