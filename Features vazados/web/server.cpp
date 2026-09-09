#include <web/server.hpp>
#include <web/maps_data.h>
#include <xorstr.h>
#include <atomic>
#include <mutex>
#include <console.h>

namespace Web {
	using namespace Context;

	static zm::http::Server* g_server = nullptr;
	static std::mutex       g_mutex;
	static std::atomic<int>  g_port{ 0 };
	static std::atomic<bool> g_running{ false };

	std::string BuildJSON() {
		std::string json = "{";

		std::string cleanMap = MapName;
		auto dot = cleanMap.find_last_of('.');
		if (dot != std::string::npos) cleanMap = cleanMap.substr(0, dot);
		auto slash = cleanMap.find_last_of("/\\");
		if (slash != std::string::npos) cleanMap = cleanMap.substr(slash + 1);

		json += std::string(xorstr_("\"map\":\"")) + cleanMap + std::string(xorstr_("\","));
		json += std::string(xorstr_("\"localYaw\":")) + std::to_string(LocalAimAngles.y) + ",";
		json += std::string(xorstr_("\"localPosition\":{"));
		json += std::string(xorstr_("\"x\":")) + std::to_string(LocalEyePos.x) + ",";
		json += std::string(xorstr_("\"y\":")) + std::to_string(LocalEyePos.y) + ",";
		json += std::string(xorstr_("\"z\":")) + std::to_string(LocalEyePos.z) + "},";

		json += std::string(xorstr_("\"players\":["));

		{
			std::lock_guard<std::mutex> lock(EntitiesMutex);
			for (size_t i = 0; i < Entities.size(); i++) {
				auto& e = Entities[i];
				if (i > 0) json += ",";
				json += "{";
				json += std::string(xorstr_("\"name\":\"")) + e.name + std::string(xorstr_("\","));
				json += std::string(xorstr_("\"health\":")) + std::to_string(e.health) + ",";
				json += std::string(xorstr_("\"armor\":")) + std::to_string(e.armor) + ",";
				json += std::string(xorstr_("\"money\":")) + std::to_string(e.money) + ",";
				json += std::string(xorstr_("\"distance\":")) + std::to_string((int)e.distance) + ",";
				json += std::string(xorstr_("\"isTeam\":")) + std::string(e.isTeam ? xorstr_("true") : xorstr_("false")) + ",";
				json += std::string(xorstr_("\"isLocal\":")) + std::string(e.isLocal ? xorstr_("true") : xorstr_("false")) + ",";
				json += std::string(xorstr_("\"isVisible\":")) + std::string(e.isVisible ? xorstr_("true") : xorstr_("false")) + ",";
				json += std::string(xorstr_("\"position\":{"));
				json += std::string(xorstr_("\"x\":")) + std::to_string(e.position.x) + ",";
				json += std::string(xorstr_("\"y\":")) + std::to_string(e.position.y) + ",";
				json += std::string(xorstr_("\"z\":")) + std::to_string(e.position.z) + "}";
				json += "}";
			}
		}

		json += std::string(xorstr_("]}"));
		return json;
	}

	void Start(int port) {
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			if (g_server) return;
			g_server = new zm::http::Server();
		}

		g_server->Get(std::string(xorstr_("/")), [](const zm::http::Request&, zm::http::Response& res) {
			res.set_content(RADAR_HTML, std::string(xorstr_("text/html")));
		});

		g_server->Get(std::string(xorstr_("/api/data")), [](const zm::http::Request&, zm::http::Response& res) {
			res.set_header(std::string(xorstr_("Access-Control-Allow-Origin")), std::string(xorstr_("*")));
			res.set_content(BuildJSON(), std::string(xorstr_("application/json")));
		});

		g_server->Get(std::string(xorstr_("/maps/(.*)/radar.png")), [](const zm::http::Request& req, zm::http::Response& res) {
			auto mapName = req.matches[1].str();
			auto* entry = MapsData::Find(mapName.c_str());
			Console::Info("[web] GET /maps/%s/radar.png -> %s",
				mapName.c_str(), entry ? "OK" : "404");
			if (entry) {
				res.set_header(std::string(xorstr_("Access-Control-Allow-Origin")), std::string(xorstr_("*")));
				res.set_content(std::string(reinterpret_cast<const char*>(entry->png), entry->pngSize), std::string(xorstr_("image/png")));
			} else {
				res.status = 404;
			}
		});

		g_server->Get(std::string(xorstr_("/maps/(.*)/data.json")), [](const zm::http::Request& req, zm::http::Response& res) {
			auto mapName = req.matches[1].str();
			auto* entry = MapsData::Find(mapName.c_str());
			Console::Info("[web] GET /maps/%s/data.json -> %s",
				mapName.c_str(), entry ? "OK" : "404");
			if (entry) {
				char buf[128];
				snprintf(buf, sizeof(buf), "{\"x\":%.0f,\"y\":%.0f,\"scale\":%.1f}", entry->x, entry->y, entry->scale);
				res.set_header(std::string(xorstr_("Access-Control-Allow-Origin")), std::string(xorstr_("*")));
				res.set_content(std::string(buf), std::string(xorstr_("application/json")));
			} else {
				res.status = 404;
			}
		});

		g_port.store(port);
		g_running.store(true);
		g_server->listen(std::string(xorstr_("127.0.0.1")), port);
		g_running.store(false);

		{
			std::lock_guard<std::mutex> lock(g_mutex);
			delete g_server;
			g_server = nullptr;
		}
	}

	void Stop() {
		std::lock_guard<std::mutex> lock(g_mutex);
		if (g_server) g_server->stop();
	}

	void Restart(int port) {
		Stop();
		while (g_running.load()) Sys::Sleep(20);
		Sys::CreateDetachedThread([](void* arg) {
			Start(static_cast<int>(reinterpret_cast<intptr_t>(arg)));
		}, reinterpret_cast<void*>(static_cast<intptr_t>(port)));
	}

	int  CurrentPort()  { return g_port.load(); }
	bool IsRunning()    { return g_running.load(); }
}
