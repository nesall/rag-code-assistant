#include "database.h"
#include <hnswlib/hnswlib.h>
#include <sqlite3.h>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <filesystem>
#include <format>


struct HnswSqliteVectorDatabase::Impl {
  std::unique_ptr<hnswlib::HierarchicalNSW<float>> index_;
  std::unique_ptr<hnswlib::L2Space> space_;
  sqlite3 *db_ = nullptr;

  size_t vectorDim_ = 0;
  size_t maxElements_ = 0;
  size_t currentCount_ = 0;
  std::string dbPath_;
  std::string indexPath_;


};

HnswSqliteVectorDatabase::HnswSqliteVectorDatabase(const std::string &dbPath, const std::string &indexPath, size_t vectorDim, size_t maxElements)
  : imp(new Impl)
{
  imp->dbPath_ = dbPath;
  imp->indexPath_ = indexPath;
  imp->vectorDim_ = vectorDim;
  imp->maxElements_ = maxElements;

  initializeDatabase();
  initializeVectorIndex();
}

HnswSqliteVectorDatabase::~HnswSqliteVectorDatabase() {
  if (imp->db_) {
    sqlite3_close(imp->db_);
  }
}

size_t HnswSqliteVectorDatabase::addDocument(const Chunk &chunk, const std::vector<float> &embedding)
{
  if (embedding.size() != imp->vectorDim_) {
    throw std::runtime_error(std::format("Embedding dimension mismatch: actual {}, claimed {}", embedding.size(), imp->vectorDim_));
  }
  size_t chunkId = insertMetadata(chunk);
  imp->index_->addPoint(embedding.data(), chunkId);
  imp->currentCount_++;
  return chunkId;
}

std::vector<size_t> HnswSqliteVectorDatabase::addDocuments(const std::vector<Chunk> &chunks,
  const std::vector<std::vector<float>> &embeddings)
{
  if (chunks.size() != embeddings.size()) {
    throw std::runtime_error("Chunks and embeddings count mismatch");
  }
  std::vector<size_t> chunkIds;
  executeSql("BEGIN TRANSACTION");
  try {
    for (size_t i = 0; i < chunks.size(); ++i) {
      size_t id = addDocument(chunks[i], embeddings[i]);
      chunkIds.push_back(id);
    }
    executeSql("COMMIT");
  } catch (...) {
    executeSql("ROLLBACK");
    throw;
  }

  return chunkIds;
}

std::vector<SearchResult> HnswSqliteVectorDatabase::search(const std::vector<float> &queryEmbedding, size_t topK)
{
  if (queryEmbedding.size() != imp->vectorDim_) {
    throw std::runtime_error(std::format("Query embedding dimension mismatch: actual {}, claimed {}", queryEmbedding.size(), imp->vectorDim_));
  }
  if (imp->currentCount_ == 0) {
    return {};
  }
  auto result = imp->index_->searchKnn(queryEmbedding.data(), topK);
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

std::vector<SearchResult> HnswSqliteVectorDatabase::searchWithFilter(const std::vector<float> &queryEmbedding,
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

void HnswSqliteVectorDatabase::clear()
{
  executeSql("DELETE FROM chunks");
  imp->space_ = std::make_unique<hnswlib::L2Space>(imp->vectorDim_);
  imp->index_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(imp->space_.get(), imp->maxElements_);
  imp->currentCount_ = 0;
}

void HnswSqliteVectorDatabase::initializeDatabase()
{
  int rc = sqlite3_open(imp->dbPath_.c_str(), &imp->db_);
  if (rc != SQLITE_OK) {
    throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(imp->db_)));
  }
  const char *chunksTable = R"(
        CREATE TABLE IF NOT EXISTS chunks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            content TEXT NOT NULL,
            source_id TEXT NOT NULL,
            start_pos INTEGER NOT NULL,
            end_pos INTEGER NOT NULL,
            token_count INTEGER NOT NULL,
            unit TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
  executeSql(chunksTable);

  const char *filesTable = R"(
        CREATE TABLE IF NOT EXISTS files_metadata (
            path TEXT PRIMARY KEY,
            last_modified INTEGER NOT NULL,
            file_size INTEGER NOT NULL,
            indexed_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
  executeSql(filesTable);
}

void HnswSqliteVectorDatabase::initializeVectorIndex()
{
  imp->space_ = std::make_unique<hnswlib::L2Space>(imp->vectorDim_);
  if (std::filesystem::exists(imp->indexPath_)) {
    try {
      imp->index_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(imp->space_.get(), imp->indexPath_);
      imp->currentCount_ = imp->index_->getCurrentElementCount();
      std::cout << "Loaded existing index with " << imp->currentCount_ << " vectors" << std::endl;
      return;
    } catch (const std::exception &e) {
      std::cerr << "Failed to load existing index: " << e.what() << std::endl;
      std::cerr << "Creating new index..." << std::endl;
    }
  }
  imp->index_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(imp->space_.get(), imp->maxElements_);
  imp->currentCount_ = 0;
}

void HnswSqliteVectorDatabase::executeSql(const std::string &sql)
{
  char *errorMessage = nullptr;
  int rc = sqlite3_exec(imp->db_, sql.c_str(), nullptr, nullptr, &errorMessage);
  if (rc != SQLITE_OK) {
    std::string error = errorMessage ? errorMessage : "Unknown error";
    if (errorMessage) sqlite3_free(errorMessage);
    throw std::runtime_error("SQL error: " + error);
  }
}

size_t HnswSqliteVectorDatabase::insertMetadata(const Chunk &chunk)
{
  const char *insertSql = R"(
        INSERT INTO chunks (content, source_id, start_pos, end_pos, token_count, unit)
        VALUES (?, ?, ?, ?, ?, ?)
    )";

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(imp->db_, insertSql, -1, &stmt, nullptr);
  int k = 1;
  sqlite3_bind_text(stmt, k++, chunk.text.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, k++, chunk.docUri.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, k++, chunk.metadata.start);
  sqlite3_bind_int64(stmt, k++, chunk.metadata.end);
  sqlite3_bind_int64(stmt, k++, chunk.metadata.tokenCount);
  sqlite3_bind_text(stmt, k++, chunk.metadata.unit.c_str(), -1, SQLITE_STATIC);
  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to insert chunk metadata");
  }
  size_t chunkId = sqlite3_last_insert_rowid(imp->db_);
  sqlite3_finalize(stmt);
  return chunkId;
}

