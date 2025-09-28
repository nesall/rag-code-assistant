#include "embedder.h"
#include <stdexcept>
#include <iostream>
#include <cmath>  // for std::sqrt
#include <httplib.h>
#include <nlohmann/json.hpp>


EmbeddingClient::EmbeddingClient(const std::string &url, int timeout)
  : serverUrl_(url), timeoutMs_(timeout)
{
  parseUrl();
}

void EmbeddingClient::parseUrl()
{
  size_t protocolEnd = serverUrl_.find("://");
  if (protocolEnd == std::string::npos) {
    throw std::runtime_error("Invalid server URL format");
  }

  size_t hostStart = protocolEnd + 3;
  size_t port_start = serverUrl_.find(":", hostStart);
  size_t path_start = serverUrl_.find("/", hostStart);

  if (port_start != std::string::npos && port_start < path_start) {
    host_ = serverUrl_.substr(hostStart, port_start - hostStart);
    port_ = std::stoi(serverUrl_.substr(port_start + 1, path_start - port_start - 1));
  } else {
    host_ = serverUrl_.substr(hostStart, path_start - hostStart);
    port_ = 80;
  }

  path_ = serverUrl_.substr(path_start);
}

std::vector<float> EmbeddingClient::generateEmbeddings(const std::vector<std::string> &texts)
{
  try {
    httplib::Client client(host_, port_);
    client.set_connection_timeout(0, timeoutMs_ * 1000);
    client.set_read_timeout(timeoutMs_ / 1000, (timeoutMs_ % 1000) * 1000);

    nlohmann::json requestBody;
    requestBody["content"] = texts;
    std::string bodyStr = requestBody.dump();

    httplib::Headers headers = {
        {"Content-Type", "application/json"}
    };

    auto res = client.Post(path_.c_str(), headers, bodyStr, "application/json");

    if (!res) {
      throw std::runtime_error("Failed to connect to embedding server");
    }

    if (res->status != 200) {
      throw std::runtime_error("Server returned error: " +
        std::to_string(res->status) + " - " + res->body);
    }

    nlohmann::json response = nlohmann::json::parse(res->body);

    if (!response.is_array() || response.size() != texts.size()) {
      throw std::runtime_error("Unexpected embedding response format");
    }

    const auto &item = response[0];
    if (!item.contains("embedding") || !item["embedding"].is_array()) {
      throw std::runtime_error("Missing or invalid 'embedding' field in response");
    }

    const auto &embeddingArray = item["embedding"];
    if (embeddingArray.empty() || !embeddingArray[0].is_array()) {
      throw std::runtime_error("Invalid embedding structure");
    }

    std::vector<float> embedding;
    const auto &embeddingData = embeddingArray[0];

    for (const auto &value : embeddingData) {
      if (value.is_number()) {
        embedding.push_back(value.get<float>());
      } else {
        throw std::runtime_error("Non-numeric value in embedding data");
      }
    }

    float l2Norm = calculateL2Norm(embedding);
    std::cout << "[l2norm] " << l2Norm << std::endl;

    return embedding;

  } catch (const nlohmann::json::exception &e) {
    std::cerr << "JSON parsing error: " << e.what() << std::endl;
    throw std::runtime_error("Failed to parse server response");
  } catch (const std::exception &e) {
    std::cerr << "Error generating embeddings: " << e.what() << std::endl;
    throw;
  }
}

std::vector<std::vector<float>> EmbeddingClient::generateBatchEmbeddings(const std::vector<std::string> &texts)
{
  std::vector<std::vector<float>> results;
  for (const auto &text : texts) {
    auto embedding = generateEmbeddings({ text });
    results.push_back(embedding);
  }
  return results;
}

float EmbeddingClient::calculateL2Norm(const std::vector<float> &vec)
{
  float sum = 0.0f;
  for (float val : vec) {
    sum += val * val;
  }
  return std::sqrt(sum);
}

/*
int main() {
    try {
        EmbeddingClient client("http://localhost:8583/embedding");

        std::vector<std::string> texts = {"Hello world", "This is a test"};
        auto embedding = client.generateEmbeddings(texts);

        std::cout << "Generated embedding with " << embedding.size() << " dimensions" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
*/
