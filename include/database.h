#pragma once

#include <hnswlib/hnswlib.h>
#include <sqlite3.h>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <optional>
#include <filesystem>
#include "chunker.h" // Defines Chunk

struct SearchResult {
  std::string content;
  std::string sourceId;
  std::string chunkType;
  size_t startPos;
  size_t endPos;
  float similarityScore;
  size_t chunkId;
};

class VectorDatabase {
public:
  struct DatabaseStats {
    size_t totalChunks;
    size_t vectorCount;
    std::vector<std::pair<std::string, size_t>> sources;
    std::vector<std::pair<std::string, size_t>> types;
  };

  VectorDatabase(const std::string &dbPath, const std::string &indexPath, size_t vectorDim, size_t maxElements = 100000);
  ~VectorDatabase();

  size_t insertChunk(const Chunk &chunk, const std::vector<float> &embedding);
  std::vector<size_t> insertChunks(const std::vector<Chunk> &chunks, const std::vector<std::vector<float>> &embeddings);
  std::vector<SearchResult> search(const std::vector<float> &queryEmbedding, size_t topK = 10);
  std::vector<SearchResult> searchWithFilter(const std::vector<float> &queryEmbedding,
    const std::string &sourceFilter = "",
    const std::string &typeFilter = "",
    size_t topK = 10);
  void saveIndex();
  DatabaseStats getStats();
  void clear();

private:
  std::unique_ptr<hnswlib::HierarchicalNSW<float>> index_;
  std::unique_ptr<hnswlib::L2Space> space_;
  sqlite3 *db_;

  size_t vectorDim_;
  size_t maxElements_;
  size_t currentCount_;
  std::string dbPath_;
  std::string indexPath_;

  void initializeDatabase();
  void initializeVectorIndex();
  void executeSql(const std::string &sql);
  size_t insertMetadata(const Chunk &chunk);
  std::optional<SearchResult> getChunkMetadata(size_t chunkId);
};
