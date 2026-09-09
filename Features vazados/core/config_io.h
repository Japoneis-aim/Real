#pragma once
#include <core/config.h>
#include <string>
#include <vector>
#include <cstring>
#include <LazyDlls/Lazyimporter.hpp>
#include <xorstr.h>
#include <nt/wrap.h>

namespace Config {

	// Path estilo Cache do Windows (mesmo padrao do ZmInternal):
	//   %LOCALAPPDATA%\Microsoft\Windows\Caches\{GUID}\<profile>.7.ver0x..-..-.db
	// Diretorio tem nome de subsistema oficial, arquivo parece chunk de cache
	// de versao do Windows. Conteudo escrito XOR-encriptado com header magic
	// "NHCM" — nao abre como texto.
	inline std::string GetConfigDir() {
		char appdata[MAX_PATH]{};
		// Sys::GetEnvA le PEB->ProcessParameters->Environment direto (sem
		// kernel32). LOCALAPPDATA tem path latin → ASCII fast-path OK.
		DWORD n = Sys::GetEnvA(L"LOCALAPPDATA", appdata, MAX_PATH);
		std::string base = (n > 0 && n < MAX_PATH) ? std::string(appdata) : std::string(".");
		return base + std::string(xorstr_(
			"\\Microsoft\\Windows\\Caches\\{4E9F3F4D-2E1A-4B6A-9F1C-7D3E5A8B0C2D}"));
	}

	inline std::string GetProfilePath(const std::string& name) {
		return GetConfigDir() + std::string(xorstr_("\\")) + name +
			std::string(xorstr_(".7.ver0x000000000000003e.db"));
	}

	inline std::string ActiveProfile = "default";
	inline std::vector<std::string> ProfileList;
	inline bool ProfileListDirty = true;

	// Cria o caminho diretorio por diretorio (LOCALAPPDATA -> Microsoft ->
	// Windows -> Caches -> {GUID}). Cada nivel via CreateDirectoryA — se ja
	// existe so ignora (ERROR_ALREADY_EXISTS).
	inline void EnsureConfigDir() {
		std::string dir = GetConfigDir();
		for (size_t i = 0; i < dir.size(); ++i) {
			if (dir[i] == '\\' || dir[i] == '/') {
				if (i > 3) {
					std::string partial = dir.substr(0, i);
					LI_CACHED(CreateDirectoryA)(partial.c_str(), nullptr);
				}
			}
		}
		LI_CACHED(CreateDirectoryA)(dir.c_str(), nullptr);
	}

	inline void RefreshProfiles() {
		ProfileList.clear();
		std::string search = GetConfigDir() + std::string(xorstr_("\\*.db"));
		WIN32_FIND_DATAA fd;
		HANDLE h = LI_CACHED(FindFirstFileA)(search.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE) { ProfileListDirty = false; return; }
		do {
			std::string name(fd.cFileName);
			// Tira o suffix ".7.ver0x...db" pra deixar so o nome do perfil.
			auto dot = name.find('.');
			if (dot != std::string::npos) name = name.substr(0, dot);
			if (!name.empty()) ProfileList.push_back(name);
		} while (LI_CACHED(FindNextFileA)(h, &fd));
		LI_CACHED(FindClose)(h);
		ProfileListDirty = false;
	}

	// XOR transform com key fixa de 32 bytes + perturbacao por posicao.
	// Mesmo esquema do ZmInternal/saveconfig.cpp.
	inline void XorTransform(uint8_t* data, size_t len) {
		static const char kKey[33] = "Zm/Cfg.MasterKey.Stealth.2026!~!";
		for (size_t i = 0; i < len; ++i) {
			uint8_t pos = static_cast<uint8_t>((i * 0x9Du) ^ (i >> 3));
			data[i] ^= static_cast<uint8_t>(static_cast<uint8_t>(kKey[i & 31]) ^ pos);
		}
	}

