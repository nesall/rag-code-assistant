#pragma once

#include <vector>
#include <string>
#include <functional>
#include <nlohmann/json.hpp>


class App;
class Settings;
struct SearchResult;

class InferenceClient {
public:
  InferenceClient(const std::string &url, const std::string &apiKey, const std::string &model, size_t timeout);
  virtual ~InferenceClient() = default;

protected:
  std::string serverUrl_;
  std::string apiKey_;
  std::string model_;
  std::string host_;
  std::string path_;
  size_t timeoutMs_;
  int port_ = 0;

  void parseUrl();
};

class EmbeddingClient : public InferenceClient {
public:
  EmbeddingClient(const Settings &settings);
  void generateEmbeddings(const std::vector<std::string> &texts, std::vector<float> &embedding) const;
  std::vector<std::vector<float>> generateBatchEmbeddings(const std::vector<std::string> &texts) const;

private:
  static float calculateL2Norm(const std::vector<float> &vec);
};

class CompletionClient : public InferenceClient {
  const App &app_;
  const size_t maxContextTokens_;
public:
  CompletionClient(const App &app);
  std::string generateCompletion(
    const nlohmann::json &messages, 
    const std::vector<SearchResult> &searchRes, 
    float temperature,
    std::function<void(const std::string &)> onStream) const;

private:

};