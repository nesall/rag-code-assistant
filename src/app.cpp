#include "app.h"
#include "settings.h"
#include "database.h"
#include "embedder.h"
#include "chunker.h"
#include "tokenizer.h"
#include "sourceproc.h"
#include <httplib.h>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <atomic>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;


struct App::Impl {
  std::unique_ptr<Settings> settings_;
  std::unique_ptr<VectorDatabase> db_;
  std::unique_ptr<EmbeddingClient> embeddingClient_;
  std::unique_ptr<SimpleTokenCounter> tokenizer_;
  std::unique_ptr<Chunker> chunker_;
  std::unique_ptr<SourceProcessor> processor_;
  //json config_;
  std::atomic<bool> serverRunning_{ false };
};

App::App(const std::string &configPath) : imp(new Impl)
{
  imp->settings_ = std::make_unique<Settings>(configPath);
  initialize();
}

App::~App()
{
}

void App::embed()
{
  std::cout << "Starting embedding process..." << std::endl;

  auto sources = imp->processor_->getSources();
  size_t totalChunks = 0;
  size_t totalFiles = 0;
  size_t totalTokens = 0;

  for (size_t i = 0; i < sources.size(); ++i) {
    const auto &[text, source] = sources[i];
    try {
      std::cout << "PROCESSING " << source << std::endl;
      std::string sourceId = std::filesystem::path(source).filename().string() + "_" + std::to_string(i + 1);
      auto chunks = imp->chunker_->chunkText(text, sourceId);
      std::cout << "  Generated " << chunks.size() << " chunks" << std::endl;
      size_t batchSize = imp->settings_->embeddingBatchSize();
      for (size_t i = 0; i < chunks.size(); i += batchSize) {
        size_t end = (std::min)(i + batchSize, chunks.size());
        std::vector<Chunk> batch(chunks.begin() + i, chunks.begin() + end);
        std::vector<std::vector<float>> embeddings;
        size_t iBatch = 1;
        for (const auto &chunk : batch) {
          std::cout << "GENERATING embeddings for batch " << iBatch++ << "/" << batch.size() << "\r" << std::flush;
          std::vector<float> emb;
          imp->embeddingClient_->generateEmbeddings({ chunk.text }, emb);
          embeddings.push_back(emb);
          totalTokens += chunk.metadata.tokenCount;
        }
        imp->db_->insertChunks(batch, embeddings);
        std::cout << "  Processed all chunks.                     \r" << std::flush;
      }
      std::cout << std::endl;
      totalChunks += chunks.size();
      totalFiles++;
    } catch (const std::exception &e) {
      std::cerr << "Error processing " << source << ": " << e.what() << std::endl;
    }
  }
  imp->db_->saveIndex();
  std::cout << "\nCompleted!" << std::endl;
  std::cout << "  Files processed: " << totalFiles << std::endl;
  std::cout << "  Total chunks: " << totalChunks << std::endl;
  std::cout << "  Total tokens: " << totalTokens << std::endl;
}

void App::search(const std::string &query, size_t topK)
{
  std::cout << "Searching for: " << query << std::endl;

  std::vector<float> queryEmbedding;
  imp->embeddingClient_->generateEmbeddings({ query }, queryEmbedding);
  auto results = imp->db_->search(queryEmbedding, topK);

  std::cout << "\nFound " << results.size() << " results:" << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  for (size_t i = 0; i < results.size(); ++i) {
    const auto &result = results[i];
    std::cout << "\n[" << (i + 1) << "] Score: " << result.similarityScore << std::endl;
    std::cout << "Source: " << result.sourceId << std::endl;
    std::cout << "Type: " << result.chunkType << std::endl;
    std::cout << "Content: " << result.content.substr(0, 200);
    if (result.content.length() > 200) std::cout << "...";
    std::cout << std::endl;
  }
}

