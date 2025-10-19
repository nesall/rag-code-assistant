#include <webview/webview.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <utils_log/logger.hpp>
#include <filesystem>
#include <string>
#include <thread>
#include <sstream>
#include <atomic>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

#if defined(__APPLE__)
#import <Cocoa/Cocoa.h>
#elif defined(__linux__)
#include <gtk/gtk.h>
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
    std::vector<std::string> paths = {
        exeDir + "/web_assets",
        exeDir + "/../web_assets",
        "web_assets",
        "../web_assets",
        "../../spa-svelte/dist"
    };
    for (const auto &path : paths) {
      if (fs::exists(path) && fs::exists(fs::path(path) / "index.html")) {
        return path;
      }
    }
    return "";
  }

  struct Prefs {
    int width = 700;
    int height = 900;
    int port = 8081;
    std::string host = "127.0.0.1";
    mutable std::mutex mutex_;
  };

  void fetchOrCreatePrefsJson(Prefs &prefs) {
    LOG_START;
    const std::string exeDir = getExecutableDir();
    static std::vector<std::string> paths = {
      exeDir + "/prefs.json",
      exeDir + "/../prefs.json",
      exeDir + "/../../prefs.json"
    };
    std::string prefsPath;
    for (const auto &path : paths) {
      if (fs::exists(path)) {
        prefsPath = path;
        break;
      }
    }
    std::ifstream f(prefsPath);
    if (f.is_open()) {
      std::stringstream ss;
      ss << f.rdbuf();
      try {
        auto j = nlohmann::json::parse(ss.str());
        if (j.contains("window") && j["window"].is_object()) {
          const auto &w = j["window"];
          if (w.contains("width") && w["width"].is_number_integer()) {
            prefs.width = w["width"].get<int>();
          }
          if (w.contains("height") && w["height"].is_number_integer()) {
            prefs.height = w["height"].get<int>();
          }
        }
        if (j.contains("api") && j["api"].is_object()) {
          const auto &w = j["api"];
          if (w.contains("host") && w["host"].is_string()) {
            prefs.host = w["host"];
          }
          if (w.contains("port") && w["port"].is_number_integer()) {
            prefs.port = w["port"].get<int>();
          }
        }
      } catch (const std::exception &e) {
        LOG_MSG << "Error parsing prefs.json: " << e.what();
      }
    } else {
      // Create prefs.json at paths[0]
      prefsPath = paths[0];
      nlohmann::json j;
      j["window"] = {
        {"width", prefs.width},
        {"height", prefs.height}
      };
      j["api"] = {
        {"host", prefs.host},
        {"port", prefs.port}
      };
      try {
        std::ofstream out(prefsPath);
        if (out.is_open()) {
          out << j.dump(2) << std::endl;
          out.close();
          LOG_MSG << "Created default prefs.json at: " << prefsPath;
        } else {
          LOG_MSG << "Failed to create prefs.json at: " << prefsPath;
        }
      } catch (const std::exception &e) {
        LOG_MSG << "Error writing prefs.json: " << e.what();
      }
    }
    prefs.width = (std::min)((std::max)(prefs.width, 200), 1400);
    prefs.height = (std::min)((std::max)(prefs.height, 300), 1000);
  }


  void setAppIcon(webview::webview &w, const std::string &iconBaseName) {
    auto assets = findWebAssets();
    if (assets.empty()) return;
    std::filesystem::path base = std::filesystem::path(assets) / iconBaseName;
#if defined(_WIN32)
    std::wstring iconPath = (base.parent_path() / (base.stem().wstring() + L".ico")).wstring();
    HICON hIcon = (HICON)LoadImageW(
      nullptr, iconPath.c_str(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE
    );
    if (hIcon) {
      HWND hwnd = (HWND)w.window().value();
      SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
      SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    }
#elif defined(__APPLE__)
    std::string iconPath = (base.parent_path() / (base.stem().string() + ".png")).string();
    NSString *path = [NSString stringWithUTF8String : iconPath.c_str()];
    NSImage *icon = [[NSImage alloc]initWithContentsOfFile:path];
    if (icon)[NSApp setApplicationIconImage : icon];
#elif defined(__linux__)
    std::string iconPath = (base.parent_path() / (base.stem().string() + ".png")).string();
    GError *error = nullptr;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(iconPath.c_str(), &error);
    if (pixbuf) {
      gtk_window_set_icon(GTK_WINDOW(w.window()), pixbuf);
      g_object_unref(pixbuf);
    }
#endif
  }

} // anonymous namespace

