#include "tokenizer.h"

#include <algorithm>
#include <vector>
#include <string>
#include <regex>
#include <fstream>
#include <sstream>
#include <codecvt>
#include <locale>

/** Example usage
    // Read the text file
    std::string text = readFile("C:\\Users\\Arman\\workspace\\projects\\embedder_cpp\\main.cpp");
    std::cout << "File content length: " << text.length() << " characters\n";

    // Initialize simplified tokenizerC:\Users\Arman\workspace\projects\embedder_cpp\tokenizer
    tokenizer::SimpleTokenCounter tokenizer("c:/users/arman/workspace/projects/embedder_cpp/tokenizer/bge_tokenizer.json");  // Optional: falls back to estimation

    // Get approximate token count
    size_t token_count = tokenizer.estimateTokenCount(text);
    std::cout << "Estimated token count: " << token_count << std::endl;

    // More accurate count if vocab is available
    size_t accurate_count = tokenizer.countTokensWithVocab(text);
    std::cout << "Token count (with vocab): " << accurate_count << std::endl;

    std::cout << std::endl;
*/

using json = nlohmann::json;
//using namespace tokenizer;

namespace {

  bool _is_punctuation_simple(char c) {
    return (c >= 33 && c <= 47) || (c >= 58 && c <= 64) ||
      (c >= 91 && c <= 96) || (c >= 123 && c <= 126);
  }

  bool _is_chinese_char_simple(uint32_t c) {
    return (c >= 0x4E00 && c <= 0x9FFF) ||
      (c >= 0x3400 && c <= 0x4DBF) ||
      (c >= 0xF900 && c <= 0xFAFF);
  }

  std::vector<uint32_t> utf8_to_utf32_simple(const std::string &str) {
    std::vector<uint32_t> result;
    for (size_t i = 0; i < str.size();) {
      uint32_t codepoint = 0;
      unsigned char c = str[i];

      if (c < 0x80) {
        codepoint = c;
        i += 1;
      } else if ((c >> 5) == 0x06) {
        codepoint = ((c & 0x1F) << 6) | (str[i + 1] & 0x3F);
        i += 2;
      } else if ((c >> 4) == 0x0E) {
        codepoint = ((c & 0x0F) << 12) |
          ((str[i + 1] & 0x3F) << 6) |
          (str[i + 2] & 0x3F);
        i += 3;
      } else if ((c >> 3) == 0x1E) {
        codepoint = ((c & 0x07) << 18) |
          ((str[i + 1] & 0x3F) << 12) |
          ((str[i + 2] & 0x3F) << 6) |
          (str[i + 3] & 0x3F);
        i += 4;
      } else {
        i += 1; // Skip invalid byte
      }
      result.push_back(codepoint);
    }
    return result;
  }

  std::string pad_chinese_chars_simple(const std::string &text) {
    auto utf32_chars = utf8_to_utf32_simple(text);
    std::string result;

    for (auto c : utf32_chars) {
      if (_is_chinese_char_simple(c)) {
        result += " ";
        if (c < 0x80) {
          result += static_cast<char>(c);
        } else if (c < 0x800) {
          result += static_cast<char>(0xC0 | (c >> 6));
          result += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
          result += static_cast<char>(0xE0 | (c >> 12));
          result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
          result += static_cast<char>(0x80 | (c & 0x3F));
        } else {
          result += static_cast<char>(0xF0 | (c >> 18));
          result += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
          result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
          result += static_cast<char>(0x80 | (c & 0x3F));
        }
        result += " ";
      } else if (c < 0x80) {
        result += static_cast<char>(c);
      }
    }
    return result;
  }

  std::vector<std::string> split_simple(const std::string &input) {
    std::istringstream stream(input);
    std::vector<std::string> words;
    words.reserve(input.size() / 3);
    std::string word;
    while (stream >> word) {
      words.push_back(word);
    }
    return words;
  }

  void splitOnPunctSimple(const std::string &text, std::vector<std::string> &result) {
    result.reserve(text.length() / 3);
    std::string current;
    for (char c : text) {
      if (_is_punctuation_simple(c)) {
        if (!current.empty()) {
          result.push_back(current);
          current.clear();
        }
        result.emplace_back(1, c);
      } else {
        current += c;
      }
    }
    if (!current.empty()) {
      result.emplace_back(current);
    }
  }

} // anonymous namespace

SimpleTokenCounter::SimpleTokenCounter(const std::string &config_path)
{
  std::ifstream file(config_path);
  if (file.is_open()) {
    json jsonObj;
    file >> jsonObj;
    if (jsonObj.contains("model") && jsonObj["model"].contains("vocab")) {
      vocab_ = jsonObj["model"]["vocab"];
    }
  }
}

size_t SimpleTokenCounter::estimateTokenCount(const std::string &text, bool addSpecialTokens)
{
  std::string padded = pad_chinese_chars_simple(text);
  std::vector<std::string> words = split_simple(padded);

  size_t total_tokens = addSpecialTokens ? 2 : 0; // [CLS] + [SEP]

  for (const auto &word : words) {
    std::vector<std::string> punct_split;
    splitOnPunctSimple(word, punct_split);

    for (const auto &token : punct_split) {
      if (token.empty()) continue;

      if (token.length() <= 4) {
        total_tokens += 1;
      } else if (token.length() <= 8) {
        total_tokens += 2;
      } else {
        total_tokens += (token.length() + 3) / 4;
      }
    }
  }

  return total_tokens;
}

size_t SimpleTokenCounter::countTokensWithVocab(const std::string &text, bool addSpecialTokens)
{
  if (vocab_.empty()) {
    return estimateTokenCount(text);
  }

  std::string padded = pad_chinese_chars_simple(text);
  std::vector<std::string> words = split_simple(padded);

  size_t total_tokens = addSpecialTokens ? 2 : 0; // [CLS] + [SEP]

  for (const auto &word : words) {
    std::vector<std::string> punct_split;
    splitOnPunctSimple(word, punct_split);

    for (const auto &token : punct_split) {
      if (token.empty()) continue;
      total_tokens += simulateWordpiece(token, addSpecialTokens);
    }
  }
  return total_tokens;
}

size_t SimpleTokenCounter::simulateWordpiece(const std::string &word, bool addSpecialTokens)
{
  if (word.length() > maxInputCharsPerWord_) {
    return addSpecialTokens ? 1 : 0; // [UNK]
  }

  auto it = cache_.find(word);
  if (it != cache_.end()) {
    return it->second;
  }

  size_t tokens = 0;
  size_t start = 0;

  while (start < word.length()) {
    size_t best_end = start + 1;

    for (size_t end = word.length(); end > start; --end) {
      std::string substr = word.substr(start, end - start);
      if (start > 0) {
        substr = "##" + substr;
      }

      if (vocab_.contains(substr)) {
        best_end = end;
        break;
      }
    }

    tokens++;
    start = best_end;
  }

  cache_[word] = tokens;

  return tokens;
}
