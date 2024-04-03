# Local Font 2 AviUtl プラグイン

システムにインストールされていないフォントを一時的に AviUtl で使えるようにするプラグインです．`Font` フォルダに入っているフォントファイルのみを追加します．

逆にシステムにインストールされているけれどAviUtlでは使う予定のないフォントを，テキストオブジェクトのドロップダウンリストから隠すこともできます．

khsk様の [LocalFont プラグイン](https://github.com/khsk/AviUtl-LocalFontPlugin)の拡張版です．

[ダウンロードはこちら．](https://github.com/sigma-axis/aviutl_localfont2/releases)

## 動作要件

- AviUtl 1.10 + 拡張編集 0.92

  http://spring-fragrance.mints.ne.jp/aviutl

  - AviUtl 1.00 や 拡張編集 0.93rc1 等でも動作しますが推奨しません．

- Visual C++ 再頒布可能パッケージ（\[2015/2017/2019/2022\] の x86 対応版が必要）

  https://learn.microsoft.com/ja-jp/cpp/windows/latest-supported-vc-redist

## 導入方法

以下のフォルダのいずれかに `localfont2.aul` と `Fonts` フォルダをコピーしてください．

1. `aviutl.exe` のあるフォルダ
1. (1) のフォルダにある `plugins` フォルダ

## 使い方

### フォントの追加方法

`Fonts` フォルダの中に AviUtl に追加・使用したいフォントファイルを配置してください．サブフォルダ内のフォントも検索します（階層無制限）．

- 階層構造の例:

  ┣ :gear: `localfont2.aul`<br>
┣ :open_file_folder: `Fonts`<br>
┃&numsp;┣ :memo: `Excludes.txt`<br>
┃&numsp;┣ :page_facing_up: フォント1<br>
┃&numsp;┣ :page_facing_up: フォント2<br>
┃&numsp;┣ :open_file_folder: フォルダA<br>
┃&numsp;┃&numsp;┣ :page_facing_up: フォント3<br>
┃&numsp;┃&numsp;┗ :page_facing_up: フォント4<br>
┃&numsp;┗ :open_file_folder: フォルダB<br>
┃&numsp;&ensp;&numsp;┣ :page_facing_up: フォント5<br>
┃&numsp;&ensp;&numsp;┗ :open_file_folder: フォルダC<br>
┃&numsp;&ensp;&numsp;&ensp;&numsp;┗ :page_facing_up: フォント6


- 以下の拡張子のファイルをフォントとして追加します:

  `.fon` `.fnt` `.ttf` `.ttc` `.fot` `.otf` `.mmm` `.pfb` `.pfm`

### フォントの除外方法

`Fonts` フォルダ内の `Excludes.txt` に除外したいフォント名を1行ずつ記述してください．

- 記述例:

  ```
  // Excludes.txt
  // ↓フォント名は1行に1つずつ記述してください．
  ＭＳ ゴシック
  ＭＳ 明朝
  FixedSys
  ```

記述法に関しては `Excludes.txt` 内のコメントにも説明があるので，そちらもご確認ください．

#### ホワイトリストモード

`Excludes.txt` のファイル名を `Whitelist.txt` に変更すると，逆に指定したフォントのみが表示されるようになります．記述方法は `Excludes.txt` と同じです．

> [!NOTE]
> `Excludes.txt` と `Whitelist.txt` が同時に存在する場合，`Whitelist.txt` が優先されてホワイトリストモードになります．この場合 `Excludes.txt` は無視されます．

#### `全フォントリスト.exa` について

タイムラインにドラッグ&ドロップしてメインウィンドウに表示させるとスクリプトが動いて，コンソールに現在ドロップダウンリストから選択可能なフォント一覧を出力できるエイリアスファイルです．フォントリスト作成の補助に利用してください．

- 動作には [patch.aul](https://github.com/nazonoSAUNA/patch.aul) と [LuaJIT](https://luajit.org/) が必要です．LuaJITは[こちら](https://github.com/Per-Terra/LuaJIT-Auto-Builds/releases)からダウンロードすることでも取得できます．

- <details>
  <summary>実行する Lua スクリプトは次の通りです（クリックで表示）．</summary>

  ```lua
  local c,ffi,err=pcall(require,"ffi");
  if not _PATCH then err="patch.aul が必要です．";
  elseif not c then err="LuaJIT が必要です．";
  elseif not made_output then
      debug_print("フォント出力中...");
      ffi.cdef[[
          typedef struct {
              char pad[28];
              char lfFaceName[32];
          } LOGFONTA;
          typedef int(__stdcall *FONTENUMPROCA)(LOGFONTA*,void*,int,int);
          int EnumFontFamiliesA(void*,const char*,FONTENUMPROCA,int);
          void* GetDC(void*);
          int ReleaseDC(void*,void*);
      ]];
      c={};
      local cb,hdc=ffi.cast("FONTENUMPROCA",function(lf,_,_,_)
          local str=ffi.string(lf.lfFaceName);
          if str:sub(1,1)~="@" then table.insert(c,str.."\n") end
          return 1;
      end),ffi.C.GetDC(nil);
      ffi.C.EnumFontFamiliesA(hdc,nil,cb,0);
      cb:free();
      ffi.C.ReleaseDC(nil,hdc);
      table.sort(c); io.write(table.concat(c));
      debug_print(#c.."個のフォント名を出力．");
      made_output=true;
  end
  obj.load("text",err or "フォント名を出力しました．\nコンソールを確認してください．");
  ```
  </details>

## その他

1. フォントの除外機能は，フォントを設定ダイアログのドロップダウンリストから「隠す」だけであって，制御文字 `<s,フォント名>` やスクリプトの `obj.setfont("フォント名",34)` などを利用して使うことは可能です．

1. oov様の[テキスト編集補助プラグイン](https://github.com/oov/aviutl_textassist)のフォントリストからも隠せます．

> [!NOTE]
> 追加したフォントを除外リストに入れた場合，ドロップダウンリストには表示されませんが，制御文字 `<s>` や スクリプト `obj.setfont()` などでは使えるようになります．

## 謝辞

このプラグインのフォントの一時追加機能は，アイデア，実装方法を含めて khsk様の [LocalFont プラグイン](https://github.com/khsk/AviUtl-LocalFontPlugin)のものを流用しています．このような場で恐縮ですが大変便利なプラグインの公開，感謝申し上げます．

## 改版履歴

- **v1.14** (2024-04-04)

  - 同梱エイリアスファイルを更新．フォント名を名前順に並び替えるように変更．

- **v1.13** (2024-03-08)

  - 同梱エイリアスファイルを更新．各フォント名の出力で，行頭にタイムスタンプが出力されないように変更．

- **v1.12** (2024-02-28)

  - 初期化周りを少し改善．

- **v1.11** (2024-01-28)

  - ホワイトリストモードでフォントが1つもないとき，従来のブラックリストモードで動作するように変更．

  - 全フォントのリストが出力できるエイリアスファイルを同梱．

- **v1.10** (2024-01-28)

  - ホワイトリストモードの追加．指定したフォント以外が非表示になります．

- **v1.06** (2024-01-27)

  - 除外フォント名の一致判定が誤っていたことがあったのを修正．

- **v1.05** (2024-01-25)

  - 全角アルファベットの大文字と小文字を同一視できていなかったのを修正．

- **v1.04** (2024-01-25)

  - `README.md` や `Excludes.txt` 内のコメント説明を更新．

  - コード整理．

  - バイナリとしてはほぼ変更なしですがそれなりに形になったので，区切りとして付随ドキュメント更新も兼ねてバージョン上げ．

- **v1.03** (2024-01-23)

  - `Excludes.txt` でブロックコメント開始・終了の行が長すぎると認識されなかったのを修正．

  - 無駄なコピーを減らした．

  - ライブラリの使い方を間違っていたかもしれないのを修正．

  - その他小さな修正，コード整理．

- **v1.02** (2024-01-22)

  - 除外フォント指定で，フォント名前後の全角空白も無視するように変更．

  - 追加フォントの拡張子をチェックするように変更．

- **v1.01** (2024-01-21)

  - ビルドオプション修正．

- **v1.00** (2024-01-21)

  - 初版．

## ライセンス

このプログラムの利用・改変・再頒布等に関しては MIT ライセンスに従うものとします．

---

The MIT License (MIT)

Copyright (C) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

https://mit-license.org/


#  Credits

## AviUtl-LocalFontPlugin

https://github.com/khsk/AviUtl-LocalFontPlugin

---

MIT License

Copyright (c) 2020 khsk

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Microsoft Research Detours Package

https://github.com/microsoft/Detours

---

Copyright (c) Microsoft Corporation.

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


#  連絡・バグ報告

- GitHub: https://github.com/sigma-axis
- Twitter: https://twitter.com/sigma_axis
- nicovideo: https://www.nicovideo.jp/user/51492481
- Misskey.io: https://misskey.io/@sigma_axis

