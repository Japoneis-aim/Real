#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <zmhttp/zmhttp.hpp>
#include <core/context.h>
#include <nt/wrap.h>  // Sys::GetExePathA
#include "radar.h"
#include <string>

namespace Web {
	using namespace Context;

	std::string BuildJSON();

	inline std::string GetExeDir() {
		// Sys::GetExePathA le PEB->ProcessParameters->ImagePathName direto
		// (mesmo dado que kernel32!GetModuleFileNameA(NULL) retornaria),
		// sem ida ao kernel32 nem ao loader.
		char path[MAX_PATH]{};
		Sys::GetExePathA(path, MAX_PATH);
		std::string s(path);
		auto pos = s.find_last_of("\\/");
		return (pos != std::string::npos) ? s.substr(0, pos) : ".";
	}

	void Start(int port = 8888);
	void Stop();
	void Restart(int port);
	int  CurrentPort();
	bool IsRunning();
}
