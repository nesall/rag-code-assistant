#ifndef _HTTPSERVER_H_
#define _HTTPSERVER_H_

#include <memory>

class Chunker;
class VectorDatabase;
class EmbeddingClient;
class CompletionClient;

class HttpServer {
  struct Impl;
  std::unique_ptr<Impl> imp;

public:
  HttpServer(Chunker &, VectorDatabase &, EmbeddingClient &, CompletionClient &);
  ~HttpServer();
  bool startServer(int port);
  void stop();
};

#endif // _HTTPSERVER_H_