std::optional<SearchResult> HnswSqliteVectorDatabase::getChunkMetadata(size_t chunkId)
{
  const char *selectSql = R"(
        SELECT content, source_id, start_pos, end_pos 
        FROM chunks WHERE id = ?
    )";
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(imp->db_, selectSql, -1, &stmt, nullptr);
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

size_t HnswSqliteVectorDatabase::deleteDocumentsBySource(const std::string &sourceId)
{
  sqlite3_stmt *stmt;
  const char *sql = "DELETE FROM chunks WHERE source_id = ?";
  sqlite3_prepare_v2(imp->db_, sql, -1, &stmt, nullptr);

  sqlite3_bind_text(stmt, 1, sourceId.c_str(), -1, SQLITE_STATIC);

  int result = sqlite3_step(stmt);
  size_t n = sqlite3_changes(imp->db_);

  sqlite3_finalize(stmt);
  sqlite3_close(imp->db_);

  return n;
  // Note: Vector index still contains these vectors
  // They become "dangling" - queries won't fail but waste space
  // Solution: Mark as deleted or rebuild index periodically
}


void HnswSqliteVectorDatabase::removeFileMetadata(const std::string &filepath)
{
  sqlite3_stmt *stmt;
  const char *sql = "DELETE FROM files_metadata WHERE path = ?";
  sqlite3_prepare_v2(imp->db_, sql, -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, filepath.c_str(), -1, SQLITE_STATIC);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void HnswSqliteVectorDatabase::upsertFileMetadata(const std::string &filepath, std::time_t mtime, size_t size)
{
  sqlite3_stmt *stmt;
  const char *sql = "INSERT OR REPLACE INTO files_metadata (path, last_modified, file_size) VALUES (?, ?, ?)";
  sqlite3_prepare_v2(imp->db_, sql, -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, filepath.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 2, mtime);
  sqlite3_bind_int64(stmt, 3, size);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

std::vector<FileMetadata> HnswSqliteVectorDatabase::getTrackedFiles() const
{
  std::vector<FileMetadata> files;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT path, last_modified, file_size FROM files_metadata";
  sqlite3_prepare_v2(imp->db_, sql, -1, &stmt, nullptr);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    FileMetadata meta;
    meta.path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    meta.lastModified = sqlite3_column_int64(stmt, 1);
    meta.fileSize = sqlite3_column_int64(stmt, 2);
    files.push_back(meta);
  }
  sqlite3_finalize(stmt);
  return files;
}

DatabaseStats HnswSqliteVectorDatabase::getStats() const
{
  DatabaseStats stats;
  stats.vectorCount = imp->currentCount_;
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(imp->db_, "SELECT COUNT(*) FROM chunks", -1, &stmt, nullptr);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    stats.totalChunks = sqlite3_column_int64(stmt, 0);
  }
  sqlite3_finalize(stmt);
  sqlite3_prepare_v2(imp->db_, "SELECT source_id, COUNT(*) FROM chunks GROUP BY source_id", -1, &stmt, nullptr);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string source = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    size_t count = sqlite3_column_int64(stmt, 1);
    stats.sources.emplace_back(source, count);
  }
  sqlite3_finalize(stmt);
  return stats;
}

void HnswSqliteVectorDatabase::persist()
{
  if (imp->currentCount_ > 0) {
    imp->index_->saveIndex(imp->indexPath_);
    std::cout << "Saved vector index with " << imp->currentCount_ << " vectors" << std::endl;
  }
}

std::string HnswSqliteVectorDatabase::dbPath() const
{
  return imp->dbPath_;
}

std::string HnswSqliteVectorDatabase::indexPath() const
{
  return imp->indexPath_;
}
