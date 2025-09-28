#ifndef _CHUNKER_H_
#define _CHUNKER_H_

#include <vector>
#include <string>
#include <regex>
#include <unordered_map>
#include "tokenizer.h"


#if 0
struct Chunk {
  std::string content;
  size_t tokenCount;
  std::string sourceId;
  size_t startPos;
  size_t endPos;
  std::string chunkType;
};
#else
struct Chunk {
  std::string docId;
  std::string chunkId;
  std::string text;
  std::string raw;
  struct {
    size_t tokenCount;
    size_t startChar;
    size_t endChar;
    std::string source;
  } metadata;
};
#endif

class Chunker {
private:
  SimpleTokenCounter &tokenizer_;
  size_t maxTokens_;
  size_t minTokens_;
  size_t overlapTokens_;

  std::vector<std::regex> functionPatterns_;
  std::vector<std::regex> sectionPatterns_;

  struct CodeBlock {
    size_t startLine;
    size_t endLine;
    size_t startPos;
    size_t endPos;
    std::string type;
  };

  std::unordered_map<std::string, size_t> tokenCache_;

public:
  Chunker(SimpleTokenCounter &tok, size_t minTok = 50, size_t maxTok = 500, float overlap = 0.1f);

  std::vector<Chunk> chunkText(const std::string &text, const std::string &sourceId = "", bool semantic = true);

private:
  std::string detectContentType(const std::string &text, const std::string &sourceId);
  std::vector<Chunk> postProcessChunks(std::vector<Chunk> &chunks);
  size_t tokenCount(const std::string &text);
  std::vector<Chunk> splitIntoChunksAdv(std::string text, const std::string &docId);
  std::vector<Chunk> splitIntoLineChunks(const std::string &text, const std::string &docId);
  std::vector<std::string> splitIntoLines(const std::string &text);

public:
  static std::string normalizeWhitespaces(const std::string &str);
  static std::string cleanTextForEmbedding(const std::string &text, const std::string &EMBED_PREPEND_PHRASE = "");
};



#endif // _CHUNKER_H_