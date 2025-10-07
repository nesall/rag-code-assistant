#pragma once

#include <memory>
#include <string>

class Chunker;
class VectorDatabase;
class EmbeddingClient;
class CompletionClient;

class App {
  struct Impl;
  std::unique_ptr<Impl> imp;
public:
  explicit App(const std::string &configPath);
  ~App();

  // CLI commands
  void embed();
  void watch(int interval_seconds = 60);
  bool update();
  void compact();
  void search(const std::string &query, size_t topK = 5);
  void stats();
  void clear();
  void chat();
  void serve(int port);

  const Chunker &chunker() const;
  const VectorDatabase &db() const;
  VectorDatabase &db();
  const EmbeddingClient &embeddingClient() const;
  const CompletionClient &completionClient() const;

public:
  static void printUsage();
  static int run(int argc, char *argv[]);

private:
  void initialize();
};

namespace utils {
  std::string currentTimestamp();
  time_t getFileModificationTime(const std::string &path);
}