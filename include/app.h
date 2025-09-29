#pragma once

#include <memory>
#include <string>


class App {
  struct Impl;
  std::unique_ptr<Impl> imp;
public:
  explicit App(const std::string &configPath);
  ~App();

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
  void initialize();
  //std::vector<Source> getSources();
  //std::vector<Source> processDirectory(const json &source);
  //std::string readFile(const std::string &path);


};
