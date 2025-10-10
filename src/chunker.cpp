#include "chunker.h"
#include <sstream>
#include <algorithm>
#include <string>

namespace {
  struct Unit {
    std::string text;
    size_t tokens;
    size_t startChar;
    size_t endChar;
  };

  //std::vector<std::string> splitUnits(const std::string &text) {
  //  std::regex re(R"((\s+|[\p{Punct}]))");
  //  std::sregex_token_iterator it(text.begin(), text.end(), re, { -1, 0 });
  //  std::sregex_token_iterator end;
  //  std::vector<std::string> result;
  //  for (; it != end; ++it) {
  //    if (!it->str().empty()) result.push_back(it->str());
  //  }
  //  return result;
  //}
  std::vector<std::string> splitUnits(const std::string &text) {
    std::vector<std::string> result;
    std::string buf;
    auto flushBuf = [&]() {
      if (!buf.empty()) {
        result.push_back(buf);
        buf.clear();
      }
      };
    for (unsigned char c : text) {
      if (std::isspace(c)) {
        flushBuf();
        // group consecutive whitespace into one unit
        if (!result.empty() && std::all_of(result.back().begin(), result.back().end(),
          [](unsigned char x) { return std::isspace(x); })) {
          result.back().push_back(c);
        } else {
          result.emplace_back(1, c);
        }
      } else if (std::ispunct(c)) {
        flushBuf();
        // punctuation = its own unit
        result.emplace_back(1, c);
      } else {
        buf.push_back(c);
      }
    }
    flushBuf();
    return result;
  }


} // anonymous namespace


Chunker::Chunker(SimpleTokenCounter &tok, size_t min_tok, size_t max_tok, float overlap)
  : tokenizer_(tok), minTokens_(min_tok), maxTokens_(max_tok), overlapTokens_(static_cast<size_t>(max_tok * overlap))
{
#if 0
  functionPatterns_ = {
      std::regex(R"(^\s*(public|private|protected|static|inline|virtual)?\s*\w+[\s\*&]*\w+\s*\([^)]*\)\s*\{)"),
      std::regex(R"(^\s*def\s+\w+\s*\([^)]*\):)"),
      std::regex(R"(^\s*function\s+\w+\s*\([^)]*\)\s*\{)"),
      std::regex(R"(^\s*class\s+\w+)")
  };
  sectionPatterns_ = {
      std::regex(R"(^#{1,6}\s+.+$)"),
      std::regex(R"(^\/\*\*[\s\S]*?\*\/$)"),
      std::regex(R"(^\/\/.*$)")
  };
#endif
}

std::vector<Chunk> Chunker::chunkText(const std::string &text, const std::string &uri, bool semantic) const
{
  std::vector<Chunk> chunks;
  std::string chunkType = detectContentType(text, uri);
  if (chunkType == "code") {
    chunks = splitIntoLineChunks(text, uri);
  } else {
    chunks = splitIntoChunksAdv(text, uri);
  }
  return postProcessChunks(chunks, chunkType);
}

std::string Chunker::detectContentType(const std::string &text, const std::string &uri)
{
  if (uri.ends_with(".cpp") || uri.ends_with(".h") ||
    uri.ends_with(".hpp") || uri.ends_with(".c") ||
    uri.ends_with(".py") || uri.ends_with(".js") ||
    uri.ends_with(".java") || uri.ends_with(".cs")) {
    return "code";
  }
  if (uri.ends_with(".md") || uri.ends_with(".txt")) {
    return "text";
  }
  size_t codeIndicators = 0, totalLines = 0;
  std::stringstream ss(text);
  std::string line;
  while (std::getline(ss, line)) {
    totalLines++;
    if (
      line.find("class ") != std::string::npos ||
      line.find("struct ") != std::string::npos ||
      line.find("string ") != std::string::npos ||
      line.find("float ") != std::string::npos ||
      line.find("double ") != std::string::npos ||
      line.find(" std::vector<") != std::string::npos ||
      line.find("def ") != std::string::npos ||
      line.find("function ") != std::string::npos ||
      line.find("#include") != std::string::npos ||
      line.find("import ") != std::string::npos ||
      line.find("macro ") != std::string::npos ||
      line.find("endmacro ") != std::string::npos ||
      std::count(line.begin(), line.end(), '{') > 0 ||
      std::count(line.begin(), line.end(), ';') > 1) {
      codeIndicators++;
    }
  }
  auto metric = totalLines * 0.3;
  if (totalLines < 3) {
    metric = text.length() * 0.3 * 0.3;
  }
  return (metric < codeIndicators) ? "code" : "text";
}

