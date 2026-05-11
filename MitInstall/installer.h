#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winspool.h>
#include <shellapi.h>

// Returns TRUE on success
BOOL MitInstall(const wchar_t* pInstallDir);
