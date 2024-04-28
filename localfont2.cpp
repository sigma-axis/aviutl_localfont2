/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <string>
#include <set>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

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
template<uint32_t codepage>
struct Encode {
	constexpr static uint32_t CodePage = codepage;

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
		std::wstring ret(cntw - 1, L'\0');
		to_wide_str(ret.data(), cntw, str, cnt_str);
		return ret;
	}
	static std::wstring to_wide_str(const std::string& str) { return to_wide_str(str.c_str()); }

	static int cnt_narrow_str(const wchar_t* wstr, int cnt_wstr = -1) {
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
		size_t cnt = cnt_narrow_str(wstr, cnt_wstr);
		if (cnt_wstr >= 0 && wstr[cnt_wstr - 1] != L'\0') cnt++;
		std::string ret(cnt - 1, '\0');
		from_wide_str(ret.data(), cnt, wstr, cnt_wstr);
		return ret;
	}
	static std::string from_wide_str(const std::wstring& wstr) { return from_wide_str(wstr.c_str()); }
};
using encode_sjis = Encode<932>;
using encode_utf8 = Encode<CP_UTF8>;
using encode_sys = Encode<CP_ACP>;

// trimming white spaces on the both sides.
inline constinit struct {
	// determining white spaces.
	constexpr static auto white_spaces =
		// ASCII.
		L" \t\v\f\n\r"sv

		// in Shift JIS.
		L"\u3000"sv

		// not in Shift JIS.
		L"\u0085\u00a0"
		L"\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200a"
		L"\u1680\u2028\u2029\u202f\u205f"sv

		// actually not white spaces.
		//L"\u180e\u200b\u200c\u200d\u2060\ufeff"sv
		;

	constexpr std::wstring_view operator()(const std::wstring_view& str) {
		auto st = str.find_first_not_of(white_spaces);
		if (st == std::string_view::npos) return str.substr(0, 0);
		auto ed = str.find_last_not_of(white_spaces);
		return str.substr(st, ed - st);
	}
} trim_string;


