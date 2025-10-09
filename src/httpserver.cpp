#include "httpserver.h"
#include "app.h"
#include "chunker.h"
#include "sourceproc.h"
#include "database.h"
#include "inference.h"
#include "settings.h"
#include "tokenizer.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <cassert>
#include <thread>
#include <exception>
#include <algorithm>
#include <set>

using json = nlohmann::json;



//auto testStreaming = [](std::function<void(const std::string &)> onChunk)
//  {
//    for (int i = 0; i < 25; ++i) {
//      std::this_thread::sleep_for(std::chrono::milliseconds(250));
//      std::ostringstream oss;
//      oss << "thread ";
//      oss << std::this_thread::get_id();
//      oss << ", chunk ";
//      oss << std::to_string(i);
//      oss << "\n\n";
//      onChunk(oss.str());
//    }
//  };



struct HttpServer::Impl {
  Impl(App &a)
    : app_(a)
  {
  }

  httplib::Server server_;

  App &app_;

  std::unique_ptr<std::thread> watchThread_;
  std::atomic<bool> watchRunning_{ false };

  static int counter_;
};

int HttpServer::Impl::counter_ = 0;

HttpServer::HttpServer(App &a)
  : imp(new Impl(a))
{
  //imp->server_.new_task_queue = [] { return new httplib::ThreadPool(4); };
}

HttpServer::~HttpServer()
{
}

