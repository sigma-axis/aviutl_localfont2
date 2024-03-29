/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <tuple>
#include <cwchar>
#include <string>
#include <set>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <Shlwapi.h>
#pragma comment(lib, "shlwapi")

#include "detours/include/detours.h"
#pragma comment(lib, "detours/lib.x86/detours")

using byte = uint8_t;
using namespace std::string_view_literals;

////////////////////////////////
// 定数定義．
////////////////////////////////
namespace filenames
{
	constexpr auto TargetFolder = L"Fonts";
	constexpr auto ExcludeFile = L"Fonts/Excludes.txt";
	constexpr auto WhitelistFile = L"Fonts/Whitelist.txt";
	constexpr const wchar_t* Extensions[] = { L"fon", L"fnt", L"ttf", L"ttc", L"fot", L"otf", L"mmm", L"pfb", L"pfm" };
}


////////////////////////////////
// 文字列操作．
////////////////////////////////
struct sjis {
	constexpr static uint32_t CodePage = 932;

	// conversion between wide character string.
	static int cnt_wide_str(const char* str, int cnt_str = -1) {
		return to_wide_str(nullptr, 0, str, cnt_str);
	}
	static int to_wide_str(wchar_t* wstr, int cnt_wstr, const char* str, int cnt_str = -1) {
		return ::MultiByteToWideChar(CodePage, 0, str, cnt_str, wstr, cnt_wstr);
	}
	template<size_t cnt_wstr>
	static int to_wide_str(wchar_t(&wstr)[cnt_wstr], const char* str, int cnt_str = -1) {
		return to_wide_str(wstr, int{ cnt_wstr }, str, cnt_str);
	}
	static std::wstring to_wide_str(const char* str, int cnt_str = -1) {
		size_t cntw = cnt_wide_str(str, cnt_str);
		if (cnt_str >= 0 && str[cnt_str - 1] != '\0') cntw++;
		std::wstring ret{ cntw - 1, L'\0', std::allocator<wchar_t>{} };
		to_wide_str(ret.data(), cntw, str, cnt_str);
		return ret;
	}
	static std::wstring to_wide_str(const std::string& str) { return to_wide_str(str.c_str()); }

	static int cnt_sjis_str(const wchar_t* wstr, int cnt_wstr = -1) {
		return from_wide_str(nullptr, 0, wstr, cnt_wstr);
	}
	static int from_wide_str(char* str, int cnt_str, const wchar_t* wstr, int cnt_wstr = -1) {
		return ::WideCharToMultiByte(CodePage, 0, wstr, cnt_wstr, str, cnt_str, nullptr, nullptr);
	}
	template<size_t cnt_str>
	static int from_wide_str(char(&str)[cnt_str], const wchar_t* wstr, int cnt_wstr = -1) {
		return from_wide_str(str, int{ cnt_str }, wstr, cnt_wstr);
	}
	static std::string from_wide_str(const wchar_t* wstr, int cnt_wstr = -1) {
		size_t cnt = cnt_sjis_str(wstr, cnt_wstr);
		if (cnt_wstr >= 0 && wstr[cnt_wstr - 1] != L'\0') cnt++;
		std::string ret{ cnt - 1, '\0', std::allocator<char>{} };
		from_wide_str(ret.data(), cnt, wstr, cnt_wstr);
		return ret;
	}
	static std::string from_wide_str(const std::wstring& wstr) { return from_wide_str(wstr.c_str()); }

	// multibyte parsing.
	constexpr static bool is_leading(char c) {
		return ('\x81' <= c && c <= '\x9f') || ('\xe0' <= c && c <= '\xfc');
	}
	constexpr static bool is_second(char c) {
		return ('\x40' <= c && c <= '\x7e') || ('\x80' <= c && c <= '\xfc');
	}

	// casing control.
	constexpr static char tolower(char c) {
		// https://blog.yimmo.org/posts/faster-tolower-in-c.html
		// needs to shift one more bit than the reference;
		// c = '\xc1' would fail otherwise.
		return c ^ ((((0x40 - c) ^ (0x5a - c)) >> 3) & 0x20);
	}
	constexpr static char tolower(char c1, char c2) {
		// lowers 'A' to 'Z' of full-widths'.
		return c1 != '\x82' ? c2 :
			c2 + ((((0x5f - c2) ^ (0x79 - c2)) >> 8) & 0x21);
	}
	constexpr static wchar_t tolower(wchar_t c) {
		return c
			^ ((((0x0040 - c) ^ (0x005a - c)) >> 11) & 0x0020) // narrow 'A' to 'Z'.
			^ ((((0xff20 - c) ^ (0xff3a - c)) >> 11) & 0x0060); // full-width 'A' to 'Z'.
	}