	// Magic header "NHCM\n" — escrito plaintext (ANTES do XOR) e validado no
	// load. Quem abrir o .db sem a chave pega bytes aleatorios.
	inline bool WriteFileData(const std::string& path, const std::string& data) {
		std::string body;
		body.reserve(data.size() + 5);
		body.append(xorstr_("NHCM\n"));
		body.append(data);

		XorTransform(reinterpret_cast<uint8_t*>(body.data()), body.size());

		HANDLE h = LI_CACHED(CreateFileA)(path.c_str(), GENERIC_WRITE, 0, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE) return false;
		DWORD written = 0;
		LI_CACHED(WriteFile)(h, body.c_str(), static_cast<DWORD>(body.size()), &written, nullptr);
		Sys::Close(h);
		return written == body.size();
	}

	inline std::string ReadFileData(const std::string& path) {
		HANDLE h = LI_CACHED(CreateFileA)(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE) return {};
		DWORD size = Sys::GetFileSize(h);
		if (size < 5 || size == INVALID_FILE_SIZE) { Sys::Close(h); return {}; }
		std::vector<uint8_t> buf(size);
		DWORD read = 0;
		LI_CACHED(ReadFile)(h, buf.data(), size, &read, nullptr);
		Sys::Close(h);
		if (read != size) return {};

		XorTransform(buf.data(), buf.size());

		// Checa magic "NHCM\n"
		if (buf.size() < 5 || buf[0] != 'N' || buf[1] != 'H' ||
			buf[2] != 'C' || buf[3] != 'M' || buf[4] != '\n')
			return {};

		return std::string(reinterpret_cast<const char*>(buf.data() + 5), buf.size() - 5);
	}

	inline void AppendLine(std::string& out, const char* key, int val) {
		char buf[128];
		int n = snprintf(buf, sizeof(buf), "%s=%d\n", key, val);
		if (n > 0) out.append(buf, n);
	}

	inline void AppendLine(std::string& out, const char* key, float val, const char* fmt) {
		char buf[128];
		int len = 0;
		if (fmt[1] == '.' && fmt[2] == '0') len = snprintf(buf, sizeof(buf), "%s=%d\n", key, static_cast<int>(val));
		else { char tmp[32]; snprintf(tmp, sizeof(tmp), fmt, val); len = snprintf(buf, sizeof(buf), "%s=%s\n", key, tmp); }
		if (len > 0) out.append(buf, len);
	}

	inline void AppendColor(std::string& out, const char* key, const Color4& c) {
		char buf[128];
		snprintf(buf, sizeof(buf), "%s=%.3f,%.3f,%.3f,%.3f\n", key, c.r, c.g, c.b, c.a);
		out.append(buf);
	}

