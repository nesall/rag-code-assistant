#pragma once

#include "chunker.h"
#include <vector>
#include <string>
#include <memory>
#include <optional>


struct SearchResult {
  std::string content;
  std::string sourceId;
  std::string chunkType;
  size_t startPos = 0;
  size_t endPos = 0;
  float similarityScore = 0;
  size_t chunkId = 0;
};


struct FileMetadata {
  std::string path;
  time_t lastModified = 0;
  size_t fileSize = 0;
  std::string hash; // Optional: content hash for change detection
};


struct DatabaseStats {
  size_t totalChunks = 0;
  size_t vectorCount = 0;
  size_t totalTokens = 0;
  std::vector<std::pair<std::string, size_t>> sources;
};


class VectorDatabase {
public:
  virtual ~VectorDatabase() = default;

  virtual size_t addDocument(const Chunk &chunk, const std::vector<float> &embedding) = 0;
  virtual std::vector<size_t> addDocuments(const std::vector<Chunk> &chunks, const std::vector<std::vector<float>> &embeddings) = 0;

  virtual std::vector<SearchResult> search(const std::vector<float> &query, size_t top_k = 10) = 0;
  virtual std::vector<SearchResult> searchWithFilter(const std::vector<float> &query,
    const std::string &sourceFilter = "",
    const std::string &typeFilter = "",
    size_t top_k = 10) = 0;

  virtual size_t deleteDocumentsBySource(const std::string &source_id) = 0;
  virtual void clear() = 0;

  virtual std::vector<FileMetadata> getTrackedFiles() const = 0;
  virtual void upsertFileMetadata(const std::string &path, std::time_t mtime, size_t size) = 0;
  virtual void removeFileMetadata(const std::string &path) = 0;

  virtual DatabaseStats getStats() const = 0;
  virtual void persist() = 0;

  virtual void beginTransaction() = 0;
  virtual void commit() = 0;
  virtual void rollback() = 0;
};


class HnswSqliteVectorDatabase : public VectorDatabase {
public:
  HnswSqliteVectorDatabase(const std::string &dbPath, const std::string &indexPath, size_t vectorDim, size_t maxElements = 100000);
  ~HnswSqliteVectorDatabase();

  size_t addDocument(const Chunk &chunk, const std::vector<float> &embedding) override;
  std::vector<size_t> addDocuments(const std::vector<Chunk> &chunks, const std::vector<std::vector<float>> &embeddings) override;
  std::vector<SearchResult> search(const std::vector<float> &queryEmbedding, size_t topK = 10) override;
  std::vector<SearchResult> searchWithFilter(const std::vector<float> &queryEmbedding,
    const std::string &sourceFilter = "",
    const std::string &typeFilter = "",
    size_t topK = 10) override;
  DatabaseStats getStats() const override;
  void clear() override;

  size_t deleteDocumentsBySource(const std::string &sourceId) override;
  void removeFileMetadata(const std::string &sourceId) override;
  void upsertFileMetadata(const std::string &sourceId, std::time_t mtime, size_t size) override;
  std::vector<FileMetadata> getTrackedFiles() const override;

  void beginTransaction() override { executeSql("BEGIN TRANSACTION"); }
  void commit() override { executeSql("COMMIT"); }
  void rollback() override { executeSql("ROLLBACK"); }

  void persist() override;

private:
  std::string dbPath() const;
  std::string indexPath() const;

private:
  struct Impl;
  std::unique_ptr<Impl> imp;

  void initializeDatabase();
  void initializeVectorIndex();
  void executeSql(const std::string &sql);
  size_t insertMetadata(const Chunk &chunk);
  std::optional<SearchResult> getChunkMetadata(size_t chunkId);
};
