#pragma once

#include <vector>
#include <string>

class EmbeddingClient {
public:
  EmbeddingClient(const std::string &url, int timeout = 30000);

  std::vector<float> generateEmbeddings(const std::vector<std::string> &texts);

  std::vector<std::vector<float>> generateBatchEmbeddings(const std::vector<std::string> &texts);

private:
  std::string server_url;
  std::string host;
  std::string path;
  int port;
  int timeout_ms;

  void parseUrl();
  float calculateL2Norm(const std::vector<float> &vec);
};