	// determining white spaces.
	constexpr static auto white_spaces = " \t\v\f\n\r"sv;
	constexpr static uint16_t white_space_sjis = 0x8140;
	constexpr static auto white_spaces_w = L" \t\v\f\n\r\u3000"sv;
	constexpr static auto white_spaces_w2 =
		L"\u0085\u00a0" L"\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200a"
		L"\u1680\u2028\u2029\u202f\u205f"sv; // those not in Shift JIS.
	constexpr static auto white_spaces_w3 =
		L"\u180e\u200b\u200c\u200d\u2060\ufeff"sv; // those not actually white spaces.

	constexpr static bool is_space(char c) { return white_spaces.contains(c); }
	constexpr static bool is_space(char c1, char c2) {
		return ((static_cast<uint8_t>(c1) << 8) | static_cast<uint8_t>(c2)) == white_space_sjis;
	}
	constexpr static bool is_space(wchar_t c) {
		return white_spaces_w.contains(c)
			/*|| white_spaces_w2.contains(c)
			|| white_spaces_w3.contains(c)*/;
	}
};

// lower casing.
constexpr struct : sjis {
	constexpr void operator()(char* str) const
	{
		for (bool partial = false; *str != '\0'; str++) {
			if (partial) {
				partial = false;
				*str = tolower(str[-1], *str);
			}
			else if (is_leading(*str)) partial = true;
			else *str = tolower(*str);
		}
	}
	constexpr void operator()(wchar_t* str) const
	{
		for (; *str != L'\0'; str++) *str = tolower(*str);
	}
	template<class CharT>
	constexpr void operator()(std::basic_string<CharT>& str) const { (*this)(str.data()); }
} tolower_str;

// trimming white spaces on the both sides.
constexpr struct : sjis {
private:
	// returns (pos, len)-pair.
	constexpr static std::pair<size_t, size_t> locate(const char* str)
	{
		const char* st = str, * ed;

		// find the beginning.
		bool partial = false;
		for (; *st != '\0'; st++) {
			if (partial) {
				if (is_space(st[-1], *st)) { partial = false; continue; }
			}
			else if (is_leading(*st)) { partial = true; continue; }
			else if (is_space(*st)) continue;

			ed = st + 1;
			// rewind back to the first byte of the double byte character.
			if (partial) { partial = false; st--; }

			// find the tail.
			for (auto cur = ed; *cur != '\0'; cur++) {
				if (partial) {
					partial = false;
					if (is_space(cur[-1], *cur)) continue;
				}
				else if (is_leading(*cur)) { partial = true; continue; }
				else if (is_space(*cur)) continue;

				ed = cur + 1;
			}
			return { st - str, ed - st };
		}

		// all characters are white spaces.
		return { 0, 0 }; 
	}
	// returns (pos, len)-pair.
	constexpr static std::pair<size_t, size_t> locate(const wchar_t* str)
	{
		const wchar_t* st = str, * ed;

		// find the beginning.
		for (; *st != L'\0'; st++) {
			if (is_space(*st)) continue;
			ed = st + 1;

			// find the tail.
			for (auto cur = ed; *cur != L'\0'; cur++) {
				if (is_space(*cur)) continue;
				ed = cur + 1;
			}
			return { st - str, ed - st };
		}

		// all characters are white spaces.
		return { 0, 0 };
	}

public:
	constexpr auto operator()(auto* str) const {
		auto [pos, len] = locate(str);
		return std::make_pair(str + pos, len);
	}
} trim_string;


