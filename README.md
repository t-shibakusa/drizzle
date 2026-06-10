# Drizzle

配信向けリアルタイム音声処理ツール（Windows）。

OBS・Reaper・仮想オーディオケーブルなどに分散しがちな配信まわりの音声まわりを、ひとつのアプリにまとめることを目指しています。弾き語り・雑談・歌配信・ASMR など、低遅延のライブ音声処理を想定しています。

> **コンセプト:** OBS の完全代替ではなく、「配信に必要な音声をできるだけ簡単に扱う」ことに特化します。

---

## 現状できること（v0.1）

### オーディオ

- **ASIO / WASAPI** などによる入出力デバイス選択（設定は次回起動時に復元）
- **マルチトラックミキサー**（1〜10 トラック）
  - トラック名の編集（例: `トラック1`）
  - 入力チャンネルの選択（オーディオ IF の IN ごと）
  - フェーダー（音量）・パン
  - **Solo** / **Mute**
  - トラックの追加・削除（削除時は確認ダイアログ）
  - トラック欄は高さ固定＋縦スクロール
- **VST3 ホスト**（1 プラグインをチェインに挿入）
  - スキャンディレクトリの登録（VST / VST2 / VST3 / AAX パス。AAX のホストは未対応）
  - リスト選択・ファイル参照でロード、エディタ表示、Clear でアンロード
- 信号経路: `入力 → トラックミキサー → [VST3] → Gain → 出力`

### UI

- OBS + DAW 風のメインレイアウト（配信プレビュー・コメント等の一部パネルは **UI モック**）
- メニュー **設定** からオーディオ設定・VST プラグイン設定を開く
- ウィンドウ位置・サイズの保存

### その他

- アプリアイコン（`icon.png`）
- 終了時のオーディオ／プラグイン DLL の解放

---

## まだ未実装・制限

- マスター・フェーダー / Mute / Mono の **オーディオへの反映**（UI と設定保存のみの状態）
- RTMP 配信・録画・OBS 連携（画面はモック）
- 仮想オーディオ出力
- 複数 VST のチェイン
- AAX プラグインのホスト（パス登録のみ）

---

## 動作環境

| 項目 | 内容 |
|------|------|
| OS | Windows 10 / 11（開発・検証は Windows 11） |
| ビルド | Visual Studio 2022、CMake 3.22+ |
| フレームワーク | [JUCE](https://juce.com/)（リポジトリ外に配置） |
| オーディオ | ASIO 対応 IF 推奨（WASAPI も可） |

---

## ビルド方法

### 前提

- Visual Studio 2022（「C++ によるデスクトップ開発」）
- CMake（PATH に追加）
- JUCE をクローンまたは展開（例: `C:\dev\JUCE`）
- `CMakeLists.txt` の `add_subdirectory("C:/dev/JUCE" JUCE)` を自分の JUCE パスに合わせて変更

初回セットアップの詳細は [INSTALL.md](INSTALL.md) を参照してください。

### 手順（PowerShell、リポジトリルート）

```powershell
# 実行中の Drizzle があれば終了
taskkill /F /IM Drizzle.exe 2>$null

# 構成（初回・CMakeLists 変更時）
cmake -S . -B build

# ビルド（Release）
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" `
  build\Drizzle.sln /p:Configuration=Release /m
```

実行ファイル:

```text
build\Drizzle_artefacts\Release\Drizzle.exe
```

`Drizzle.exe` がロックされてリンクに失敗する場合は、タスクマネージャーでプロセスを終了するか、既存の exe をリネームしてから再ビルドしてください。

### オプション: VST2 ホスト

[Steinberg VST2 SDK](https://www.steinberg.net/vst2sdk) を取得し、CMake 構成時にパスを指定します。

```powershell
cmake -S . -B build -DDRIZZLE_VST2_SDK_PATH="C:/path/to/VST2_SDK"
```

---

## 使い方（概要）

1. `Drizzle.exe` を起動する。
2. **設定 → オーディオ設定** でデバイス（ASIO 推奨）・バッファサイズを選ぶ。
3. 左の **トラック** 欄で入力・フェーダー・Solo / Mute を調整する。必要なら **+ トラックを追加**。
4. **設定 → VST プラグイン設定** でスキャンパスを登録し **Scan** → プラグインを **Load**（Waves は WaveShell 経由。詳細は下記）。
5. 終了時に設定は `%APPDATA%\Drizzle\` に自動保存される。

### 設定ファイルの保存先

| ファイル | 内容 |
|----------|------|
| `%APPDATA%\Drizzle\audio_settings.xml` | オーディオデバイス設定 |
| `%APPDATA%\Drizzle\session_settings.xml` | トラック・マスター・ウィンドウ位置など |
| `%APPDATA%\Drizzle\plugin_paths.xml` | プラグインスキャンディレクトリ |

### Waves プラグイン利用時

- `C:\Program Files\Common Files\VST3` の **WaveShell\*.vst3** 経由で個別プラグインを選ぶ。
- `Plug-Ins V16` フォルダ内の `.vst3` を直接 Browse しない。
- リストの **WaveShell 本体** はロード対象外。個別プラグイン名を選ぶ。
- ライセンスエラー時は Waves Central でログイン・同期・修復を試す。

---

## プロジェクト構成

```text
drizzle/
├── icon.png              # アプリアイコン
├── CMakeLists.txt
├── README.md
├── INSTALL.md            # 開発環境のセットアップ
├── toCursor.md           # 開発引継ぎ・実装メモ
├── Source/
│   ├── Main.cpp / MainComponent.*
│   ├── AudioEngine.*
│   ├── PluginChain.* / PluginScanPaths.*
│   ├── TrackMixerProcessor.*
│   ├── SessionSettings.*
│   ├── ApplicationShutdown.h
│   └── ui/
│       ├── DrizzlePanels.*
│       ├── SettingsPanels.*
│       ├── DrizzleTheme.h
│       └── MixerFaderLookAndFeel.h
└── build/                # 生成物（git 管理外）
```

---

## ロードマップ（概要）

| 段階 | 内容 |
|------|------|
| **現在** | マルチトラックミキサー + VST3 ホスト + ASIO、配信用 UI の骨格 |
| 今後 | マスター音量のオーディオ反映、仮想オーディオ出力 |
| 将来 | OBS 連携、RTMP 配信・録画、映像まわり |

詳細な開発状況・既知問題は [toCursor.md](toCursor.md) を参照してください。

---

## ライセンス

（リポジトリに LICENSE が無い場合は、プロジェクトオーナーが追記してください。）

JUCE の利用には [JUCE ライセンス](https://juce.com/juce-legal/) に従ってください。VST2 SDK を有効にする場合は Steinberg のライセンス条件も適用されます。

---

## 関連ドキュメント

- [INSTALL.md](INSTALL.md) — 開発環境構築（Visual Studio / CMake / JUCE / ASIO）
- [toCursor.md](toCursor.md) — 実装詳細・引継ぎ用
