#include "database.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <filesystem>
#include <format>

VectorDatabase::VectorDatabase(const std::string &dbPath, const std::string &indexPath, size_t vectorDim, size_t maxElements)
  : db_(nullptr), vectorDim_(vectorDim), maxElements_(maxElements), currentCount_(0), dbPath_(dbPath), indexPath_(indexPath)
{
  initializeDatabase();
  initializeVectorIndex();
}

VectorDatabase::~VectorDatabase() {
  if (db_) {
    sqlite3_close(db_);
  }
}

size_t VectorDatabase::insertChunk(const Chunk &chunk, const std::vector<float> &embedding)
{
  if (embedding.size() != vectorDim_) {
    throw std::runtime_error(std::format("Embedding dimension mismatch: actual {}, claimed {}", embedding.size(), vectorDim_));
  }
  size_t chunkId = insertMetadata(chunk);
  index_->addPoint(embedding.data(), chunkId);
  currentCount_++;
  return chunkId;
}

std::vector<size_t> VectorDatabase::insertChunks(const std::vector<Chunk> &chunks,
  const std::vector<std::vector<float>> &embeddings)
{
  if (chunks.size() != embeddings.size()) {
    throw std::runtime_error("Chunks and embeddings count mismatch");
  }
  std::vector<size_t> chunkIds;
  executeSql("BEGIN TRANSACTION");
  try {
    for (size_t i = 0; i < chunks.size(); ++i) {
      size_t id = insertChunk(chunks[i], embeddings[i]);
      chunkIds.push_back(id);
    }
    executeSql("COMMIT");
  } catch (...) {
    executeSql("ROLLBACK");
    throw;
  }

  return chunkIds;
}

std::vector<SearchResult> VectorDatabase::search(const std::vector<float> &queryEmbedding, size_t topK)
{
  if (queryEmbedding.size() != vectorDim_) {
    throw std::runtime_error(std::format("Query embedding dimension mismatch: actual {}, claimed {}", queryEmbedding.size(), vectorDim_));
  }
  if (currentCount_ == 0) {
    return {};
  }
  auto result = index_->searchKnn(queryEmbedding.data(), topK);
  std::vector<SearchResult> searchResults;
  while (!result.empty()) {
    auto [distance, label] = result.top();
    result.pop();

    float similarity = 1.0f / (1.0f + distance);
    auto chunkData = getChunkMetadata(label);
    if (chunkData.has_value()) {
      SearchResult sr = chunkData.value();
      sr.similarityScore = similarity;
      sr.chunkId = label;
      searchResults.push_back(sr);
    }
  }

  std::reverse(searchResults.begin(), searchResults.end());
  return searchResults;
}

std::vector<SearchResult> VectorDatabase::searchWithFilter(const std::vector<float> &queryEmbedding,
  const std::string &sourceFilter,
  const std::string &typeFilter,
  size_t topK)
{
  auto results = search(queryEmbedding, topK * 2);
  std::vector<SearchResult> filtered;
  for (const auto &result : results) {
    bool matches = true;
    if (!sourceFilter.empty() && result.sourceId.find(sourceFilter) == std::string::npos) {
      matches = false;
    }
    if (!typeFilter.empty() && result.chunkType != typeFilter) {
      matches = false;
    }
    if (matches) {
      filtered.push_back(result);
      if (filtered.size() >= topK) break;
    }
  }
  return filtered;
}

void VectorDatabase::saveIndex()
{
  if (currentCount_ > 0) {
    index_->saveIndex(indexPath_);
    std::cout << "Saved vector index with " << currentCount_ << " vectors" << std::endl;
  }
}