void App::stats()
{
  auto s = imp->db_->getStats();

  std::cout << "\n=== Database Statistics ===" << std::endl;
  std::cout << "Total chunks: " << s.totalChunks << std::endl;
  std::cout << "Vectors in index: " << s.vectorCount << std::endl;

  std::cout << "\nChunks by source:" << std::endl;
  for (const auto &[source, count] : s.sources) {
    std::cout << "  " << source << ": " << count << std::endl;
  }

  std::cout << "\nChunks by type:" << std::endl;
  for (const auto &[type, count] : s.types) {
    std::cout << "  " << type << ": " << count << std::endl;
  }
}

void App::clear()
{
  std::cout << "Are you sure you want to clear all data? (yes/no): ";
  std::string confirm;
  std::cin >> confirm;

  if (confirm == "yes") {
    imp->db_->clear();
    std::cout << "Database cleared." << std::endl;
  } else {
    std::cout << "Cancelled." << std::endl;
  }
}

void App::startServer(int port) {
  httplib::Server server;
  imp->serverRunning_ = true;

  // Health check
  server.Get("/health", [](const httplib::Request &, httplib::Response &res) {
    json response = { {"status", "ok"} };
    res.set_content(response.dump(), "application/json");
    std::cout << "Health check OK" << std::endl;
    });

  // Search endpoint
  server.Post("/search", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      std::cout << "Received /search request" << std::endl;
      json request = json::parse(req.body);

      std::string query = request["query"].get<std::string>();
      size_t top_k = request.value("top_k", 5);

      // Generate query embedding
      std::vector<float> queryEmbedding;
      imp->embeddingClient_->generateEmbeddings({ query }, queryEmbedding);

      // Search
      auto results = imp->db_->search(queryEmbedding, top_k);

      // Format response
      json response = json::array();
      for (const auto &result : results) {
        response.push_back({
            {"content", result.content},
            {"source_id", result.sourceId},
            {"chunk_type", result.chunkType},
            {"similarity_score", result.similarityScore},
            {"start_pos", result.startPos},
            {"end_pos", result.endPos}
          });
      }

      res.set_content(response.dump(), "application/json");

    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 400;
      res.set_content(error.dump(), "application/json");
    }
    });

  // Embed text endpoint (one-off embedding without storage)
  server.Post("/embed", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      std::cout << "Received /embed request" << std::endl;
      json request = json::parse(req.body);
      std::string text = request["text"].get<std::string>();

      std::vector<float> embedding;
      imp->embeddingClient_->generateEmbeddings({ text }, embedding);

      json response = {
          {"embedding", embedding},
          {"dimension", embedding.size()}
      };

      res.set_content(response.dump(), "application/json");

    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 400;
      res.set_content(error.dump(), "application/json");
    }
    });

  // Add document endpoint
  server.Post("/add", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      std::cout << "Received /add request" << std::endl;
      json request = json::parse(req.body);

      std::string content = request["content"].get<std::string>();
      std::string source_id = request["source_id"].get<std::string>();

      // Chunk the content
      auto chunks = imp->chunker_->chunkText(content, source_id);

      // Generate embeddings and insert
      size_t inserted = 0;
      for (const auto &chunk : chunks) {
        std::vector<float> embedding;
        imp->embeddingClient_->generateEmbeddings({ chunk.text }, embedding);
        imp->db_->insertChunk(chunk, embedding);
        inserted++;
      }

      // Save index
      imp->db_->saveIndex();

      json response = {
          {"status", "success"},
          {"chunks_added", inserted}
      };

      res.set_content(response.dump(), "application/json");

    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 400;
      res.set_content(error.dump(), "application/json");
    }
    });

  // Stats endpoint
  server.Get("/stats", [this](const httplib::Request &, httplib::Response &res) {
    try {
      auto stats = imp->db_->getStats();

      json sources_obj = json::object();
      for (const auto &[source, count] : stats.sources) {
        sources_obj[source] = count;
      }

      json types_obj = json::object();
      for (const auto &[type, count] : stats.types) {
        types_obj[type] = count;
      }

      json response = {
          {"total_chunks", stats.totalChunks},
          {"vector_count", stats.vectorCount},
          {"sources", sources_obj},
          {"types", types_obj}
      };

      res.set_content(response.dump(), "application/json");

    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
    });

  std::cout << "Starting HTTP API server on port " << port << "..." << std::endl;
  std::cout << "Endpoints:" << std::endl;
  std::cout << "  GET  /health" << std::endl;
  std::cout << "  POST /search  - {\"query\": \"...\", \"top_k\": 5}" << std::endl;
  std::cout << "  POST /embed   - {\"text\": \"...\"}" << std::endl;
  std::cout << "  POST /add     - {\"content\": \"...\", \"source_id\": \"...\"}" << std::endl;
  std::cout << "  GET  /stats" << std::endl;
  std::cout << "\nPress Ctrl+C to stop" << std::endl;

  server.listen("0.0.0.0", port);
}