bool HttpServer::startServer(int port, bool enableWatch, int watchInterval)
{
  auto &server = imp->server_;

  server.Get("/api/health", [](const httplib::Request &, httplib::Response &res) {
    std::cout << "GET /api/health\n";
    json response = { {"status", "ok"} };
    res.set_content(response.dump(), "application/json");
    });

  server.Post("/api/search", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      std::cout << "POST /api/search\n";
      json request = json::parse(req.body);

      std::string query = request["query"].get<std::string>();
      size_t top_k = request.value("top_k", 5);
      std::vector<float> queryEmbedding;
      imp->app_.embeddingClient().generateEmbeddings({ query }, queryEmbedding);

      auto results = imp->app_.db().search(queryEmbedding, top_k);

      json response = json::array();
      for (const auto &result : results) {
        response.push_back({
            {"content", result.content},
            {"source_id", result.sourceId},
            {"chunk_type", result.chunkType},
            {"chunk_unit", result.chunkUnit},
            {"similarity_score", result.similarityScore},
            {"start_pos", result.start},
            {"end_pos", result.end}
          });
      }

      res.set_content(response.dump(), "application/json");

    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 400;
      res.set_content(error.dump(), "application/json");
    }
    });

  // (one-off embedding without storage)
  server.Post("/api/embed", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      std::cout << "POST /api/embed\n";
      json request = json::parse(req.body);
      std::string text = request["text"].get<std::string>();

      std::vector<float> embedding;
      imp->app_.embeddingClient().generateEmbeddings({ text }, embedding);

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

  server.Post("/api/documents", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      std::cout << "POST /api/documents\n";
      json request = json::parse(req.body);

      std::string content = request["content"].get<std::string>();
      std::string source_id = request["source_id"].get<std::string>();

      auto chunks = imp->app_.chunker().chunkText(content, source_id);

      size_t inserted = 0;
      for (const auto &chunk : chunks) {
        std::vector<float> embedding;
        imp->app_.embeddingClient().generateEmbeddings({ chunk.text }, embedding);
        imp->app_.db().addDocument(chunk, embedding);
        inserted++;
      }

      imp->app_.db().persist();

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

  server.Get("/api/documents", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      std::cout << "GET /api/documents\n";
      auto files = imp->app_.db().getTrackedFiles();
      json response = json::array();
      for (const auto &file : files) {
        response.push_back({
            {"path", file.path},
            {"lastModified", file.lastModified},
            {"size", file.fileSize}
          });
      }
      res.set_content(response.dump(), "application/json");
    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
    });

  server.Get("/api/stats", [this](const httplib::Request &, httplib::Response &res) {
    try {
      std::cout << "GET /api/stats\n";
      auto stats = imp->app_.db().getStats();
      json sources_obj = json::object();
      for (const auto &[source, count] : stats.sources) {
        sources_obj[source] = count;
      }
      json response = {
          {"total_chunks", stats.totalChunks},
          {"vector_count", stats.vectorCount},
          {"sources", sources_obj}
      };
      res.set_content(response.dump(), "application/json");
      std::cout << std::this_thread::get_id() << " Starting /stats ...\n";
      std::this_thread::sleep_for(std::chrono::seconds(20));
      std::cout << std::this_thread::get_id() << " Finished /stats !\n";
    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
    });

  server.Post("/api/update", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      std::cout << "POST /api/update\n";
      auto nof = imp->app_.update();
      json response = { {"status", "updated"}, {"nof_files", std::to_string(nof)} };
      res.set_content(response.dump(), "application/json");
    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
    });

  server.Post("/api/chat", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      std::cout << "POST /api/chat\n";
      json request = json::parse(req.body);
      float temperature = request.value("temperature", 0.5f);
      // format for messages field in request
      /*
        "messages": [
          {"role": "system", "content": "Keep it short."},
          {"role": "user", "content": "What is the capital of France?"}
        ],
      */
      auto messagesJson = request["messages"];
      // Fetch content of the last message
      assert(0 < messagesJson.size());
      std::string role = messagesJson.back()["role"].get<std::string>();
      std::string question = messagesJson.back()["content"].get<std::string>();


      std::string attachments;
      const size_t a = question.find("[Attachment: ");
      if (a != std::string::npos) {
        size_t b = question.rfind("[/Attachment]");
        if (b == std::string::npos) {
          b = question.length() - 1;
        }
        attachments = utils::trimmed(question.substr(a, b + std::string("[/Attachment]").length()));
        question = utils::trimmed(question.substr(0, a));
      }

      // Last message is always from user
      assert(role == "user");

      std::vector<SearchResult> results;
      if (!attachments.empty()) {
        results.push_back({
            attachments,
            "attachment",
            "char",
            Chunker::detectContentType(attachments, ""),
            0, // chunkId
            0,
            attachments.length(),
            1.0f
          });
      }

      std::unordered_map<std::string, size_t> sourcesRank;
      auto questionChunks = imp->app_.chunker().chunkText(question, "", false);
      for (const auto &qc : questionChunks) {
        std::vector<float> embedding;
        imp->app_.embeddingClient().generateEmbeddings({ qc.text }, embedding);
        auto res = imp->app_.db().search(embedding, imp->app_.settings().embeddingTopK());
        results.insert(results.end(), res.begin(), res.end());
        for (const auto &r : res) {
          sourcesRank[r.sourceId] += 1;
        }
      }

      std::sort(results.begin(), results.end(), [&sourcesRank](const SearchResult &a, const SearchResult &b) {
        return sourcesRank[a.sourceId] > sourcesRank[b.sourceId];
        });
      std::set<std::string> sources;
      for (const auto r : results) {
        sources.insert(r.sourceId);
        if (sources.size() == 2) break;
      }

      std::set<std::string> fullSources;

      for (const auto &src : sources) {
        auto data = imp->app_.sourceProcessor().fetchSource(src);
        if (!data.content.empty()) {
          results.push_back({
              data.content,
              src,
              "char",
              Chunker::detectContentType(src, ""),
              -1ull, // chunkId
              0,
              data.content.length(),
              1.0f
            });
          fullSources.insert(src);
          auto trackedFiles = imp->app_.db().getTrackedFiles();
          std::vector<std::string> trackedSources;
          for (const auto &tf : trackedFiles) {
            trackedSources.push_back(tf.path);
          }
          auto relations = SourceProcessor::filterRelatedSources(trackedSources, src);
          for (const auto &rel : relations) {
            if (sources.find(rel) == sources.end()) {
              auto rdata = imp->app_.sourceProcessor().fetchSource(rel);
              if (!rdata.content.empty()) {

                results.push_back({
                    rdata.content,
                    rel,
                    "char",
                    Chunker::detectContentType(src, ""),
                    -1ull,
                    0,
                    rdata.content.length(),
                    1.0f,
                  });
                sources.insert(rel);
                fullSources.insert(rel);
              }
            }
          }
        }
      }

      results.erase(std::remove_if(results.begin(), results.end(),
        [&fullSources](const SearchResult &r) {
          return fullSources.find(r.sourceId) != fullSources.end() && r.chunkId != -1ull;
        }), results.end());

      res.set_header("Content-Type", "text/event-stream");
      res.set_header("Cache-Control", "no-cache");
      res.set_header("Connection", "keep-alive");

      res.set_chunked_content_provider(
        "text/event-stream",
        [this, messagesJson, results, temperature](size_t offset, httplib::DataSink &sink) {
          //std::cout << "set_chunked_content_provider: in callback ...\n";
          try {
            std::string context = imp->app_.completionClient().generateCompletion(
              messagesJson, results, temperature,
              [&sink](const std::string &chunk) {
#ifdef _DEBUG
                //std::cout << chunk;
#endif
                // SSE format requires "data: <payload>\n\n"
                nlohmann::json payload = { {"content", chunk} };
                std::string sse = "data: " + payload.dump() + "\n\n";
                bool success = sink.write(sse.data(), sse.size());
                if (!success) {
                  return; // Client disconnected
                }
              });

            //testStreaming([&sink](const std::string &chunk) {
            //  if (!sink.write(chunk.data(), chunk.size())) {
            //    return; // client disconnected
            //  }
            //  });

            std::string done = "data: [DONE]\n\n";
            sink.write(done.data(), done.size());
            sink.done();
          } catch (const std::exception &e) {
            std::string error = "data: {\"error\": \"" + std::string(e.what()) + "\"}\n\n";
            sink.write(error.data(), error.size());
            sink.done();
          }
          //std::cout << "set_chunked_content_provider: callback DONE.\n";
          return true;
        }
      );

    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 400;
      res.set_content(error.dump(), "application/json");
    }
    });

  server.Get("/api", [](const httplib::Request &, httplib::Response &res) {
    std::cout << "GET /api\n";
    json info = {
        {"name", "Embeddings RAG API"},
        {"version", "1.0.0"},
        {"endpoints", {
            {"GET /api/health", "Health check"},
            {"GET /api/documents", "Get documents"},
            {"GET /api/stats", "Database statistics"},
            {"POST /api/search", "Semantic search"},
            {"POST /api/chat", "Chat with context (streaming)"},
            {"POST /api/embed", "Generate embeddings"},
            {"POST /api/documents", "Add documents"},
            {"POST /api/update", "Trigger manual update"},
        }}
    };
    res.set_content(info.dump(2), "application/json");
    });

  std::cout << "Starting HTTP API server on port " << port << "...\n";

  if (enableWatch) {
    startWatch(watchInterval);
    std::cout << "  Auto-update: enabled (every " << watchInterval << "s)\n";
  } else {
    std::cout << "  Auto-update: disabled\n";
  }

  std::cout << "\nEndpoints:\n";
  std::cout << "  GET  /api/health\n";
  std::cout << "  GET  /api/stats\n";
  std::cout << "  GET  /api/documents\n";
  std::cout << "  POST /api/search    - {\"query\": \"...\", \"top_k\": 5}\n";
  std::cout << "  POST /api/embed     - {\"text\": \"...\"}\n";
  std::cout << "  POST /api/documents - {\"content\": \"...\", \"source_id\": \"...\"}\n";
  std::cout << "  POST /api/chat      - {\"messages\":[\"role\":\"...\", \"content\":\"...\"], \"temperature\": \"...\"}\n";
  std::cout << "\nPress Ctrl+C to stop\n";

  return server.listen("0.0.0.0", port);
}