VectorDatabase::DatabaseStats VectorDatabase::getStats()
{
  DatabaseStats stats;
  stats.vectorCount = currentCount_;
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM chunks", -1, &stmt, nullptr);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    stats.totalChunks = sqlite3_column_int64(stmt, 0);
  }
  sqlite3_finalize(stmt);
  sqlite3_prepare_v2(db_, "SELECT source_id, COUNT(*) FROM chunks GROUP BY source_id", -1, &stmt, nullptr);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string source = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    size_t count = sqlite3_column_int64(stmt, 1);
    stats.sources.emplace_back(source, count);
  }
  sqlite3_finalize(stmt);
  sqlite3_prepare_v2(db_, "SELECT chunk_type, COUNT(*) FROM chunks GROUP BY chunk_type", -1, &stmt, nullptr);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string type = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    size_t count = sqlite3_column_int64(stmt, 1);
    stats.types.emplace_back(type, count);
  }
  sqlite3_finalize(stmt);
  return stats;
}

void VectorDatabase::clear()
{
  executeSql("DELETE FROM chunks");
  space_ = std::make_unique<hnswlib::L2Space>(vectorDim_);
  index_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(space_.get(), maxElements_);
  currentCount_ = 0;
}

void VectorDatabase::initializeDatabase()
{
  int rc = sqlite3_open(dbPath_.c_str(), &db_);
  if (rc != SQLITE_OK) {
    throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db_)));
  }
  const char *createTableSql = R"(
        CREATE TABLE IF NOT EXISTS chunks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            content TEXT NOT NULL,
            source_id TEXT NOT NULL,
            chunk_type TEXT NOT NULL,
            start_pos INTEGER NOT NULL,
            end_pos INTEGER NOT NULL,
            token_count INTEGER NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
  executeSql(createTableSql);
}

void VectorDatabase::initializeVectorIndex()
{
  space_ = std::make_unique<hnswlib::L2Space>(vectorDim_);
  if (std::filesystem::exists(indexPath_)) {
    try {
      index_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(space_.get(), indexPath_);
      currentCount_ = index_->getCurrentElementCount();
      std::cout << "Loaded existing index with " << currentCount_ << " vectors" << std::endl;
      return;
    } catch (const std::exception &e) {
      std::cerr << "Failed to load existing index: " << e.what() << std::endl;
      std::cerr << "Creating new index..." << std::endl;
    }
  }
  index_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(space_.get(), maxElements_);
  currentCount_ = 0;
}

void VectorDatabase::executeSql(const std::string &sql)
{
  char *errorMessage = nullptr;
  int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errorMessage);
  if (rc != SQLITE_OK) {
    std::string error = errorMessage ? errorMessage : "Unknown error";
    if (errorMessage) sqlite3_free(errorMessage);
    throw std::runtime_error("SQL error: " + error);
  }
}

size_t VectorDatabase::insertMetadata(const Chunk &chunk)
{
  const char *insertSql = R"(
        INSERT INTO chunks (content, source_id, chunk_type, start_pos, end_pos, token_count)
        VALUES (?, ?, ?, ?, ?, ?)
    )";

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db_, insertSql, -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, chunk.text.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, chunk.docId.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, chunk.metadata.source.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 4, chunk.metadata.startChar);
  sqlite3_bind_int64(stmt, 5, chunk.metadata.endChar);
  sqlite3_bind_int64(stmt, 6, chunk.metadata.tokenCount);
  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to insert chunk metadata");
  }
  size_t chunkId = sqlite3_last_insert_rowid(db_);
  sqlite3_finalize(stmt);
  return chunkId;
}

std::optional<SearchResult> VectorDatabase::getChunkMetadata(size_t chunkId)
{
  const char *selectSql = R"(
        SELECT content, source_id, chunk_type, start_pos, end_pos 
        FROM chunks WHERE id = ?
    )";
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db_, selectSql, -1, &stmt, nullptr);
  sqlite3_bind_int64(stmt, 1, chunkId);
  SearchResult result;
  bool found = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    result.content = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    result.sourceId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    result.chunkType = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    result.startPos = sqlite3_column_int64(stmt, 3);
    result.endPos = sqlite3_column_int64(stmt, 4);
    found = true;
  }
  sqlite3_finalize(stmt);
  return found ? std::optional<SearchResult>(result) : std::nullopt;
}
