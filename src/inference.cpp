#include "inference.h"
#include "database.h"
#include <stdexcept>
#include <iostream>
#include <cmath>  // for std::sqrt
#include <httplib.h>


InferenceClient::InferenceClient(const std::string &url, size_t timeout)
  : serverUrl_(url), timeoutMs_(timeout)
{
  parseUrl();
}

void InferenceClient::parseUrl()
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


//---------------------------------------------------------------------------


EmbeddingClient::EmbeddingClient(const std::string &url, size_t timeout) : InferenceClient(url, timeout)
{
}

void EmbeddingClient::generateEmbeddings(const std::vector<std::string> &texts, std::vector<float> &embedding)
{
  embedding.reserve(1024);
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
      throw std::runtime_error("Server returned error: " + std::to_string(res->status) + " - " + res->body);
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
    const auto &embeddingData = embeddingArray[0];
    for (const auto &value : embeddingData) {
      if (value.is_number()) {
        embedding.push_back(value.get<float>());
      } else {
        throw std::runtime_error("Non-numeric value in embedding data");
      }
    }
    //float l2Norm = calculateL2Norm(embedding);
    //std::cout << "[l2norm] " << l2Norm << std::endl;
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
    std::vector<float> embedding;
    generateEmbeddings({ text }, embedding);
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


//---------------------------------------------------------------------------


namespace {
  const std::string &_queryTemplate{ R"(
  You're a helpful software developer assistent, please use the provided context to base your answers on
  for user questions. Answer to the best of your knowledge.
  Context:
  __CONTEXT__

  __QUESTION__
  )" };
} // anonymous namespace

CompletionClient::CompletionClient(const std::string &url, size_t timeout) : InferenceClient(url, timeout)
{
}

std::string CompletionClient::generateCompletion(
  const nlohmann::json &messagesJson, 
  const std::vector<SearchResult> &searchRes, 
  float temperature,
  std::function<void(const std::string &)> onStream)
{
  httplib::Client client(host_, port_);
  client.set_connection_timeout(0, timeoutMs_ * 1000);
  client.set_read_timeout(timeoutMs_ / 1000, (timeoutMs_ % 1000) * 1000);

  /*
  * Json Request body format.
  {
    "model": "",
    "messages": [
      {"role": "system", "content": "Keep it short."},
      {"role": "user", "content": "What is the capital of France?"}
    ],
    "temperature": 0.7
   }
  */

  std::string question = messagesJson.back()["content"].get<std::string>();

  std::string context;
  for (const auto &r : searchRes) {
    context += r.content + "\n\n";
  }

  std::string prompt = _queryTemplate;
  size_t pos = prompt.find("__CONTEXT__");
  assert(pos != std::string::npos);
  prompt.replace(pos, std::string("__CONTEXT__").length(), context);
  
  pos = prompt.find("__QUESTION__");
  assert(pos != std::string::npos);
  prompt.replace(pos, std::string("__QUESTION__").length(), question);  

  // Assign propmt to the last messagesJson's content field
  nlohmann::json modifiedMessages = messagesJson;
  modifiedMessages.back()["content"] = prompt;

  //std::cout << "Full context: " << modifiedMessages.dump() << "\n";

  nlohmann::json requestBody;
  requestBody["model"] = "";
  requestBody["messages"] = modifiedMessages;
  requestBody["temperature"] = temperature;
  requestBody["stream"] = true;

  httplib::Headers headers = {
      {"Content-Type", "application/json"}
  };

  std::string fullResponse;
  std::string buffer; // holds leftover partial data

  auto res = client.Post(
    path_.c_str(),
    headers,
    requestBody.dump(),
    "application/json",
    [&fullResponse, &onStream, &buffer](const char *data, size_t len) {
      // llama-server sends SSE format: "data: {...}\n\n"
      buffer.append(data, len);
      size_t pos;
      while ((pos = buffer.find("\n\n")) != std::string::npos) {
        std::string event = buffer.substr(0, pos); // one SSE event
        buffer.erase(0, pos + 2);
        if (event.find("data: ", 0) == 0) {
          std::string jsonStr = event.substr(6);
          if (jsonStr == "[DONE]") {
            break;
          }
          try {
            nlohmann::json chunkJson = nlohmann::json::parse(jsonStr);
            if (chunkJson.contains("choices") && !chunkJson["choices"].empty()) {
              const auto &choice = chunkJson["choices"][0];
              if (choice.contains("delta") && choice["delta"].contains("content")) {
                std::string content = choice["delta"]["content"];
                fullResponse += content;
                if (onStream) {
                  onStream(content);
                }
              }
            }
          } catch (const std::exception &e) {
            std::cerr << "Error parsing chunk: " << e.what() << " in: " << jsonStr << std::endl;
          }
        }
      }
      return true; // Continue receiving
    }
  );

  if (!res) {
    throw std::runtime_error("Failed to connect to completion server");
  }

  if (res->status != 200) {
    throw std::runtime_error("Server returned error: " + std::to_string(res->status) + " - " + res->body);
  }

  return fullResponse;
}
