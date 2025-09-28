#include "embedder.h"
#include <stdexcept>
#include <iostream>
#include <cmath>  // for std::sqrt
#include <httplib.h>
#include <nlohmann/json.hpp>


EmbeddingClient::EmbeddingClient(const std::string &url, int timeout)
  : server_url(url), timeout_ms(timeout)
{
  parseUrl();
}

void EmbeddingClient::parseUrl()
{
  size_t protocol_end = server_url.find("://");
  if (protocol_end == std::string::npos) {
    throw std::runtime_error("Invalid server URL format");
  }

  size_t host_start = protocol_end + 3;
  size_t port_start = server_url.find(":", host_start);
  size_t path_start = server_url.find("/", host_start);

  if (port_start != std::string::npos && port_start < path_start) {
    host = server_url.substr(host_start, port_start - host_start);
    port = std::stoi(server_url.substr(port_start + 1, path_start - port_start - 1));
  } else {
    host = server_url.substr(host_start, path_start - host_start);
    port = 80;
  }

  path = server_url.substr(path_start);
}

std::vector<float> EmbeddingClient::generateEmbeddings(const std::vector<std::string> &texts)
{
  try {
    httplib::Client client(host, port);
    client.set_connection_timeout(0, timeout_ms * 1000);
    client.set_read_timeout(timeout_ms / 1000, (timeout_ms % 1000) * 1000);

    nlohmann::json request_body;
    request_body["content"] = texts;
    std::string body_str = request_body.dump();

    httplib::Headers headers = {
        {"Content-Type", "application/json"}
    };

    auto res = client.Post(path.c_str(), headers, body_str, "application/json");

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

    const auto &embedding_array = item["embedding"];
    if (embedding_array.empty() || !embedding_array[0].is_array()) {
      throw std::runtime_error("Invalid embedding structure");
    }

    std::vector<float> embedding;
    const auto &embedding_data = embedding_array[0];

    for (const auto &value : embedding_data) {
      if (value.is_number()) {
        embedding.push_back(value.get<float>());
      } else {
        throw std::runtime_error("Non-numeric value in embedding data");
      }
    }

    float l2_norm = calculateL2Norm(embedding);
    std::cout << "[l2norm] " << l2_norm << std::endl;

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