std::vector<Chunk> Chunker::postProcessChunks(std::vector<Chunk> &chunks, const std::string &chunkType) const
{
  std::vector<Chunk> processed;
  for (size_t i = 0; i < chunks.size(); ++i) {
    Chunk &chunk = chunks[i];
    chunk.metadata.type = chunkType;
    if (chunk.metadata.tokenCount < minTokens_ && i + 1 < chunks.size()) {
      Chunk &next_chunk = chunks[i + 1];
      size_t combined_tokens = tokenCount(chunk.text + next_chunk.text);
      if (combined_tokens <= maxTokens_ && chunk.docUri == next_chunk.docUri) {
        chunk.text += next_chunk.text;
        chunk.metadata.tokenCount = combined_tokens;
        chunk.metadata.end = next_chunk.metadata.end;
        ++i;
      }
    }
    processed.push_back(chunk);
  }
  return processed;
}

size_t Chunker::tokenCount(const std::string &text) const
{
  auto it = tokenCache_.find(text);
  if (it != tokenCache_.end()) {
    return it->second;
  }
  size_t count = tokenizer_.countTokensWithVocab(text);
  tokenCache_[text] = count;
  return count;
}

std::vector<Chunk> Chunker::splitIntoChunksAdv(std::string text, const std::string &uri) const
{
  auto overlap = overlapTokens_;
  if (maxTokens_ * 0.6 < overlap) overlap = static_cast<size_t>(maxTokens_ * 0.6);
  text = normalizeWhitespaces(text);
  auto rawUnits = splitUnits(text);
  std::vector<Unit> units;
  size_t charPos = 0;
  for (auto &uText : rawUnits) {
    size_t tks = tokenCount(uText);
    units.push_back({ uText, tks, charPos, charPos + uText.size() });
    charPos += uText.size();
  }
  std::vector<Chunk> chunks;
  size_t chunkId = 0;
  size_t start = 0;
  while (start < units.size()) {
    size_t tokenCnt = 0;
    size_t end = start;
    while (end < units.size() &&
      tokenCnt + units[end].tokens <= maxTokens_) {
      tokenCnt += units[end].tokens;
      end++;
    }
    if (start < end) {
      size_t startChar = units[start].startChar;
      size_t endChar = units[end - 1].endChar;
      std::string raw = text.substr(startChar, endChar - startChar);
      std::string chunkText;
      for (size_t i = start; i < end; i++) chunkText += units[i].text;
      size_t tokensToCheck = tokenCount(raw);
      chunks.push_back({
          uri,
          uri + "_" + std::to_string(chunkId++),
          chunkText,
          raw,
          {tokenCnt, startChar, endChar, "char"}
        });
    }
    if (end >= units.size()) break;
    if (overlap > 0) {
      size_t overlapTokens = 0;
      size_t overlapUnits = 0;
      while (end - 1 - overlapUnits >= 0 && overlapTokens < overlap) {
        overlapTokens += units[end - 1 - overlapUnits].tokens;
        overlapUnits++;
      }
      start = end - overlapUnits;
    } else {
      start = end;
    }
  }
  return chunks;
}

