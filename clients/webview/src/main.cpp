#include <webview/webview.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <utils_log/logger.hpp>
#include <filesystem>
#include <string>
#include <cassert>
#include <thread>
#include <format>
#include <sstream>
#include <atomic>
#include <fstream>
#include <unordered_map>

#ifdef _WIN32
#define WIN32_LEAM_AND_MEAN
#include <windows.h>
#include "WinDarkTitlebarImpl.h"
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

  struct AppConfig {
    int width = 700;
    int height = 900;
    int port = 8590;
    std::string host = "127.0.0.1";
    std::unordered_map<std::string, std::string> uiPrefs;
    mutable std::mutex mutex_;

    nlohmann::json toJson() const {
      nlohmann::json j;
      j["window"] = {
          {"width", width},
          {"height", height}
      };
      j["api"] = {
          {"host", host},
          {"port", port}
      };
      j["uiPrefs"] = nlohmann::json::array();
      for (const auto &item : uiPrefs) {
        j["uiPrefs"].push_back({
          {"key", item.first},
          {"value", item.second}
        });
      }
      return j;
    }
  };

  std::string getConfigPath() {
    const std::string exeDir = getExecutableDir();
    static std::vector<std::string> paths = {
      exeDir + "/appconfig.json",
      exeDir + "/../appconfig.json",
      exeDir + "/../../appconfig.json"
    };
    std::string prefsPath;
    for (const auto &path : paths) {
      if (fs::exists(path)) {
        prefsPath = path;
        break;
      }
    }
    if (!fs::exists(prefsPath)) {
      prefsPath = paths[0];
    }
    return prefsPath;
  }

  void savePrefsToFile(const AppConfig &prefs) {
    const auto prefsPath = getConfigPath();
    nlohmann::json j = prefs.toJson();
    std::ofstream out(prefsPath);
    if (out.is_open()) {
      out << j.dump(2) << std::endl;
      out.close();
      LOG_MSG << "Updated appconfig.json with new server URL";
    } else {
      LOG_MSG << "Failed to update" << prefsPath;
      throw std::runtime_error("Failed to update appconfig.json");
    }
  }

  void fetchOrCreatePrefsJson(AppConfig &prefs) {
    LOG_START;
    const auto prefsPath = getConfigPath();
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
        if (j.contains("uiPrefs") && j["uiPrefs"].is_array()) {
          for (const auto &item : j["uiPrefs"]) {
            if (item.contains("key") && item.contains("value") &&
                item["key"].is_string() && item["value"].is_string()) {
              prefs.uiPrefs.insert({
                item["key"].get<std::string>(),
                item["value"].get<std::string>()
                });
            }
          }
        }
      } catch (const std::exception &e) {
        LOG_MSG << "Error parsing appconfig.json: " << e.what();
      }
    } else {
      nlohmann::json j = prefs.toJson();
      try {
        std::ofstream out(prefsPath);
        if (out.is_open()) {
          out << j.dump(2) << std::endl;
          out.close();
          LOG_MSG << "Created default appconfig.json at: " << prefsPath;
        } else {
          LOG_MSG << "Failed to create appconfig.json at: " << prefsPath;
        }
      } catch (const std::exception &e) {
        LOG_MSG << "Error writing appconfig.json: " << e.what();
      }
    }
    if (prefs.host == "localhost") prefs.host = "127.0.0.1";
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
  if (assetsPath.empty()) {
    LOG_MSG << "Error: Could not find web assets (index.html)";
    LOG_MSG << "Please build the SPA client first:";
    LOG_MSG << "  cd ../spa-svelte && npm run build";
    return 1;
  }

  AppConfig prefs;
  fetchOrCreatePrefsJson(prefs);

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
            return false;
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

  const int serverPort = svr.bind_to_any_port("127.0.0.1");
  std::thread serverThread([&svr, &serverReady, serverPort]() {
    LOG_START;
    LOG_MSG << "Starting HTTP server on http://127.0.0.1:" << serverPort;
    serverReady = true;
    svr.listen_after_bind();
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
    w.set_title(std::format("Phenix Code Assistant - v1.0 [build date: {} {}]", __DATE__, __TIME__));
    w.set_size(prefs.width, prefs.height, WEBVIEW_HINT_NONE);

#ifdef _WIN32
    HWND hWnd = static_cast<HWND>(w.window().value());
    WinDarkTitlebarImpl winDarkImpl;
    winDarkImpl.init();
    auto changeTheme = [&winDarkImpl, hWnd](bool dark) {
      winDarkImpl.setTitleBarTheme(hWnd, dark);
      };
    changeTheme(prefs.uiPrefs["darkOrLight"] == "dark");
#endif

    w.bind("setPersistentKey", [&prefs, &svr, changeTheme](const std::string &id, const std::string &data, void *)
      {
        LOG_MSG << "setPersistentKey:" << id << data;
        try {
          auto j = nlohmann::json::parse(data);
          if (j.is_array() && 2 == j.size()) {
            std::string key = j[0];
            std::string val = j[1];
            LOG_MSG << key << val;
            if (!key.empty()) {
              std::lock_guard<std::mutex> lock(prefs.mutex_);
              prefs.uiPrefs[key] = val;
              savePrefsToFile(prefs);
              LOG_MSG << "Saved persistent key: " << key;
#ifdef _WIN32
              if (key == "darkOrLight") {
                changeTheme(val == "dark");
              }
#endif
              return;
            }
          }
        } catch (const std::exception &ex) {
          LOG_MSG << ex.what();
        }
      }, nullptr
    );

    w.bind("getPersistentKey", [&prefs, &svr](const std::string &data) -> std::string
      {
        LOG_MSG << "getPersistentKey:" << data;
        try {
          auto j = nlohmann::json::parse(data);
          if (j.is_array() && 0 < j.size()) {
            std::string key = j[0].get<std::string>();
            std::lock_guard<std::mutex> lock(prefs.mutex_);
            auto it = prefs.uiPrefs.find(key);
            if (it != prefs.uiPrefs.end()) {
              // Return a valid JSON string (quoted). Example: "\"dark\""
              return nlohmann::json(it->second).dump();
            }
          }
        } catch (const std::exception &ex) {
          LOG_MSG << ex.what();
        }
        // Return JSON null when not found
        return "null";
      }
    );

    w.bind("setServerUrl", [&prefs, &svr](const std::string &url) -> std::string
      {
        LOG_MSG << "setServerUrl:" << url;
        try {
          // Parse URL (simple parsing for this example)
          size_t hostStart = url.find("://") + 3;
          size_t portStart = url.find(":", hostStart);
          size_t pathStart = url.find("/", hostStart);

          std::lock_guard<std::mutex> lock(prefs.mutex_);

          std::string newHost;
          int newPort = prefs.port;

          if (portStart != std::string::npos) {
            newHost = url.substr(hostStart, portStart - hostStart);
            std::string portStr = url.substr(portStart + 1, pathStart - portStart - 1);
            newPort = std::stoi(portStr);
          } else {
            newHost = url.substr(hostStart, pathStart - hostStart);
          }
          if (newHost == "localhost") newHost = "127.0.0.1";

          // Update preferences
          prefs.host = newHost;
          prefs.port = newPort;

          savePrefsToFile(prefs);

          return "{\"status\": \"success\", \"message\": \"Server connection updated\"}";
        } catch (const std::exception &e) {
          LOG_MSG << "Error updating server connection: " << e.what();
          return "{\"status\": \"error\", \"message\": \"" + std::string(e.what()) + "\"}";
        }
      }
    );

    w.bind("getServerUrl", [&prefs, &svr](const std::string &) -> std::string
      {
        std::lock_guard<std::mutex> lock(prefs.mutex_);
        LOG_MSG << "getServerUrl" << prefs.host << prefs.port;
        try {
          std::string url = std::format("http://{}:{}", prefs.host, prefs.port);
          return nlohmann::json(url).dump();
        } catch (const std::exception &ex) {
          LOG_MSG << ex.what();
        }
        return "null";
      }
    );

    w.init(R"(
      window.cppApi = {
        setServerUrl,
        getServerUrl,
        setPersistentKey,
        getPersistentKey,
      };
      window.addEventListener('error', function(e) {
        console.error('JS Error:', e.message, e.filename, e.lineno);
      });
      console.log('Webview initialized, location:', window.location.href);
    )");

    const std::string url = "http://127.0.0.1:" + std::to_string(serverPort);
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