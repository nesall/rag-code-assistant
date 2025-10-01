#include "httpserver.h"
#include "chunker.h"
#include "database.h"
#include "inference.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <cassert>
#include <thread>
#include <format>

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
  Impl(Chunker &c, VectorDatabase &d, EmbeddingClient &e, CompletionClient  &g)
    : chunker_(c), db_(d), embeddingClient_(e), completionClient_(g)
  {
  }

  httplib::Server server_;

  Chunker &chunker_;
  VectorDatabase &db_;
  EmbeddingClient &embeddingClient_;
  CompletionClient &completionClient_;

  static int counter_;
};

int HttpServer::Impl::counter_ = 0;

HttpServer::HttpServer(Chunker &c, VectorDatabase &d, EmbeddingClient &e, CompletionClient &g) 
  : imp(new Impl(c, d, e, g))
{
  //imp->server_.new_task_queue = [] { return new httplib::ThreadPool(4); };
}

HttpServer::~HttpServer()
{
}

bool HttpServer::startServer(int port)
{
  auto &server = imp->server_;

  server.Get("/health", [](const httplib::Request &, httplib::Response &res) {
    json response = { {"status", "ok"} };
    res.set_content(response.dump(), "application/json");
    std::cout << "Health check OK" << std::endl;
    });

  server.Post("/search", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      std::cout << "Received /search request" << std::endl;
      json request = json::parse(req.body);

      std::string query = request["query"].get<std::string>();
      size_t top_k = request.value("top_k", 5);
      std::vector<float> queryEmbedding;
      imp->embeddingClient_.generateEmbeddings({ query }, queryEmbedding);

      auto results = imp->db_.search(queryEmbedding, top_k);

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

  // (one-off embedding without storage)
  server.Post("/embed", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      std::cout << "Received /embed request" << std::endl;
      json request = json::parse(req.body);
      std::string text = request["text"].get<std::string>();

      std::vector<float> embedding;
      imp->embeddingClient_.generateEmbeddings({ text }, embedding);

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

  server.Post("/add", [this](const httplib::Request &req, httplib::Response &res) {
    try {
      std::cout << "Received /add request" << std::endl;
      json request = json::parse(req.body);

      std::string content = request["content"].get<std::string>();
      std::string source_id = request["source_id"].get<std::string>();

      auto chunks = imp->chunker_.chunkText(content, source_id);

      size_t inserted = 0;
      for (const auto &chunk : chunks) {
        std::vector<float> embedding;
        imp->embeddingClient_.generateEmbeddings({ chunk.text }, embedding);
        imp->db_.addDocument(chunk, embedding);
        inserted++;
      }

      imp->db_.persist();

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

  server.Get("/stats", [this](const httplib::Request &, httplib::Response &res) {
    try {
      auto stats = imp->db_.getStats();

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

  server.Post("/chat", [this](const httplib::Request &req, httplib::Response &res) {
    try {
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
      // Last message is always from user
      assert(role == "user");

      std::vector<float> embedding;
      imp->embeddingClient_.generateEmbeddings({ question }, embedding);
      auto results = imp->db_.search(embedding, 5);


      res.set_header("Content-Type", "text/event-stream");
      res.set_header("Cache-Control", "no-cache");
      res.set_header("Connection", "keep-alive");

      res.set_chunked_content_provider(
        "text/event-stream",
        [this, messagesJson, results, temperature](size_t offset, httplib::DataSink &sink) {
          std::cout << "set_chunked_content_provider: in callback ...\n";
          try {
            std::string context = imp->completionClient_.generateCompletion(
              messagesJson, results, temperature,
              [&sink](const std::string &streamedChunk) {
#ifdef _DEBUG
                std::cout << streamedChunk;
#endif
                // SSE format requires "data: <payload>\n\n"
                std::string sse = "data: " + streamedChunk + "\n\n";
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
          std::cout << "set_chunked_content_provider: callback DONE.\n";
          return true;
        }
      );

    } catch (const std::exception &e) {
      json error = { {"error", e.what()} };
      res.status = 400;
      res.set_content(error.dump(), "application/json");
    }
    });

  std::cout << "Starting HTTP API server on port " << port << "...\n";
  std::cout << "Endpoints:\n";
  std::cout << "  GET  /health\n";
  std::cout << "  POST /search  - {\"query\": \"...\", \"top_k\": 5}\n";
  std::cout << "  POST /embed   - {\"text\": \"...\"}\n";
  std::cout << "  POST /add     - {\"content\": \"...\", \"source_id\": \"...\"}\n";
  std::cout << "  POST /chat    - {\"messages\":[\"role\":\"...\", \"content\":\"...\"], \"temperature\": \"...\"}\n";
  std::cout << "  GET  /stats\n";
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