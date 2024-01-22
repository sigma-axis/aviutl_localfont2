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

- 以下の拡張子のファイルをフォントとして追加します:

  `.fon` `.fnt` `.ttf` `.ttc` `.fot` `.otf` `.mmm` `.pfb` `.pfm`

### フォントの除外方法

`Fonts` フォルダ内の `Excludes.txt` にフォント名を1行ずつ記述してください．

- 記述例:

  ```
  // Excludes.txt
  // ↓フォント名は1行に1つずつ記述してください．
  ＭＳ ゴシック
  ＭＳ 明朝
  FixedSys
  ```

記述法に関しては `Excludes.txt` 内のコメントにも説明があるので，そちらもご確認ください．

## その他

1. フォントの除外機能は，フォントをドロップダウンリストから「隠す」だけであって，制御文字 `<s,フォント名>` や `obj.setfont("フォント名",34)` などを利用して使うことは可能です．

1. oov様の[テキスト編集補助プラグイン](https://github.com/oov/aviutl_textassist)のフォントリストからも隠せます．

## 謝辞

このプラグインのフォントの一時追加機能は，アイデア，実装方法を含めて khsk様の [LocalFont プラグイン](https://github.com/khsk/AviUtl-LocalFontPlugin)のものを流用しています．この場ようなで恐縮ですが大変便利なプラグインの公開，感謝申し上げます．

## 改版履歴

- v1.01 (2024-01-21)

  - ビルドオプション修正．

- v1.00 (2024-01-21)

  - 初版．

# ライセンス

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

