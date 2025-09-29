#pragma once

#include <memory>
#include <string>


class EmbedderApp {
  struct Impl;
  std::unique_ptr<Impl> imp;
public:
  explicit EmbedderApp(const std::string &configPath);
  ~EmbedderApp();

  // CLI commands
  void embed();
  void search(const std::string &query, size_t topK = 5);
  void stats();
  void clear();

  // HTTP API server
  void startServer(int port = 8081);

public:
  static void printUsage();
  static int run(int argc, char *argv[]);

private:
  void loadConfig(const std::string &path);
  void initialize();
  //std::vector<Source> getSources();
  //std::vector<Source> processDirectory(const json &source);
  //std::string readFile(const std::string &path);


};