int main() {
  LOG_START;
  const std::string assetsPath = findWebAssets();
  Prefs prefs;
  fetchOrCreatePrefsJson(prefs);

  // Check assets BEFORE starting server
  if (assetsPath.empty()) {
    LOG_MSG << "Error: Could not find web assets (index.html)";
    LOG_MSG << "Please build the SPA client first:";
    LOG_MSG << "  cd ../spa-svelte && npm run build";
    return 1;
  }

  LOG_MSG << "Loading Svelte app from: " << fs::absolute(assetsPath).string();

  httplib::Server svr;
  svr.set_mount_point("/", fs::absolute(assetsPath).string().c_str());

  svr.set_logger([](const auto &req, const auto &res) {
    LOG_MSG << req.method << " " << req.path << " -> " << res.status;
    });

  svr.Get("/api/.*", [&prefs](const httplib::Request &req, httplib::Response &res) {
    LOG_START;
    LOG_MSG << "svr.Get " << req.method << " " << req.path;
    std::string host;
    int port;
    {
      std::lock_guard<std::mutex> lock(prefs.mutex_);
      host = prefs.host;
      port = prefs.port;
    }

    httplib::Client cli(host, port);
    cli.set_connection_timeout(0, 60 * 1000ull);
    auto result = cli.Get(req.path.c_str());
    if (result) {
      res.status = result->status;
      res.set_content(result->body, result->get_header_value("Content-Type"));
    } else {
      res.status = 503;
      res.set_content("{\"error\": \"Backend unavailable\"}", "application/json");
    }
    });

  svr.Post("/api/.*", [&prefs](const httplib::Request &req, httplib::Response &res) {
    LOG_START;
    LOG_MSG << "svr.Post " << req.method << " " << req.path;

    std::string contentType = req.get_header_value("Content-Type");
    if (contentType.empty()) {
      contentType = "application/json";
    }

    // Special case for /api/chat - handle streaming
    if (req.path.find("/api/chat") != std::string::npos) {

      // Set up streaming response headers
      res.set_header("Content-Type", "text/event-stream");
      res.set_header("Cache-Control", "no-cache");
      res.set_header("Connection", "keep-alive");

      res.set_chunked_content_provider(
        "text/event-stream",
        [&prefs, req, &res, contentType](size_t offset, httplib::DataSink &sink) {
          LOG_MSG << "Starting chunked content provider, offset: " << offset;
          std::string host;
          int port;
          {
            std::lock_guard<std::mutex> lock(prefs.mutex_);
            host = prefs.host;
            port = prefs.port;
          }

          httplib::Client cli(host, port);
          cli.set_connection_timeout(0, 60 * 1000ull);

          httplib::Headers headers = { {"Accept", "text/event-stream"} };
          auto postRes = cli.Post(
            req.path.c_str(),
            headers,
            req.body,
            contentType,
            [&sink](const char *data, size_t len) -> bool {
              //LOG_MSG << "Received chunk: " << len << " bytes";
              return sink.write(data, len);
            }
          );

          sink.done();
          
          if (!postRes) {
            LOG_MSG << "Error: Backend streaming unavailable";
            res.status = 503;
            res.set_content("{\"error\": \"Backend streaming unavailable\"}", "application/json");
          }
          if (postRes->status != 200) {
            LOG_MSG << "Error: Backend streaming returned status " << postRes->status;
            res.status = postRes->status;
          }
          
          LOG_MSG << "Streaming completed successfully";

          return true;
        });
      
    } else {
      // Regular POST handling for non-streaming endpoints
      std::string host;
      int port;
      {
        std::lock_guard<std::mutex> lock(prefs.mutex_);
        host = prefs.host;
        port = prefs.port;
      }

      httplib::Client cli(host, port);
      cli.set_connection_timeout(0, 60 * 1000ull);

      auto result = cli.Post(req.path.c_str(), req.body, contentType);

      if (result) {
        res.status = result->status;
        res.set_content(result->body, result->get_header_value("Content-Type"));
      } else {
        res.status = 503;
        res.set_content("{\"error\": \"Backend unavailable\"}", "application/json");
      }
    }
    });

  std::atomic<bool> serverReady{ false };
  const int port = 8709;

  std::thread serverThread([&svr, &serverReady, port]() {
    LOG_START;
    LOG_MSG << "Starting HTTP server on http://127.0.0.1:" << port;
    serverReady = true;
    svr.listen("127.0.0.1", port);
    LOG_MSG << "HTTP server stopped";
    });

  // Wait for server to be ready
  while (!serverReady.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  try {
    LOG_MSG << "Using window size, w" << prefs.width << ", h" << prefs.height;

    webview::webview w(true, nullptr);
    setAppIcon(w, "logo");
    w.set_title("RAG Code Assistant");
    w.set_size(prefs.width, prefs.height, WEBVIEW_HINT_NONE);

    w.bind("sendUrlToCpp", [&prefs, &svr](const std::string &url) -> std::string
      {
        LOG_MSG << "New URL from client UI: " << url;
        std::lock_guard<std::mutex> lock(prefs.mutex_);
        try {
          // Parse URL (simple parsing for this example)
          size_t hostStart = url.find("://") + 3;
          size_t portStart = url.find(":", hostStart);
          size_t pathStart = url.find("/", hostStart);

          std::string newHost;
          int newPort = prefs.port;

          if (portStart != std::string::npos) {
            newHost = url.substr(hostStart, portStart - hostStart);
            std::string portStr = url.substr(portStart + 1, pathStart - portStart - 1);
            newPort = std::stoi(portStr);
          } else {
            newHost = url.substr(hostStart, pathStart - hostStart);
          }

          // Update preferences
          prefs.host = newHost;
          prefs.port = newPort;

          // Save to prefs.json
          const std::string prefsPath = getExecutableDir() + "/prefs.json";
          nlohmann::json j;
          j["window"] = {
              {"width", prefs.width},
              {"height", prefs.height}
          };
          j["api"] = {
              {"host", prefs.host},
              {"port", prefs.port}
          };

          std::ofstream out(prefsPath);
          if (out.is_open()) {
            out << j.dump(2) << std::endl;
            out.close();
            LOG_MSG << "Updated prefs.json with new server URL";
          } else {
            LOG_MSG << "Failed to update prefs.json";
            return "{\"status\": \"error\", \"message\": \"Failed to save preferences\"}";
          }

          return "{\"status\": \"success\", \"message\": \"Server connection updated\"}";
        } catch (const std::exception &e) {
          LOG_MSG << "Error updating server connection: " << e.what();
          return "{\"status\": \"error\", \"message\": \"" + std::string(e.what()) + "\"}";
        }
      }
    );

    w.init(R"(
      window.cppApi = {
        sendServerUrl: function(url) {
          return sendUrlToCpp(msg);
        }
      };
      window.addEventListener('error', function(e) {
        console.error('JS Error:', e.message, e.filename, e.lineno);
      });
      console.log('Webview initialized, location:', window.location.href);
    )");

    const std::string url = "http://127.0.0.1:" + std::to_string(port);
    LOG_MSG << "Navigating to: " << url;

    w.navigate(url);
    w.run();

    LOG_MSG << "Webview closed by user, stopping HTTP server...";
  } catch (const std::exception &e) {
    LOG_MSG << "Webview error: " << e.what();
  }

  svr.stop();
  if (serverThread.joinable()) {
    serverThread.join();
    LOG_MSG << "HTTP server thread joined cleanly";
  }

  return 0;
}