#pragma once

#include <vector>
#include <string>
#include <nlohmann/json.hpp>


struct SearchResult;

class InferenceClient {
public:
  InferenceClient(const std::string &url, size_t timeout = 30000);

protected:
  std::string serverUrl_;
  std::string host_;
  std::string path_;
  int port_ = 0;
  size_t timeoutMs_;

  void parseUrl();
};

class EmbeddingClient : public InferenceClient {
public:
  EmbeddingClient(const std::string &url, size_t timeout = 30000);
  void generateEmbeddings(const std::vector<std::string> &texts, std::vector<float> &embedding);
  std::vector<std::vector<float>> generateBatchEmbeddings(const std::vector<std::string> &texts);

private:
  float calculateL2Norm(const std::vector<float> &vec);
};

class CompletionClient : public InferenceClient {
public:
  CompletionClient(const std::string &url, size_t timeout = 30000);
  std::string generateCompletion(
    const nlohmann::json &messages, 
    const std::vector<SearchResult> &searchRes, 
    float temperature,
    std::function<void(const std::string &)> onStream);

private:

};