////////////////////////////////
// Private フォント追加．
////////////////////////////////
inline void iterate_files_recursively(const std::wstring& path, const auto& on_file) {
	WIN32_FIND_DATAW file{};

	// loop for the recursion.
	[&file, &on_file](this const auto& inner, std::wstring path) {
		auto h = ::FindFirstFileW(path.c_str(), &file);
		if (h == nullptr) return;
		path.pop_back();

		do {
			// skip '.' and '..'
			if (file.cFileName == L"."sv || file.cFileName == L".."sv) continue;

			auto subitem = path + file.cFileName;
			if ((file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				// recursively iterate over subdirectories.
				inner(subitem + L"/*");
			else on_file(subitem, file);
		} while (::FindNextFileW(h, &file));
		::FindClose(h);
	} (path + L"/*");
}
inline void add_fonts(std::wstring path)
{
	std::set<std::wstring> exts{ std::from_range, filenames::Extensions };
	auto is_font_ext = [&exts](const std::wstring& name) {
		auto pos = name.find_last_of(L'.');
		if (pos == std::wstring::npos) return false;
		auto ext = name.substr(pos + 1);
		::CharLowerW(ext.data());
		return exts.contains(ext);
	};

	iterate_files_recursively(path, [&](const std::wstring& file_path, WIN32_FIND_DATAW& file) {
		if (is_font_ext(file_path))
			// add font privately.
			::AddFontResourceExW(file_path.c_str(), FR_PRIVATE, 0);
	});
}


////////////////////////////////
// 除外リスト．
////////////////////////////////
inline class {
	std::set<std::wstring> list{};
	bool white_mode = false;

	// returns whether the current line should skip parsing.
	static constexpr bool is_linecomment(const std::wstring_view& line)
	{
		return line.starts_with(L"//"sv);
	}

	// returns whether the current line should skip parsing.
	static constexpr bool is_blockcomment(const std::wstring_view& line, size_t& blocklevel)
	{
		constexpr wchar_t symb = L'%';
		auto cnt = line.find_first_not_of(symb);
		if (cnt == std::wstring_view::npos) cnt = line.length();

		if (blocklevel == 0) {
			blocklevel = cnt;
			return cnt > 0;
		}
		if (blocklevel == cnt) blocklevel = 0;
		return true;
	}

public:
	bool load(std::wstring path, bool whitelist)
	{
		std::FILE* file;
		if (::_wfopen_s(&file, path.c_str(), L"rt") != 0 || file == nullptr) return false;

		list.clear();

		// skip BOM (EF BB BF).
		for (int c : { 0xEF, 0xBB, 0xBF }) {
			if (std::fgetc(file) != c) { std::rewind(file); break; }
		}

		// traverse each line.
		char line[MAX_PATH];
		size_t blocklevel = 0;
		while (std::fgets(line, std::size(line), file) != nullptr) {
			auto linew = encode_utf8::to_wide_str(line);

			// trim the string.
			auto span = trim_string(linew);

			// skip comment lines,
			if (is_blockcomment(span, blocklevel)) continue;
			if (is_linecomment(span)) continue;

			// and those that are too long or empty.
			if (span.empty() || span.length() >= font_length_max) continue;

			// add the lower-cased string.
			::CharLowerBuffW(const_cast<wchar_t*>(span.data()), span.length());
			list.emplace(span);
		}

		std::fclose(file);

		white_mode = whitelist;
		return list.size() > 0;
	}
	template<class CharT>
	bool operator()(const CharT* name) const
	{
		if constexpr (std::is_same_v<wchar_t, CharT>) {
			std::wstring buf{ trim_string(name) };
			::CharLowerW(buf.data());
			return white_mode ^ list.contains(buf);
		}
		else if constexpr (std::is_same_v<char, CharT>) {
			wchar_t buf[font_length_max];
			encode_sys::to_wide_str(buf, name);
			return (*this)(buf);
		}
		std::unreachable();
	}
	auto count() const { return list.size(); }
	bool is_whitelist()const { return white_mode; }
	constexpr static size_t font_length_max = LF_FACESIZE;
} excludes;


////////////////////////////////
// Detours のラッパー．
////////////////////////////////
struct DetourHelper {
	static void Attach(auto&... args) {
		::DetourTransactionBegin();
		(args.attach(), ...);
		::DetourTransactionCommit();
	}
	static void Detach(auto&... args) {
		::DetourTransactionBegin();
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
	template<auto& original, auto at_char, class ProcT>
	static int WINAPI detour_template(HDC hdc, const decltype(at_char)* logfont, ProcT proc, LPARAM lparam)
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
	constexpr static auto& detour = detour_template<original, '@', FONTENUMPROCA>;
} enum_font_families_A;

constexpr struct : EnumFontFamiliesBase {
	// other plugins may use this API, such as textassist.auf by oov.
	static inline auto* original = &::EnumFontFamiliesW;
	constexpr static auto& detour = detour_template<original, L'@', FONTENUMPROCW>;
} enum_font_families_W;


////////////////////////////////
// 初期化処理．
////////////////////////////////
inline void on_attach(HINSTANCE hinst)
{
	std::wstring path(MAX_PATH - 1, L'\0');
	path.erase(::GetModuleFileNameW(hinst, path.data(), path.length() + 1));

	// backup the dll file name for the future use.
	static_assert(std::wstring::npos + 1 == 0);
	auto dllname = path.substr(path.find_last_of(L"/\\"sv) + 1);

	// set `path` as the base of file paths.
	path.erase(path.length() - dllname.length());

	// add priavte fonts.
	add_fonts(path + filenames::TargetFolder);

	// create the exclusion list.
	if (!excludes.load(path + filenames::WhitelistFile, true))
		excludes.load(path + filenames::ExcludeFile, false);

	// override Win32 API.
	if (excludes.count() > 0) {
		// needs to increment the reference count of this DLL.
		::LoadLibraryW(dllname.c_str());
		DetourHelper::Attach(enum_font_families_A, enum_font_families_W);
	}
}
inline void on_detach()
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


////////////////////////////////
// バージョン情報．
////////////////////////////////
extern "C" __declspec(dllexport) const char* __stdcall ThisAulVersion(void)
{
	return "v1.20-pre1";
}