	inline void SaveTo(const std::string& path) {
		EnsureConfigDir();
		auto& g = GameSettings;
		std::string data;
		data.reserve(2048);

		AppendLine(data, xorstr_("teamCheck"), (int)g.teamCheck);
		AppendLine(data, xorstr_("aimbot"), (int)g.aimbot);
		AppendLine(data, xorstr_("aimFov"), g.aimFov, "%.1f");
		AppendLine(data, xorstr_("aimSmooth"), g.aimSmooth, "%.1f");
		AppendLine(data, xorstr_("aimBone"), g.aimBone);
		AppendLine(data, xorstr_("aimKey"), g.aimKey);
		AppendLine(data, xorstr_("aimRequireVisible"), (int)g.aimRequireVisible);
		AppendLine(data, xorstr_("aimDynamic"), (int)g.aimDynamic);
		AppendLine(data, xorstr_("aimHighlight"), (int)g.aimHighlight);
		AppendLine(data, xorstr_("aimFovCircle"), (int)g.aimFovCircle);
		AppendLine(data, xorstr_("aimHumanize"), (int)g.aimHumanize);
		AppendLine(data, xorstr_("aimAssistMode"), (int)g.aimAssistMode);
		AppendLine(data, xorstr_("aimDistance"), g.aimDistance, "%.0f");
		AppendLine(data, xorstr_("streamMode"), (int)g.streamMode);
		AppendLine(data, xorstr_("noFlash"), (int)g.noFlash);
		AppendLine(data, xorstr_("discordRpc"), (int)g.discordRpc);
		AppendLine(data, xorstr_("fpsLimit"), g.fpsLimit);
		AppendLine(data, xorstr_("rcs"), (int)g.rcs);
		AppendLine(data, xorstr_("rcsStrength"), g.rcsStrength, "%.1f");
		AppendLine(data, xorstr_("triggerbot"), (int)g.triggerbot);
		AppendLine(data, xorstr_("triggerKey"), g.triggerKey);
		AppendLine(data, xorstr_("triggerDelay"), g.triggerDelay);
		AppendLine(data, xorstr_("esp"), (int)g.esp);
		AppendLine(data, xorstr_("espBox"), (int)g.espBox);
		AppendLine(data, xorstr_("espHealth"), (int)g.espHealth);
		AppendLine(data, xorstr_("espName"), (int)g.espName);
		AppendLine(data, xorstr_("espSkeleton"), (int)g.espSkeleton);
		AppendLine(data, xorstr_("espWeapon"), (int)g.espWeapon);
		AppendLine(data, xorstr_("espFlags"), (int)g.espFlags);
		AppendLine(data, xorstr_("espLookDir"), (int)g.espLookDir);
		AppendLine(data, xorstr_("espDistance"), g.espDistance, "%.0f");
		AppendLine(data, xorstr_("bombTimer"), (int)g.bombTimer);
		AppendLine(data, xorstr_("webRadar"), (int)g.webRadar);
		AppendLine(data, xorstr_("menuKey"), MenuKey);

		AppendColor(data, xorstr_("colEnemyVisible"), g.colEnemyVisible);
		AppendColor(data, xorstr_("colEnemyHidden"), g.colEnemyHidden);
		AppendColor(data, xorstr_("colTeam"), g.colTeam);
		AppendColor(data, xorstr_("colTarget"), g.colTarget);
		AppendColor(data, xorstr_("colDynamic"), g.colDynamic);
		AppendColor(data, xorstr_("colSkeleton"), g.colSkeleton);
		AppendColor(data, xorstr_("colLookDir"), g.colLookDir);
		AppendColor(data, xorstr_("colFovCircle"), g.colFovCircle);
		AppendColor(data, xorstr_("colBox"), g.colBox);
		AppendColor(data, xorstr_("colName"), g.colName);
		AppendColor(data, xorstr_("colWeapon"), g.colWeapon);
		AppendColor(data, xorstr_("colBomb"), g.colBomb);
		AppendColor(data, xorstr_("colNade"), g.colNade);

		WriteFileData(path, data);
	}

	inline void Save() { SaveTo(GetProfilePath(ActiveProfile)); ProfileListDirty = true; }

	inline void SaveProfile(const std::string& name) {
		ActiveProfile = name;
		SaveTo(GetProfilePath(name));
		ProfileListDirty = true;
	}

