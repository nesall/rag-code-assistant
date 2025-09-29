#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include "include/settings.h"
#include "include/tokenizer.h"
#include "include/chunker.h"
#include "include/embedder.h"
#include "include/database.h"
#include "include/app.h"


int main(int argc, char *argv[]) {
    return EmbedderApp::run(argc, argv);

#if 0
  try {
    Settings settings("settings.json");
    SourceProcessor processor(settings);
    auto sources = processor.getAllSources();
    // Now chunk, embed, and store each source

    EmbeddingClient embedder(settings.embeddingApiUrl());
    VectorDatabase db(settings.databaseSqlitePath(), settings.databaseIndexPath(), settings.databaseVectorDim());
    SimpleTokenCounter tokenizer(settings.tokenizerPath());
    Chunker chunker(tokenizer, settings.chunkingMinTokens(), settings.chunkingMaxTokens(), 0/*settings.chunkingOverlap()*/);

    for (size_t i = 0; i < sources.size(); ++i) {
      const auto [text, source] = sources[i];
      std::string sourceId = std::filesystem::path(source).filename().string() + "_" + std::to_string(i + 1);
      auto chunks = chunker.chunkText(text, sourceId, settings.chunkingSemantic());
      std::cout << "Source ID: " << sourceId << ", Total Chunks: " << chunks.size() << std::endl;
      for (const auto &chunk : chunks) {
        std::cout
                  << ", #tokens: " << chunk.metadata.tokenCount
                  << ",  dockId: " << chunk.docId
                  << "\n";
      }
      std::cout << "----------------------------------------\n";
      size_t sumTokens = 0;
      for (const auto &chunk : chunks) {
        sumTokens += chunk.metadata.tokenCount;
      }
      std::cout << "total summed tokens " << sumTokens << "; text tokens " << tokenizer.countTokensWithVocab(text) << "\n";

      std::cout << "generating embeddings...\n";
      for (const auto &chunk : chunks) {
        std::vector<float> embedding;
        embedder.generateEmbeddings({ chunk.text }, embedding);
        std::cout << "embedding for chunk " << chunk.chunkId << " [" << embedding.size() << "]\n";
        db.insertChunk(chunk, embedding);
      }

      db.saveIndex();
    }

    std::vector<float> queryEmbedding;
    embedder.generateEmbeddings({ "batch chunking" }, queryEmbedding);
    auto results = db.search(queryEmbedding, 5);


  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
#endif
}