void App::initialize()
{
  std::string dbPath = imp->settings_->databaseSqlitePath();
  std::string indexPath = imp->settings_->databaseIndexPath();
  size_t vectorDim = imp->settings_->databaseVectorDim();
  size_t maxElements = imp->settings_->databaseMaxElements();

  imp->db_ = std::make_unique<VectorDatabase>(dbPath, indexPath, vectorDim, maxElements);

  std::string apiUrl = imp->settings_->embeddingApiUrl();
  size_t timeout = imp->settings_->embeddingTimeoutMs();
  imp->embeddingClient_ = std::make_unique<EmbeddingClient>(apiUrl, timeout);

  std::string tokenizerPath = imp->settings_->tokenizerConfigPath();
  imp->tokenizer_ = std::make_unique<SimpleTokenCounter>(tokenizerPath);

  size_t minTokens = imp->settings_->chunkingMinTokens();
  size_t maxTokens = imp->settings_->chunkingMaxTokens();
  float overlap = imp->settings_->chunkingOverlap();
  imp->chunker_ = std::make_unique<Chunker>(*imp->tokenizer_, minTokens, maxTokens, overlap);
  imp->processor_ = std::make_unique<SourceProcessor>(*imp->settings_);
}

void App::printUsage()
{
  std::cout << "Usage: embedder <command> [options]\n\n";
  std::cout << "Commands:\n";
  std::cout << "  embed              - Process and embed all configured sources\n";
  std::cout << "  search <query>     - Search for similar chunks\n";
  std::cout << "  stats              - Show database statistics\n";
  std::cout << "  clear              - Clear all data\n";
  std::cout << "  serve [port]       - Start HTTP API server (default: 8081)\n";
  std::cout << "\nOptions:\n";
  std::cout << "  --config <path>    - Config file path (default: settings.json)\n";
  std::cout << "  --top <k>          - Number of results for search (default: 5)\n";
  std::cout << std::endl;
}

int App::run(int argc, char *argv[])
{
  try {
    if (argc < 2) {
      printUsage();
      return 1;
    }

    std::string command = argv[1];
    std::string configPath = "settings.json";

    for (int i = 2; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--config" && i + 1 < argc) {
        configPath = argv[++i];
      }
    }

    App app(configPath);

    if (command == "embed") {
      app.embed();
    } else if (command == "search") {
      if (argc < 3) {
        std::cerr << "Error: search requires a query" << std::endl;
        return 1;
      }
      std::string query = argv[2];
      size_t topK = 5;
      for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--top" && i + 1 < argc) {
          topK = std::stoi(argv[++i]);
        }
      }
      app.search(query, topK);
    } else if (command == "stats") {
      app.stats();
    } else if (command == "clear") {
      app.clear();
    } else if (command == "serve") {
      int port = 8081;
      if (argc > 2) {
        try {
          port = std::stoi(argv[2]);
        } catch (...) {
          std::cout << "Using default port " << port << std::endl;
        }
      }
      app.startServer(port);
    } else {
      std::cerr << "Unknown command: " << command << std::endl;
      printUsage();
      return 1;
    }

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n\n";
    printUsage();
    return 1;
  }
  return 0;
}