	inline void LoadFrom(const std::string& path) {
		auto& g = GameSettings;
		std::string data = ReadFileData(path);
		if (data.empty()) return;

		auto readColor = [](const char* line, const char* name, Color4& c) -> bool {
			size_t nlen = strlen(name);
			if (strncmp(line, name, nlen) != 0 || line[nlen] != '=') return false;
			sscanf_s(line + nlen + 1, "%f,%f,%f,%f", &c.r, &c.g, &c.b, &c.a);
			return true;
		};

		size_t pos = 0;
		while (pos < data.size()) {
			size_t eol = data.find('\n', pos);
			if (eol == std::string::npos) eol = data.size();
			std::string line = data.substr(pos, eol - pos);
			pos = eol + 1;
			if (line.empty()) continue;
			if (line.back() == '\r') line.pop_back();

			const char* l = line.c_str();

			if (readColor(l, xorstr_("colEnemyVisible"), g.colEnemyVisible)) continue;
			if (readColor(l, xorstr_("colEnemyHidden"), g.colEnemyHidden)) continue;
			if (readColor(l, xorstr_("colTeam"), g.colTeam)) continue;
			if (readColor(l, xorstr_("colTarget"), g.colTarget)) continue;
			if (readColor(l, xorstr_("colDynamic"), g.colDynamic)) continue;
			if (readColor(l, xorstr_("colSkeleton"), g.colSkeleton)) continue;
			if (readColor(l, xorstr_("colLookDir"), g.colLookDir)) continue;
			if (readColor(l, xorstr_("colFovCircle"), g.colFovCircle)) continue;
			if (readColor(l, xorstr_("colBox"), g.colBox)) continue;
			if (readColor(l, xorstr_("colName"), g.colName)) continue;
			if (readColor(l, xorstr_("colWeapon"), g.colWeapon)) continue;
			if (readColor(l, xorstr_("colBomb"), g.colBomb)) continue;
			if (readColor(l, xorstr_("colNade"), g.colNade)) continue;

			auto eq = line.find('=');
			if (eq == std::string::npos) continue;
			std::string k = line.substr(0, eq);
			std::string v = line.substr(eq + 1);
			float fval = static_cast<float>(atof(v.c_str()));
			int ival = atoi(v.c_str());

			if (k == std::string(xorstr_("teamCheck"))) g.teamCheck = ival;
			else if (k == std::string(xorstr_("aimbot"))) g.aimbot = ival;
			else if (k == std::string(xorstr_("aimFov"))) g.aimFov = fval;
			else if (k == std::string(xorstr_("aimSmooth"))) g.aimSmooth = fval;
			else if (k == std::string(xorstr_("aimBone"))) g.aimBone = ival;
			else if (k == std::string(xorstr_("aimKey"))) g.aimKey = ival;
			else if (k == std::string(xorstr_("aimRequireVisible"))) g.aimRequireVisible = ival;
			else if (k == std::string(xorstr_("aimDynamic"))) g.aimDynamic = ival;
			else if (k == std::string(xorstr_("aimHighlight"))) g.aimHighlight = ival;
			else if (k == std::string(xorstr_("aimFovCircle"))) g.aimFovCircle = ival;
			else if (k == std::string(xorstr_("aimHumanize"))) g.aimHumanize = ival;
			else if (k == std::string(xorstr_("aimAssistMode"))) g.aimAssistMode = ival;
			else if (k == std::string(xorstr_("aimDistance"))) g.aimDistance = fval;
			else if (k == std::string(xorstr_("streamMode"))) g.streamMode = ival;
			else if (k == std::string(xorstr_("noFlash"))) g.noFlash = ival;
			else if (k == std::string(xorstr_("discordRpc"))) g.discordRpc = ival;
			else if (k == std::string(xorstr_("fpsLimit"))) g.fpsLimit = ival;
			else if (k == std::string(xorstr_("rcs"))) g.rcs = ival;
			else if (k == std::string(xorstr_("rcsStrength"))) g.rcsStrength = fval;
			else if (k == std::string(xorstr_("triggerbot"))) g.triggerbot = ival;
			else if (k == std::string(xorstr_("triggerKey"))) g.triggerKey = ival;
			else if (k == std::string(xorstr_("triggerDelay"))) g.triggerDelay = ival;
			else if (k == std::string(xorstr_("esp"))) g.esp = ival;
			else if (k == std::string(xorstr_("espBox"))) g.espBox = ival;
			else if (k == std::string(xorstr_("espHealth"))) g.espHealth = ival;
			else if (k == std::string(xorstr_("espName"))) g.espName = ival;
			else if (k == std::string(xorstr_("espSkeleton"))) g.espSkeleton = ival;
			else if (k == std::string(xorstr_("espWeapon"))) g.espWeapon = ival;
			else if (k == std::string(xorstr_("espFlags"))) g.espFlags = ival;
			else if (k == std::string(xorstr_("espLookDir"))) g.espLookDir = ival;
			else if (k == std::string(xorstr_("espDistance"))) g.espDistance = fval;
			else if (k == std::string(xorstr_("bombTimer"))) g.bombTimer = ival;
			else if (k == std::string(xorstr_("webRadar"))) g.webRadar = ival;
			else if (k == std::string(xorstr_("menuKey"))) MenuKey = ival;
		}
	}

	inline void Load() { LoadFrom(GetProfilePath(ActiveProfile)); }

	inline void LoadProfile(const std::string& name) {
		ActiveProfile = name;
		LoadFrom(GetProfilePath(name));
	}

	inline void DeleteProfile(const std::string& name) {
		if (name == "default") return;
		LI_CACHED(DeleteFileA)(GetProfilePath(name).c_str());
		ProfileListDirty = true;
		if (ActiveProfile == name) ActiveProfile = "default";
	}

}
