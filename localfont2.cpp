/*
The MIT License (MIT)

Copyright (c) 2024-2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <memory>
#include <string>
#include <set>
#include <map>
#include <optional>

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
	constexpr auto ExcludesFile = L"Fonts/Excludes.txt";
	constexpr auto WhitelistFile = L"Fonts/Whitelist.txt";
	constexpr auto AliasFile = L"Fonts/Aliases.txt";
	constexpr wchar_t const* Extensions[] = { L"fon", L"fnt", L"ttf", L"ttc", L"fot", L"otf", L"mmm", L"pfb", L"pfm" };
}


////////////////////////////////
// 文字列操作．
////////////////////////////////
template<uint32_t codepage>
struct Encode {
	constexpr static uint32_t CodePage = codepage;

	// conversion between wide character string.
	static int cnt_wide_str(char const* str, int cnt_str = -1) {
		return to_wide_str(nullptr, 0, str, cnt_str);
	}
	static int to_wide_str(wchar_t* wstr, int cnt_wstr, char const* str, int cnt_str = -1) {
		return ::MultiByteToWideChar(CodePage, 0, str, cnt_str, wstr, cnt_wstr);
	}
	template<size_t cnt_wstr>
	static int to_wide_str(wchar_t(&wstr)[cnt_wstr], char const* str, int cnt_str = -1) {
		return to_wide_str(wstr, int{ cnt_wstr }, str, cnt_str);
	}
	static std::wstring to_wide_str(char const* str, int cnt_str = -1) {
		size_t cntw = cnt_wide_str(str, cnt_str);
		if (cntw == 0 || (cnt_str >= 0 && str[cnt_str - 1] != '\0')) cntw++;
		std::wstring ret(cntw - 1, L'\0');
		to_wide_str(ret.data(), cntw, str, cnt_str);
		return ret;
	}
	static std::wstring to_wide_str(std::string const& str) { return to_wide_str(str.c_str()); }

	static int cnt_narrow_str(wchar_t const* wstr, int cnt_wstr = -1) {
		return from_wide_str(nullptr, 0, wstr, cnt_wstr);
	}
	static int from_wide_str(char* str, int cnt_str, wchar_t const* wstr, int cnt_wstr = -1) {
		return ::WideCharToMultiByte(CodePage, 0, wstr, cnt_wstr, str, cnt_str, nullptr, nullptr);
	}
	template<size_t cnt_str>
	static int from_wide_str(char(&str)[cnt_str], wchar_t const* wstr, int cnt_wstr = -1) {
		return from_wide_str(str, int{ cnt_str }, wstr, cnt_wstr);
	}
	static std::string from_wide_str(wchar_t const* wstr, int cnt_wstr = -1) {
		size_t cnt = cnt_narrow_str(wstr, cnt_wstr);
		if (cnt == 0 || (cnt_wstr >= 0 && wstr[cnt_wstr - 1] != L'\0')) cnt++;
		std::string ret(cnt - 1, '\0');
		from_wide_str(ret.data(), cnt, wstr, cnt_wstr);
		return ret;
	}
	static std::string from_wide_str(std::wstring const& wstr) { return from_wide_str(wstr.c_str()); }
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

	constexpr std::wstring_view operator()(std::wstring_view const& str) {
		auto st = str.find_first_not_of(white_spaces);
		if (st == std::string_view::npos) return str.substr(0, 0);
		auto ed = str.find_last_not_of(white_spaces);
		return str.substr(st, ed - st + 1);
	}
} trim_string;

// templated string copy functions.
template<class T>
concept char_type = std::same_as<T, char> || std::same_as<T, wchar_t>;
template<size_t N>
static inline void copy_from_wide(char(&dest)[N], wchar_t const* src) { encode_sys::from_wide_str(dest, src); }
template<size_t N>
static inline void copy_from_wide(wchar_t(&dest)[N], wchar_t const* src) { ::wcscpy_s(dest, src); }

// lower casing.
static inline std::wstring to_lower(std::wstring_view const& str)
{
	std::wstring ret{ str };
	::CharLowerBuffW(ret.data(), ret.length());
	return ret;
}
static inline auto to_lower(std::wstring_view const& str, size_t pos, size_t count = std::wstring_view::npos) {
	return to_lower(str.substr(pos, count));
}
template<size_t N>
static inline std::wstring to_lower(wchar_t const(&str)[N]) {
	return to_lower(std::wstring_view{ str, ::wcsnlen(str, N - 1) });
}

// wrapping class to read a text file.
struct TextReader {
	std::FILE* file;
	int current_line = 0;
	TextReader(std::wstring const& path)
	{
		if (::_wfopen_s(&file, path.c_str(), L"rt") != 0 || file == nullptr) return;

		// skip BOM (EF BB BF).
		for (int c : { 0xEF, 0xBB, 0xBF }) {
			if (std::fgetc(file) != c) { std::rewind(file); break; }
		}
	}

	template<size_t line_len = MAX_PATH>
	std::optional<std::wstring> read_line()
	{
		char line[line_len];
		if (std::fgets(line, std::size(line), file) == nullptr) return std::nullopt;
		current_line++;
		return encode_utf8::to_wide_str(line);
	}

	// returns whether the current line should skip parsing.
	static constexpr bool is_linecomment(std::wstring_view const& line)
	{
		return line.starts_with(L"//"sv);
	}

	size_t block_level = 0;
	// returns whether the current line should skip parsing.
	constexpr bool is_blockcomment(std::wstring_view const& line)
	{
		constexpr wchar_t symb = L'%';
		auto cnt = line.find_first_not_of(symb);
		if (cnt == std::wstring_view::npos) cnt = line.length();

		if (block_level == 0) {
			block_level = cnt;
			return cnt > 0;
		}

		if (block_level == cnt) block_level = 0;
		return true;
	}

	operator bool() const { return file != nullptr; }
	void close() { if (*this) std::fclose(file), file = nullptr; }
	~TextReader() { close(); }
};


////////////////////////////////
// Private フォント追加．
////////////////////////////////
inline void iterate_files_recursively(std::wstring const& path, auto const& on_file) {
	WIN32_FIND_DATAW file{};

	// loop for the recursion.
	[&file, &on_file](this auto const& inner, std::wstring path) {
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
	auto is_font_ext = [&exts](std::wstring const& name) {
		auto pos = name.find_last_of(L'.');
		if (pos == std::wstring::npos) return false;
		return exts.contains(to_lower(name, pos + 1));
	};

	iterate_files_recursively(path, [&](std::wstring const& file_path, WIN32_FIND_DATAW& file) {
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

	bool load(TextReader&& file)
	{
		if (!file) return false;

		list.clear();

		// traverse each line.
		while (auto linew = file.read_line()) {
			// trim the string.
			auto span = trim_string(*linew);

			// skip comment lines,
			if (file.is_blockcomment(span)) continue;
			if (file.is_linecomment(span)) continue;

			// and those that are too long or empty.
			if (span.empty() || span.length() >= font_length_max) continue;

			// add the lower-cased string.
			::CharLowerBuffW(const_cast<wchar_t*>(span.data()), span.length());
			list.emplace(span);
		}

		return true;
	}

public:
	bool load(std::wstring path, bool whitelist)
	{
		if (!load(path)) return false;

		white_mode = whitelist;
		return list.size() > 0;
	}
	bool operator()(std::wstring_view const& font_name) const {
		std::wstring buf{ trim_string(font_name) };
		if (buf.length() > 0 && buf[0] == at_char) buf.erase(0, 1);
		::CharLowerBuffW(buf.data(), buf.length());
		return white_mode ^ list.contains(buf);
	}
	template<size_t N>
	bool operator()(wchar_t const(&font_name)[N]) const {
		return (*this)(std::wstring_view{ font_name, ::wcsnlen(font_name, N - 1)});
	}
	bool operator()(std::string_view const& font_name) const {
		auto buf = encode_sys::to_wide_str(font_name.data(), font_name.size());
		if (buf.length() == 0) return white_mode; // treat as an unknown name.
		return (*this)(buf);
	}
	template<size_t N>
	bool operator()(char const(&font_name)[N]) const {
		return (*this)(std::string_view{ font_name, ::strnlen(font_name, N) });
	}

	auto count() const { return list.size(); }
	bool is_whitelist()const { return white_mode; }
	constexpr static size_t font_length_max = LF_FACESIZE;
	constexpr static wchar_t at_char = L'@';
} excludes;


////////////////////////////////
// フォントエイリアス．
////////////////////////////////
inline class {
	struct names {
		std::wstring alias;
		std::wstring font; // record as @-headed.
		names() = default;
		names(std::wstring_view a, std::wstring_view f) : alias{ a }, font{ f } {}
	};
	std::map<std::wstring, names> list{};
	std::map<HGDIOBJ, std::wstring> fake_fonts{};
	static std::wstring at_headed(std::wstring_view name) {
		// return the at-headed std::wstring.
		auto ret = std::wstring{};
		ret.resize_and_overwrite(name.length() + 1,
		[&](wchar_t* p, size_t n) {
			*p = at_char;
			std::char_traits<wchar_t>::copy(p + 1, name.data(), n - 1);
			return n;
		});
		return ret;
	}

	// fallback value of ::EnumFontFamiliesA/W for aliases.
	static inline std::unique_ptr<std::tuple<LOGFONTW, TEXTMETRICW, LOGFONTA, TEXTMETRICA, DWORD>> info_fallback{};
	static void prepare_info_fallback()
	{
		if (info_fallback) return;
		info_fallback = std::make_unique<decltype(info_fallback)::element_type>();

		auto hdc = ::GetDC(nullptr);
		::EnumFontFamiliesW(hdc, nullptr, [](LOGFONTW const* lf, TEXTMETRICW const* metric, DWORD type, LPARAM lp) {
			auto& info = *reinterpret_cast<decltype(info_fallback)*>(lp);
			std::get<0>(*info) = *lf;
			std::get<1>(*info) = *metric;
			std::get<4>(*info) = type;
			return FALSE;
		}, reinterpret_cast<LPARAM>(&info_fallback));
		::EnumFontFamiliesA(hdc, nullptr, [](LOGFONTA const* lf, TEXTMETRICA const* metric, DWORD type, LPARAM lp) {
			auto& info = *reinterpret_cast<decltype(info_fallback)*>(lp);
			std::get<2>(*info) = *lf;
			std::get<3>(*info) = *metric;
			return FALSE;
		}, reinterpret_cast<LPARAM>(&info_fallback));
		::ReleaseDC(nullptr, hdc);
	}

	bool load(TextReader&& file)
	{
		if (!file) return false;

		list.clear();

		// traverse each line.
		while (auto linew = file.read_line()) {
			// trim the string.
			auto span = trim_string(*linew);

			// skip comment lines.
			if (span.empty()) continue;
			if (file.is_blockcomment(span)) continue;
			if (file.is_linecomment(span)) continue;

			// split the line with '='.
			std::wstring_view alias, target;
			size_t pos, len;
			if ((pos = span.find_first_of(L'=')) == std::wstring_view::npos ||
				(alias = trim_string(span.substr(0, pos))).empty() || alias[0] == at_char ||
				(target = trim_string(span.substr(pos + 1))).empty() || target[0] == at_char ||
				(len = encode_sys::cnt_narrow_str(alias.data(), alias.size())) == 0 ||
				len + 1 >= font_length_max - 1 || target.size() >= font_length_max - 1 ||
				// add an entry, but disallowing duplicated element.
				!list.try_emplace(to_lower(alias), alias, at_headed(target)).second) {
				::printf_s("[Local Font 2]: Aliases.txt の [%d] 行目は無視されます: %s\n",
					file.current_line, encode_sys::from_wide_str(span.data(), span.size()).c_str());
			}
		}

		return list.size() > 0;
	}

public:
	void load(std::wstring const& path) {
		if (!load(TextReader{ path })) return;

		// prepare other related data.
		prepare_info_fallback();
	}

	wchar_t const* operator()(wchar_t const* alias) const
	{
		// normalize the (possible) alias.
		auto buf = trim_string(alias);
		bool const has_at = buf.length() > 0 && buf[0] == at_char;
		if (has_at) buf = buf.substr(1);

		auto const it = list.find(to_lower(buf));
		if (it == list.end()) return nullptr; // not an alias.

		// return the target name, removing the heading '@' if necessary.
		return it->second.font.c_str() + (has_at ? 0 : 1);
	}
	wchar_t const* operator()(char const* alias) const {
		return (*this)(encode_sys::to_wide_str(alias).c_str());
	}
	template<size_t N>
	bool operator()(wchar_t const* alias, wchar_t(&font_name)[N]) const
	{
		if (auto target = (*this)(alias); target != nullptr) {
			// copy to the destination buffer.
			::wcscpy_s(font_name, target);
			return true;
		}
		else return false; // not an alias.
	}

	// on_alias is a function object called as:
	// on_alias(std::basic_string_view<CharT> alias, std::basic_string_view<CharT> font_name)
	// both alias and font_name are null-terminated at the end of string_view.
	// it returns bool to indicate if or not to continue the iteration.
	template<char_type CharT>
	void for_alias(auto const& on_alias) const {
		if constexpr (std::same_as<CharT, wchar_t>) {
			for (auto const& [_, v] : list) {
				if (!on_alias(std::wstring_view{ v.alias }, std::wstring_view{ v.font }.substr(1)))
					break;
			}
		}
		else for_alias<wchar_t>([&](std::wstring_view const& k, std::wstring_view const& v) {
			return on_alias(k, std::string_view{ encode_sys::from_wide_str(v.data()) });
		});
	}

	template<char_type CharT>
	auto font_info_fallback() const {
		if constexpr (std::same_as<CharT, wchar_t>)
			return std::forward_as_tuple(std::get<0>(*info_fallback), std::get<1>(*info_fallback), std::get<4>(*info_fallback));
		else
			return std::forward_as_tuple(std::get<2>(*info_fallback), std::get<3>(*info_fallback), std::get<4>(*info_fallback));
	}

	HFONT add_fake_font(HFONT font, wchar_t const* name)
	{
		// add to the "fake" font list (except null).
		if (font != nullptr)
			fake_fonts.emplace(font, std::wstring{ name, ::wcsnlen(name, font_length_max - 1) });
		return font;
	}
	bool remove_fake_font(HGDIOBJ font) {
		// remove from the "fake" font list if any.
		return fake_fonts.erase(font) > 0;
	}
	wchar_t const* find_fake_font(HGDIOBJ font) const {
		// retrieve the alias used to create the font, or nullptr if not a "fake" name.
		auto it = fake_fonts.find(font);
		return it != fake_fonts.end() ? it->second.c_str() : nullptr;
	}

	auto count() const { return list.size(); }
	constexpr static size_t font_length_max = excludes.font_length_max;
	constexpr static wchar_t at_char = L'@';
} aliases;


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
	bool is_effective(this auto const& self) {
		return self.original != nullptr && self.detour != nullptr;
	}
};


////////////////////////////////
// フォント系関数乗っ取り．
////////////////////////////////

// EnumFontFamiliesA/W.
struct EnumFontFamiliesBase : DetourHelper {
protected:
	template<auto& original, char_type CharT, class ProcT>
	static int WINAPI detour_template(HDC hdc, CharT const* logfont, ProcT proc, LPARAM lparam)
	{
		// if the name is specified, follow the original behavior.
		if (logfont != nullptr) return original(hdc, logfont, proc, lparam);

		// first, enumerate the real fonts, filtering by the exclusion list.
		int ret;
		if (excludes.count() > 0) {
			auto cxt = std::pair{ proc, lparam };
			ret = original(hdc, logfont, [](auto lf, auto metric, auto type, LPARAM lparam) {
				if (excludes(lf->lfFaceName) || aliases(lf->lfFaceName) != nullptr) return 1;

				// default behavior otherwise.
				auto [proc, lp] = *reinterpret_cast<decltype(cxt)*>(lparam);
				return proc(lf, metric, type, lp);
			}, reinterpret_cast<LPARAM>(&cxt));
		}
		else ret = original(hdc, logfont, proc, lparam);

		// then enumerate aliases if any.
		if (ret != 0 && aliases.count() > 0)
			aliases.for_alias<CharT>([&](auto const& alias, auto const& font_name) {
				// see if it should be excluded.
				if (excludes(alias)) return true;

				// first try to search with the target name, and call back with the first match.
				auto cxt = std::tuple{ proc, lparam, &alias, &ret };
				if (original(hdc, font_name.data(), [](auto lf, auto metric, auto type, LPARAM lparam) {
					auto& [proc, lp, alias, ret] = *reinterpret_cast<decltype(cxt)*>(lparam);

					auto lf2 = *lf;
					copy_from_wide(lf2.lfFaceName, alias->data());
					*ret = proc(&lf2, metric, type, lp);

					// only need the first match. stop the enumeration.
					return 0;
				}, reinterpret_cast<LPARAM>(&cxt)) != 0) { // returns TRUE if no match found.
					// if no match was found, call back with the fallback data.
					auto const& [lf, metric, type] = aliases.font_info_fallback<CharT>();
					copy_from_wide(lf.lfFaceName, alias.data());
					ret = proc(&lf, &metric, type, lparam);
				}

				return ret != 0;
			});
		return ret;
	}
};

constexpr struct : EnumFontFamiliesBase {
	// exedit.auf uses this API.
	static inline auto* original = &::EnumFontFamiliesA;
	constexpr static auto& detour = detour_template<original, char, FONTENUMPROCA>;
} enum_font_families_A;

constexpr struct : EnumFontFamiliesBase {
	// other plugins may use this API, such as textassist.auf by oov.
	static inline auto* original = &::EnumFontFamiliesW;
	constexpr static auto& detour = detour_template<original, wchar_t, FONTENUMPROCW>;
} enum_font_families_W;

// CreateFontIndirectA/W.
// not sure this is necessary.
constexpr struct CreateFontIndirectW : DetourHelper {
	static inline auto* original = &::CreateFontIndirectW;
	static HFONT WINAPI detour(LOGFONTW const* lplf)
	{
		LOGFONTW lf;
		if (aliases(lplf->lfFaceName, lf.lfFaceName)) {
			// if it's aliased, copy the rest of LOGFONTW and call with the real name.
			std::memcpy(&lf, lplf, offsetof(LOGFONTW, lfFaceName));
			return aliases.add_fake_font(original(&lf), lplf->lfFaceName);
		}
		// original behavior otherwise.
		return original(lplf);
	}
} create_font_indirect_W;

struct CreateFontIndirectA : DetourHelper {
	static inline auto* original = &::CreateFontIndirectA;
	static HFONT WINAPI detour(LOGFONTA const* lplf)
	{
		// deduce to the wide version instead.
		LOGFONTW lf;
		encode_sys::to_wide_str(lf.lfFaceName, lplf->lfFaceName);
		std::memcpy(&lf, lplf, offsetof(LOGFONTA, lfFaceName));
		return create_font_indirect_W.detour(&lf);
	}
} create_font_indirect_A;

// CreateFontA/W.
constexpr struct : DetourHelper {
	static inline auto* original = &::CreateFontW;
	static HFONT WINAPI detour(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, DWORD bItalic,
		DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision,
		DWORD iQuality, DWORD iPitchAndFamily, wchar_t const* pszFaceName)
	{
		wchar_t buf[aliases.font_length_max];
		if (pszFaceName != nullptr && aliases(pszFaceName, buf)) {
			// if aliased, re-direct to the real name,
			// and store that alias for the future use.
			return aliases.add_fake_font(original(cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic, bUnderline, bStrikeOut,
				iCharSet, iOutPrecision, iClipPrecision, iQuality, iPitchAndFamily, buf),
				pszFaceName);
		}
		return original(cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic, bUnderline, bStrikeOut,
			iCharSet, iOutPrecision, iClipPrecision, iQuality, iPitchAndFamily, pszFaceName);
	}
} create_font_W;

constexpr struct : DetourHelper {
	static inline auto* original = &::CreateFontA;
	static HFONT WINAPI detour(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, DWORD bItalic,
		DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision,
		DWORD iQuality, DWORD iPitchAndFamily, char const* pszFaceName)
	{
		// deduce to the wide version instead.
		wchar_t buf[aliases.font_length_max];
		encode_sys::to_wide_str(buf, pszFaceName);
		return create_font_W.detour(cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic, bUnderline, bStrikeOut,
			iCharSet, iOutPrecision, iClipPrecision, iQuality, iPitchAndFamily, buf);
	}
} create_font_A;

// GetObjectA/W.
struct GetObjectBase : DetourHelper {
protected:
	template<auto& original, class LogFontT>
	static int WINAPI detour_template(HGDIOBJ hgdiobj, int cbBuffer, LogFontT* lpvObject)
	{
		if (hgdiobj != nullptr && cbBuffer == sizeof(LogFontT) && hgdiobj != nullptr) {
			if (auto alias = aliases.find_fake_font(hgdiobj); alias != nullptr) {
				// if it's a font with a fake name, return the fake LOGFONT.
				auto ret = original(hgdiobj, sizeof(LogFontT), lpvObject);
				copy_from_wide(lpvObject->lfFaceName, alias);
				return ret;
			}
		}
		return original(hgdiobj, cbBuffer, lpvObject);
	}
};

constexpr struct : GetObjectBase {
	static inline auto* original = &::GetObjectW;
	constexpr static auto& detour = detour_template<original, LOGFONTW>;
} get_object_W;;

constexpr struct : GetObjectBase {
	static inline auto* original = &::GetObjectA;
	constexpr static auto& detour = detour_template<original, LOGFONTA>;
} get_object_A;

// DeleteObject.
constexpr struct : DetourHelper {
	static inline auto* original = &::DeleteObject;
	static BOOL WINAPI detour(HGDIOBJ ho)
	{
		// remove from the list if exists.
		aliases.remove_fake_font(ho);
		return original(ho);
	}
} delete_object;


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
		excludes.load(path + filenames::ExcludesFile, false);

	// create the alias list.
	aliases.load(path + filenames::AliasFile);

	// override Win32 API.
	if (excludes.count() > 0 || aliases.count() > 0) {
		// needs to increment the reference count of this DLL.
		::LoadLibraryW(dllname.c_str());
		if (aliases.count() > 0) {
			DetourHelper::Attach(enum_font_families_A, enum_font_families_W,
				create_font_indirect_A, create_font_indirect_W,
				create_font_A, create_font_W,
				get_object_A, get_object_W, delete_object);
		}
		else DetourHelper::Attach(enum_font_families_A, enum_font_families_W);
	}
}
inline void on_detach()
{
	// restore the API.
	if (excludes.count() > 0 || aliases.count() > 0) {
		// will never be called probably.
		if (aliases.count() > 0) {
			DetourHelper::Detach(enum_font_families_A, enum_font_families_W,
				create_font_indirect_A, create_font_indirect_W,
				create_font_A, create_font_W,
				get_object_A, get_object_W, delete_object);
		}
		else DetourHelper::Detach(enum_font_families_A, enum_font_families_W);
	}
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
extern "C" __declspec(dllexport) char const* __stdcall ThisAulVersion(void)
{
	return "v1.30-beta3";
}
