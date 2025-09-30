## embedder\_cpp

Chunking/embedding utility for both code and natural language.  
This is experimental RAG oriented embedder in pure C++.  

### How to build

```
# clone the repository and cd into it. Then:
mkdir build && cd build
cmake ...
make
```


### CLI commands

Initial full embed of all sources from settings.json  
```./embedder embed --config settings.json```

Check for changes and update only what changed  
```./embedder update```

Continuous monitoring (checks every 60 seconds)  
```./embedder watch 60```

Reclaim space used by deleted index items  
```./embedder compact```

Search nearest neighbours  
```./embedder search "how to optimize C++" --top 10```

### REST API endpoints

```
# Health check
curl http://localhost:8081/health

# Search
curl -X POST http://localhost:8081/search \
  -H "Content-Type: application/json" \
  -d '{"query": "optimize performance", "top_k": 5}'

# Embed text (without storing)
curl -X POST http://localhost:8081/embed \
  -H "Content-Type: application/json" \
  -d '{"text": "your text here"}'

# Add document
curl -X POST http://localhost:8081/add \
  -H "Content-Type: application/json" \
  -d '{"content": "document content", "source_id": "doc.txt"}'

# Get stats
curl http://localhost:8081/stats
```

