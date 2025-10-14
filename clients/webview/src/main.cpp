#include <webview/webview.h>
#include <httplib.h>
#include <ulogger.hpp>
#include <filesystem>
#include <string>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

  std::string getExecutableDir() {
    LOG_START;
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return fs::path(path).parent_path().string();
#else
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    return fs::path(std::string(result, (count > 0) ? count : 0)).parent_path().string();
#endif
  }

  std::string findWebAssets() {
    LOG_START;
    std::string exeDir = getExecutableDir();
    // Check multiple possible locations
    std::vector<std::string> paths = {
        exeDir + "/web_assets",           // Development build
        exeDir + "/../web_assets",        // Alternate location
        "web_assets",                     // Current directory
        "../web_assets",                  // Parent directory
        "../../spa-svelte/dist"           // Direct from source (dev)
    };
    for (const auto &path : paths) {
      if (fs::exists(path)) {
        return path;
      }
    }
    return "";
  }

} // anonymous namespace

int main() {
  LOG_START;
  const std::string assetsPath = findWebAssets();

  if (!fs::exists(std::filesystem::path(assetsPath))) {
    LOG_MSG << "Error: index.html not found in " << assetsPath;
    return 1;
  }

  LOG_MSG << "Loading Svelte app from: " << fs::absolute(assetsPath).string();

  httplib::Server svr;
  svr.set_mount_point("/", assetsPath.c_str());

  svr.set_logger([](const auto &req, const auto &res) {
    LOG_MSG << req.method << " " << req.path << " -> " << res.status;
    });

  std::atomic<int> port{ 8709 };

  std::thread serverThread([&svr, &port]() {
    LOG_START;
    int p = port.load();
    do {
      port.store(p);
      LOG_MSG << "Starting HTTP server on http://localhost:" << port.load();
    } while (!svr.listen("127.0.0.1", p++));
    LOG_MSG << "HTTP server listen returned";
    });
  serverThread.detach();

  if (assetsPath.empty()) {
    LOG_MSG << "Error: Could not find web assets (index.html)";
    LOG_MSG << "Please build the SPA client first:";
    LOG_MSG << "  cd ../spa-svelte && npm run build";
    svr.stop();
    if (serverThread.joinable()) serverThread.join();
    return 1;
  }

  try {
    webview::webview w(true, nullptr);
    w.set_title("RAG Code Assistant");
    w.set_size(700, 900, WEBVIEW_HINT_NONE);

    // Bind C++ functions for JavaScript communication
    w.bind("sendToCpp", [](const std::string &msg) -> std::string {
      LOG_MSG << "Message from Svelte: " << msg;
      return "{\"status\": \"success\", \"message\": \"Received by C++\"}";
      });

    // Expose C++ API to JavaScript
    w.init(R"(
            window.cppApi = {
                sendMessage: function(msg) {
                    return sendToCpp(msg);
                }
            };
    
            // Log errors to C++
            window.addEventListener('error', function(e) {
                console.error('JS Error:', e.message, e.filename, e.lineno);
            });
    
            console.log('Webview initialized, location:', window.location.href);
        )");

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const std::string url = "http://127.0.0.1:" + std::to_string(port);

    w.navigate(url);
    w.run();
    LOG_MSG << "Webview closed by user, stopping HTTP server...";
    svr.stop();
    if (serverThread.joinable()) {
      serverThread.join();
      LOG_MSG << "HTTP server thread joined cleanly";
    }
  } catch (const std::exception &e) {
    LOG_MSG << "Webview error: " << e.what();
    svr.stop();
    if (serverThread.joinable()) serverThread.join();
    return 1;
  }

  return 0;
}