/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <cwchar>
#include <string>
#include <set>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <detours.h>
#pragma comment(lib, "detours/lib.x86/detours")

using byte = uint8_t;

////////////////////////////////
// 定数定義．
////////////////////////////////
namespace filenames
{
	constexpr const char* TargetFolder = "Fonts";
	constexpr const char* ExcludeFile = "Fonts/Excludes.txt";
}


////////////////////////////////
// 文字列比較．
////////////////////////////////
constexpr struct {
	constexpr static uint32_t CodePage_shiftjis = 932;

	constexpr char tolower(char c) const {
		// https://blog.yimmo.org/posts/faster-tolower-in-c.html
		return c ^ ((((0x40 - c) ^ (0x5a - c)) >> 2) & 0x20);
	}

	constexpr void operator()(char* str) const
	{
		for (bool partial = false; *str != '\0'; str++) {
			auto c = static_cast<uint8_t>(*str);
			if (partial) {
				partial = false;
				uint16_t c16 = (static_cast<uint8_t>(str[-1]) << 8) | c;
				if (0x8260 <= c16 && c16 <= 0x8279) // from 'A' to 'Z' of full-widths'.
					*str += 0x21; // into 'a' to 'z' of full-widths'.
			}
			else if ((0x81 <= c && c <= 0x9f) || (0xe0 <= c && c <= 0xfc)) partial = true;
			else *str = tolower(c);
		}
	}
	constexpr void operator()(wchar_t* str) const
	{
		for (; *str != L'\0'; str++) {
			auto c = static_cast<uint16_t>(*str);
			if (0x0041 <= c && c <= 0x005a) *str ^= 0x0020;
			else if (0xff21 <= c && c <= 0xff3a) *str ^= 0x0060;
		}
	}
} tolower_str;
void trim_string(char* str, size_t& len)
{
	constexpr auto white_spaces = " \t\v\n\r";

	for (auto* end = str + len; --end >= str && std::strchr(white_spaces, *end);) *end = '\0', len--;
	auto start = str; while (len > 0 && std::strchr(white_spaces, *start)) start++, len--;
	std::memmove(str, start, len + 1);
}