void HttpServer::stop()
{
  if (imp->server_.is_running()) {
    std::cout << "Server stopping...\n";
    imp->server_.stop();
    std::cout << "Server stopped!\n";
  }
}

void HttpServer::startWatch(int intervalSeconds)
{
  imp->watchRunning_ = true;

  imp->watchThread_ = std::make_unique<std::thread>([this, intervalSeconds]() {
    std::cout << "[Watch] Background monitoring started (interval: " << intervalSeconds << "s)\n";

    while (imp->watchRunning_) {
      // Sleep in small chunks so we can stop quickly
      for (int i = 0; i < intervalSeconds && imp->watchRunning_; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }

      if (!imp->watchRunning_) break;

      try {
        //auto sources = collectSources();
        //std::vector<std::string> current_files;
        //for (const auto &source : sources) {
        //  current_files.push_back(source.path);
        //}

        //if (updater_->needsUpdate(current_files)) {
        //  std::cout << "\n[Watch] Changes detected at "
        //    << getCurrentTimestamp() << std::endl;

        //  auto info = updater_->detectChanges(current_files);
        //  size_t updated = updater_->updateDatabase(
        //    *embedding_client_, *chunker_, info
        //  );

        //  std::cout << "[Watch] Update completed: "
        //    << updated << " files processed" << std::endl;
        //}

        imp->app_.update();
      } catch (const std::exception &e) {
        std::cerr << "[Watch] Error during update: " << e.what() << std::endl;
      }
    }
    std::cout << "[Watch] Background monitoring stopped" << std::endl;
    });
}

void HttpServer::stopWatch()
{
  if (imp->watchRunning_) {
    std::cout << "Stopping watch mode..." << std::endl;
    imp->watchRunning_ = false;
    if (imp->watchThread_ && imp->watchThread_->joinable()) {
      imp->watchThread_->join();
    }
  }
}