std::vector<Chunk> Chunker::splitIntoLineChunks(const std::string &text, const std::string &uri) const
{
  std::vector<Chunk> chunks;
  std::vector<std::string> lines;
  {
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
      auto subLines = splitIntoLines(line);
      lines.insert(lines.end(), subLines.begin(), subLines.end());
    }
  }
  size_t chunkId = 0;
  size_t start = 0;
  while (start < lines.size()) {
    size_t tokenCnt = 0;
    size_t end = start;
    std::string chunkText;
    // Accumulate lines until token budget exceeded
    while (end < lines.size()) {
      auto lineTokens = tokenCount(lines[end]);
      if (tokenCnt + lineTokens > maxTokens_) break;
      tokenCnt += lineTokens;
      chunkText += lines[end];
      end++;
    }
    if (start < end) {
      std::string raw;
      for (size_t i = start; i < end; i++) raw += lines[i];
      chunks.push_back({
          uri,
          uri + "_" + std::to_string(chunkId++),
          chunkText,
          raw,
          {tokenCnt, start, end, "line"}
        });
    }
    if (lines.size() <= end) break;
    if (0 < overlapTokens_) {
      size_t overlapTokens = 0;
      size_t overlapLines = 0;
      while (end - 1 - overlapLines >= 0 && overlapTokens < overlapTokens_) {
        overlapTokens += tokenCount(lines[end - 1 - overlapLines]);
        overlapLines++;
      }
      start = end - overlapLines;
    } else {
      start = end;
    }
  }
  return chunks;
}

std::string Chunker::normalizeWhitespaces(const std::string &str)
{
  auto start = str.find_first_not_of(" \t\r\n");
  auto end = str.find_last_not_of(" \t\r\n");
  std::string s = (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
  // collapse whitespace except newlines into single space
  s = std::regex_replace(s, std::regex{ "[^\\S\n]+" }, " ");
  // collapse multiple newlines
  s = std::regex_replace(s, std::regex{ "\n\\s*\n" }, "\n");
  return s;
}

#if 0
std::string Chunker::cleanTextForEmbedding(const std::string &text, const std::string &EMBED_PREPEND_PHRASE)
{
  std::string prepend = EMBED_PREPEND_PHRASE.empty() ? "" : (std::regex_replace(EMBED_PREPEND_PHRASE, std::regex{ "^\\s+|\\s+$" }, "") + " ");
  std::string s = prepend + normalizeWhitespaces(text);

  // Convert \\n -> newline
  s = std::regex_replace(s, std::regex{ "\\\\n" }, "\n");
  // Handle other double backslashes (not before n)
  s = std::regex_replace(s, std::regex{ "\\\\(?!n)" }, "\\\\");
  // Remove non-ASCII
  s.erase(std::remove_if(s.begin(), s.end(),
    [](unsigned char c) { return c > 127; }),
    s.end());
  // Trim & cut to 2000 chars
  auto start = s.find_first_not_of(" \t\r\n");
  auto end = s.find_last_not_of(" \t\r\n");
  s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
  if (s.size() > 2000) s.resize(2000);
  return s;
}
#endif

std::vector<std::string> Chunker::splitIntoLines(const std::string &text) const
{
  std::vector<std::string> subs;
  auto nTokens = tokenCount(text);
  if (nTokens <= maxTokens_) {
    auto s{ text };
    if (!s.ends_with('\n')) s += '\n';
    return { s };
  }
  auto units = splitUnits(text);
  std::string current;
  size_t currentTokens = 0;
  for (const auto &u : units) {
    size_t uTokens = tokenCount(u);
    if (maxTokens_ < currentTokens + uTokens && !current.empty()) {
      if (!current.ends_with('\n')) current += '\n';
      subs.push_back(current);
      current.clear();
      currentTokens = 0;
    }
    current += u;
    currentTokens += uTokens;
  }
  if (!current.empty()) subs.push_back(current);
  return subs;
}