////////////////////////////////
// Private フォント追加．
////////////////////////////////
template<size_t N>
void add_fonts(char(&path)[N], size_t tail)
{
	WIN32_FIND_DATAA file{};

	strcat_s(path + tail, N - tail, "/*");
	auto h = ::FindFirstFileA(path, &file);
	if (h == nullptr) return;
	tail++;

	do {
		strcat_s(path + tail, N - tail, file.cFileName);
		if ((file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			// add font privately.
			// file may not be a font... assume the API will handle well.
			::AddFontResourceExA(path, FR_PRIVATE, 0);

		else if (std::strcmp(file.cFileName, ".") != 0 && std::strcmp(file.cFileName, "..") != 0)
			// recursively add fonts in subdirectories.
			add_fonts(path, tail + std::strlen(file.cFileName));

	} while (::FindNextFileA(h, &file));
	::FindClose(h);
}


////////////////////////////////
// 除外リスト．
////////////////////////////////
inline class {
	std::set<std::string> list{};
	std::set<std::wstring> listw{};
	bool is_linecomment(const char* line)
	{
		constexpr const char* begin = "//";
		auto c = begin;
		while (*c != '\0' && *line == *c) line++, c++;
		return *c == '\0';
	}

	// count: current "block level", returns new block level.
	bool is_blockcomment(const char* line, int& count)
	{
		constexpr char symb = '%';
		int cnt = 0;
		while (*line == symb) cnt++, line++;

		if (count <= 0) {
			count = cnt;
			return cnt > 0;
		}
		if (count == cnt) count = 0;
		return true;
	}

public:
	void init(const char* path)
	{
		FILE* file = nullptr;
		if (fopen_s(&file, path, "r") != 0 || file == nullptr) return;

		char line[MAX_PATH];
		int blocklevel = 0;
		while (std::fgets(line, std::size(line), file)) {
			auto len = std::strlen(line);

			// trim the string.
			trim_string(line, len);

			// skip comment lines.
			if (is_blockcomment(line, blocklevel)) continue;
			if (is_linecomment(line)) continue;

			// and those that are too long or empty.
			if (len == 0 || len >= LF_FACESIZE) continue;

			// uniform the casing.
			tolower_str(line);

			// prepare wide string.
			wchar_t linew[LF_FACESIZE];
			::MultiByteToWideChar(tolower_str.CodePage_shiftjis, 0,
				line, len + 1, linew, std::size(linew));

			// then add.
			list.emplace(line);
			listw.emplace(linew);
		}

		std::fclose(file);
	}
	bool operator()(const char* name) { return list.contains(name); }
	bool operator()(const wchar_t* name) { return listw.contains(name); }
} excludes;


////////////////////////////////
// Detours のラッパー．
////////////////////////////////
struct DetourHelper {
	static void Attach(auto&... args) {
		if (!(args.is_effective() || ...)) return;

		::DetourTransactionBegin();
		::DetourUpdateThread(::GetCurrentThread());
		(args.attach(), ...);
		::DetourTransactionCommit();
	}
	static void Detach(auto&... args) {
		if (!(args.is_effective() || ...)) return;

		::DetourTransactionBegin();
		::DetourUpdateThread(::GetCurrentThread());
		(args.detach(), ...);
		::DetourTransactionCommit();
	}

	void set_original(this auto& self, void* f) {
		*reinterpret_cast<void**>(&self.original) = f;
	}
	void set_detour(this auto& self, void* f) {
		*reinterpret_cast<void**>(&self.detour) = f;
	}

private:
	void attach(this auto& self) {
		if (self.is_effective())
			::DetourAttach(reinterpret_cast<void**>(&self.original), self.detour);
	}
	void detach(this auto& self) {
		if (self.is_effective())
			::DetourDetach(reinterpret_cast<void**>(&self.original), self.detour);
	}
	bool is_effective(this const auto& self) {
		return self.original != nullptr && self.detour != nullptr;
	}
};


////////////////////////////////
// EnumFontFamiliesA/W 乗っ取り．
////////////////////////////////
inline struct : DetourHelper {
	static int WINAPI detour(HDC hdc, PCSTR logfont, FONTENUMPROCA proc, LPARAM lparam)
	{
		struct Cxt {
			FONTENUMPROCA proc;
			LPARAM lparam;
		} cxt{ proc, lparam };
		return original(hdc, logfont, [](const LOGFONTA* lf, auto metric, auto type, LPARAM lparam) {
			// filter by the face name.
			char name[std::extent_v<decltype(lf->lfFaceName)>];
			strcpy_s(name, lf->lfFaceName + (*lf->lfFaceName == '@' ? 1 : 0));
			tolower_str(name);
			if (excludes(name)) return TRUE;

			// default behavior otherwise.
			auto* cxt = reinterpret_cast<Cxt*>(lparam);
			return cxt->proc(lf, metric, type, cxt->lparam);
		}, reinterpret_cast<LPARAM>(&cxt));
	}
	static inline decltype(&detour) original = &EnumFontFamiliesA;
} enum_font_families_A;

inline struct : DetourHelper {
	static int WINAPI detour(HDC hdc, PCWSTR logfont, FONTENUMPROCW proc, LPARAM lparam)
	{
		struct Cxt {
			FONTENUMPROCW proc;
			LPARAM lparam;
		} cxt{ proc, lparam };
		return original(hdc, logfont, [](const LOGFONTW* lf, auto metric, auto type, LPARAM lparam) {
			// filter by the face name.
			wchar_t name[std::extent_v<decltype(lf->lfFaceName)>];
			wcscpy_s(name, lf->lfFaceName + (*lf->lfFaceName == L'@' ? 1 : 0));
			tolower_str(name);
			if (excludes(name)) return TRUE;

			// default behavior otherwise.
			auto* cxt = reinterpret_cast<Cxt*>(lparam);
			return cxt->proc(lf, metric, type, cxt->lparam);
		}, reinterpret_cast<LPARAM>(&cxt));
	}
	static inline decltype(&detour) original = &EnumFontFamiliesW;
} enum_font_families_W;


////////////////////////////////
// 初期化処理．
////////////////////////////////
void on_attach(HINSTANCE hinst)
{
	char path[MAX_PATH];
	::GetModuleFileNameA(hinst, path, std::size(path));
	auto* name = path;
	while (auto p = std::strpbrk(name, "\\/")) name = p + 1;

	// increment the reference count of this DLL.
	::LoadLibraryA(name);

	// replace the name to the target directory.
	strcpy_s(name, std::size(path) - (name - path), filenames::TargetFolder);

	// add priavte fonts.
	add_fonts(path, std::strlen(path));

	strcpy_s(name, std::size(path) - (name - path), filenames::ExcludeFile);
	excludes.init(path);

	// override Win32 API.
	DetourHelper::Attach(enum_font_families_A, enum_font_families_W);
}


////////////////////////////////
// Entry point.
////////////////////////////////
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwNotification, LPVOID lpReserved)
{
	switch (dwNotification) {
	case DLL_PROCESS_ATTACH:
		on_attach(hInstance);
		break;
	}
	return TRUE;
}