////////////////////////////////
// Private フォント追加．
////////////////////////////////
template<size_t N>
void add_fonts(wchar_t(&path)[N])
{
	std::set<std::wstring> exts{ std::from_range, filenames::Extensions };
	auto is_font_ext = [&exts](wchar_t* name) {
		auto p = ::PathFindExtensionW(name);
		if (*p == L'\0') return false;
		tolower_str(++p);
		return exts.contains(p);
	};
	WIN32_FIND_DATAW file{};

	// core loop for the recursion.
	[&](this const auto& inner, size_t tail) {
		if (wcscpy_s(path + tail, N - tail, L"/*") != 0) return;
		auto h = ::FindFirstFileW(path, &file);
		if (h == nullptr) return;
		tail++;

		do {
			// skip '.' and '..'
			if (file.cFileName == L"."sv || file.cFileName == L".."sv) continue;

			if (wcscpy_s(path + tail, N - tail, file.cFileName) != 0) break;
			if ((file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				// recursively add fonts in subdirectories.
				inner(tail + std::wcslen(file.cFileName));
			else if (is_font_ext(file.cFileName))
				// add font privately.
				::AddFontResourceExW(path, FR_PRIVATE, 0);

		} while (::FindNextFileW(h, &file));
		::FindClose(h);
	} (std::wcslen(path));
}


////////////////////////////////
// 除外リスト．
////////////////////////////////
inline class {
	std::set<std::string> list{};
	std::set<std::wstring> listw{};
	bool white_mode = false;

	// returns whether the current line should skip parsing.
	static constexpr bool is_linecomment(const char* line)
	{
		constexpr const char* begin = "//";
		auto c = begin;
		while (*c != '\0' && *line == *c) line++, c++;
		return *c == '\0';
	}

	// returns whether the current line should skip parsing.
	static constexpr bool is_blockcomment(const char* line, uint32_t& blocklevel)
	{
		constexpr char symb = '%';
		uint32_t cnt = 0;
		while (line[cnt] == symb) cnt++;

		if (blocklevel == 0) {
			blocklevel = cnt;
			return cnt > 0;
		}
		if (blocklevel == cnt) blocklevel = 0;
		return true;
	}

public:
	bool load(const wchar_t* path, bool whitelist)
	{
		std::FILE* file = nullptr;
		if (_wfopen_s(&file, path, L"r") != 0 || file == nullptr) return false;

		char line[MAX_PATH];
		uint32_t blocklevel = 0;
		while (std::fgets(line, std::size(line), file)) {
			// trim the string.
			auto [begin, len] = trim_string(line);
			begin[len] = '\0';

			// skip comment lines,
			if (is_blockcomment(begin, blocklevel)) continue;
			if (is_linecomment(begin)) continue;

			// and those that are too long or empty.
			if (len == 0 || len >= LF_FACESIZE) continue;

			// uniform the casing.
			tolower_str(begin);

			// prepare the wide string.
			wchar_t linew[LF_FACESIZE];
			sjis::to_wide_str(linew, begin);

			// then add.
			list.emplace(begin);
			listw.emplace(linew);
		}

		std::fclose(file);

		white_mode = whitelist;
		return true;
	}
	template<class CharT>
	bool operator()(const CharT* name) const
	{
		auto [pos, len] = trim_string(name);
		std::basic_string<CharT> buf{ pos, len };
		tolower_str(buf);
		if constexpr (std::is_same_v<CharT, wchar_t>)
			return listw.contains(buf) ^ white_mode;
		else return list.contains(buf) ^ white_mode;
	}
	auto count() const { return list.size(); }
	bool is_whitelist()const { return white_mode; }
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
struct EnumFontFamiliesBase : DetourHelper {
protected:
	template<auto& original, class StrT, class ProcT, auto at_char>
	static int WINAPI detour_template(HDC hdc, StrT logfont, ProcT proc, LPARAM lparam)
	{
		auto cxt = std::make_pair(proc, lparam);
		return original(hdc, logfont, [](auto lf, auto metric, auto type, LPARAM lparam) {
			// filter by the face name.
			if (excludes(lf->lfFaceName + (lf->lfFaceName[0] == at_char ? 1 : 0))) return TRUE;

			// default behavior otherwise.
			auto [proc, lp] = *reinterpret_cast<decltype(cxt)*>(lparam);
			return proc(lf, metric, type, lp);
		}, reinterpret_cast<LPARAM>(&cxt));
	}
};

constexpr struct : EnumFontFamiliesBase {
	// exedit.auf uses this API.
	static inline auto* original = &::EnumFontFamiliesA;
	constexpr static auto& detour = detour_template<original, PCSTR, FONTENUMPROCA, '@'>;
} enum_font_families_A;

constexpr struct : EnumFontFamiliesBase {
	// other plugins may use this API, such as textassist.auf by oov.
	static inline auto* original = &::EnumFontFamiliesW;
	constexpr static auto& detour = detour_template<original, PCWSTR, FONTENUMPROCW, L'@'>;
} enum_font_families_W;


////////////////////////////////
// 初期化処理．
////////////////////////////////
void on_attach(HINSTANCE hinst)
{
	wchar_t path[MAX_PATH];
	::GetModuleFileNameW(hinst, path, std::size(path));

	auto* name = ::PathFindFileNameW(path);
	auto const size_name = std::size(path) - (name - path);

	// backup the dll file name for the future use.
	std::wstring dllname{ name };

	// add priavte fonts.
	wcscpy_s(name, size_name, filenames::TargetFolder);
	add_fonts(path);

	// create the exclusion list.
	wcscpy_s(name, size_name, filenames::WhitelistFile);
	if (!excludes.load(path, true) || excludes.count() == 0) {
		wcscpy_s(name, size_name, filenames::ExcludeFile);
		excludes.load(path, false);
	}

	// override Win32 API.
	if (excludes.count() > 0) {
		// needs to increment the reference count of this DLL.
		::LoadLibraryW(dllname.c_str());
		DetourHelper::Attach(enum_font_families_A, enum_font_families_W);
	}
}
void on_detach()
{
	// restore the API.
	if (excludes.count() > 0)
		// will never be called probably.
		DetourHelper::Detach(enum_font_families_A, enum_font_families_W);
}


////////////////////////////////
// Entry point.
////////////////////////////////
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwNotification, LPVOID lpReserved)
{
	switch (dwNotification) {
	case DLL_PROCESS_ATTACH:
		::DisableThreadLibraryCalls(hInstance);
		on_attach(hInstance);
		break;
	case DLL_PROCESS_DETACH:
		if (lpReserved == nullptr) on_detach();
		break;
	}
	return TRUE;
}

