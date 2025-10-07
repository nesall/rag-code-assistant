#pragma once

#include <string>
#include <unordered_map>
#include "nlohmann/json.hpp"

class SimpleTokenCounter {
  mutable std::unordered_map<std::string, size_t> cache_;
private:
  nlohmann::json vocab_;
  size_t maxInputCharsPerWord_ = 100;
  size_t simulateWordpiece(const std::string &word, bool addSpecialTokens) const;
public:
  explicit SimpleTokenCounter(const std::string &config_path);
  size_t estimateTokenCount(const std::string &text, bool addSpecialTokens = false) const;
  size_t countTokensWithVocab(const std::string &text, bool addSpecialTokens = false) const;
};

