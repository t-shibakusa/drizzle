# Drizzle 開発引継ぎ資料

## プロジェクト概要

Drizzle は、

- OBS
- Reaper
- 仮想オーディオケーブル

など複数ツールに分散している配信環境を統合することを目的とした、配信特化型リアルタイム音声処理ツール。

対象用途：

- 弾き語り配信
- 雑談配信
- 歌配信
- ASMR
- 低遅延リアルタイム音声処理

---

## 開発方針

フル DAW ではなく、

> 「配信向けリアルタイム音声ツール」

として開発する。OBS 完全互換は目指さず、配信で必要な機能へ特化する。

---

## 技術構成

| 項目 | 内容 |
|------|------|
| 言語 | C++17 |
| フレームワーク | JUCE |
| ビルド | CMake + Visual Studio 2022 |
| OS | Windows 11 |
| JUCE | `C:\dev\JUCE` |

### ビルド

```powershell
taskkill /F /IM Drizzle.exe 2>$null
cmake -S . -B build
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" build\Drizzle.sln /p:Configuration=Release /m
```

出力：

```text
build/Drizzle_artefacts/Release/Drizzle.exe
build/Drizzle_artefacts/Debug/Drizzle.exe
```

`Drizzle.exe` がロックされる場合はプロセス終了後に再ビルド。必要なら exe を `.bak` にリネームしてからリンク。

### CMake 主要定義

```cmake
JUCE_ASIO=1
JUCE_PLUGINHOST_VST3=1
JUCE_PLUGINHOST_VST=$<BOOL:${DRIZZLE_HAS_VST2}>  # DRIZZLE_VST2_SDK_PATH 指定時のみ
ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/icon.png"
```

VST2 を有効にする場合：

```cmake
- DDRIZZLE_VST2_SDK_PATH="C:/path/to/VST2_SDK"
```

---

## ディレクトリ構成

```text
drizzle/
├─ icon.png
├─ CMakeLists.txt
├─ toCursor.md
├─ build/
└─ Source/
   ├─ Main.cpp / MainComponent.h/cpp
   ├─ AudioEngine.h/cpp
   ├─ PluginChain.h/cpp
   ├─ PluginScanPaths.h/cpp
   ├─ TrackMixerProcessor.h/cpp
   ├─ SessionSettings.h/cpp
   ├─ ApplicationShutdown.h
   └─ ui/
      ├─ DrizzleTheme.h
      ├─ DrizzlePanels.h/cpp
      ├─ SettingsPanels.h/cpp
      └─ MixerFaderLookAndFeel.h
```

---

## オーディオ信号経路

```text
Audio I/F 入力（最大32ch）
  → TrackMixerProcessor（トラックミキサー）
  → [VST3 プラグイン]（任意）
  → Gain
  → 出力
```

- `AudioProcessorPlayer` + `AudioDeviceManager` でリアルタイム処理
- トラックごとに入力チャンネル・ゲイン・パン・Mute・Solo を適用
- Solo が有効なトラックがある場合、そのトラックのみミックスに乗る

---

## UI 構成（メイン画面）

モックアップ準拠の OBS+DAW 風レイアウト（`MainComponent`）。

| 領域 | クラス | 内容 |
|------|--------|------|
| 左 | `TrackMixerPanel` | トラック一覧・マスター出力 |
| 中央上 | `StreamPreviewPanel` | 配信プレビュー（モック） |
| 中央下 | `StreamSettingsPanel` | 配信設定（モック） |
| 右 | `CommentPanel` | コメント（モック） |
| 下 | `StatusBarComponent` | ステータスバー |
| 上 | `SystemMetricsBar` | CPU 等（モック） |

メニュー「設定」からオーディオ設定・VST プラグイン設定ダイアログを開く。

---

## トラックミキサー（TrackMixerPanel）

### トラック数

| 項目 | 値 |
|------|-----|
| 最小 | 1 |
| 最大 | 10 |
| 初期 | 1 |
| デフォルト名 | `トラック1`, `トラック2`, … |

### UI 操作

- **スクロール**: トラック行の高さは 110px 固定。`Viewport` で縦スクロール（右端にスクロールバー）
- **追加**: 10 未満のときマスター直上に「+ トラックを追加」ボタン
- **削除**: 各トラック右上の × ボタン → 確認ダイアログ後に削除（1 本のときは削除ボタン非表示）
- **名前**: ラベルクリックで編集（JUCE `Label` 編集モード）、Enter / 他クリックで確定
- **入力**: ボタンから入力チャンネル選択ダイアログ
- **S / M**: Solo / Mute（トグル、ON 時ハイライト）
- **フェーダー**: `MixerFaderLookAndFeel` による溝＋矩形キャップ風 UI、ゲイン 0〜1（スキュー付き）

### マスター

- フェーダー・Mute・Mono ボタン（マスター音量のオーディオ反映は未実装の可能性あり。設定値は保存される）

---

## 設定の永続化

| ファイル | パス | 内容 |
|----------|------|------|
| オーディオデバイス | `%APPDATA%\Drizzle\audio_settings.xml` | ASIO/WASAPI、入出力、バッファ等 |
| セッション | `%APPDATA%\Drizzle\session_settings.xml` | トラック数・各トラック設定、マスター、pluginGain、ウィンドウ位置・サイズ |
| プラグインスキャンパス | `%APPDATA%\Drizzle\plugin_paths.xml` | VST / VST2 / VST3 / AAX タブ別ディレクトリ |
| Dead man's pedal | `%APPDATA%\Drizzle\dead_mans_pedal.txt` | スキャン失敗プラグイン記録 |

セッション XML の `TRACKS` ノードに `count` 属性でトラック数を保存。

変更時に `AudioEngine::saveSessionSettings()`、終了時にも保存。

---

## VST プラグインホスト

### 機能

- VST3 スキャン（Scan ボタンのみ。起動時の全件スキャンは廃止）
- リスト選択 / Browse でロード
- プラグイン Editor 表示
- Clear でアンロード
- `releaseAll()`: 設定ダイアログ閉じ・終了時に DLL 解放（XLN 更新時のファイルロック対策）

### スキャンディレクトリ

`PluginScanPaths` でフォーマット別（VST / VST2 / VST3 / AAX）に複数パス登録。AAX は JUCE 標準ではホスト非対応（パス登録のみ）。

### Waves 利用時の注意

- `WaveShell*.vst3`（`Common Files\VST3`）経由で個別プラグインをロード
- `Plug-Ins V16` 内を直接 Browse しない
- リストの `WaveShell*-VST3` 本体は選択不可
- ライセンスエラー時は Waves Central のログイン・同期・修復を確認

---

## アプリ終了処理

`ApplicationShutdown.h` + `MainWindow::prepareForShutdown()` で以下を順に実行：

1. ウィンドウ位置・サイズ保存
2. トラック名編集の確定
3. モーダルダイアログ全解除
4. プラグインエディタ等の別ウィンドウを閉じる
5. オーディオコールバック停止 → VST 解放 → デバイスクローズ
6. `MainComponent` 破棄

`AudioEngine::shutdown()` は二重呼び出し防止フラグ付き。

---

## アプリアイコン

プロジェクトルートの `icon.png` を `CMakeLists.txt` の `ICON_BIG` / `ICON_SMALL` で指定。ビルド時に `icon.ico` が生成され exe に埋め込まれる。

---

## 確認済み

- JUCE ビルド・GUI 起動
- ASIO / WASAPI デバイス選択・永続化
- VST3 ロード・処理・Editor 表示
- トラックミキサー（入出力ルーティング、ゲイン、パン、Solo、Mute）
- トラック追加・削除（1〜10）
- スクロール可能なトラック欄
- セッション・ウィンドウ位置の保存・復元
- トラック名編集（Label 編集モード）
- 終了時のリソース解放
- アプリアイコン（exe）

---

## 未実装・既知の課題

- マスターフェーダー / Mute / Mono のオーディオ反映（設定保存のみの可能性）
- 配信・録画・OBS 連携（UI はモック）
- 仮想オーディオ出力
- AAX ホスト（Avid SDK が必要）
- 複数 VST のチェイン

---

## 次にやるべきこと（候補）

1. **マスター音量**を `GainAudioProcessor` またはミキサー後段に接続
2. **配信パネル**の実機能化（またはモック整理）
3. **仮想オーディオ出力**（配信向け）
4. OBS WebSocket 連携の検討

---

## 注意事項

### Docker は使用しない

ASIO / WASAPI / VST / リアルタイム音声処理と相性が悪いため、Windows ホストで開発する。

### include 形式

```cpp
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
```

`#include <JuceHeader.h>` は CMake 構成では使わない。

### AudioDeviceSelector の注意

`AudioDeviceSelectorComponent` はコンテンツ高さに応じてリサイズする。親で `childBoundsChanged` → `resized()` のループを作ると起動時クラッシュしうる。**Viewport でラップ**し、`deviceManager.initialise()` の後に生成すること。

---

## 開発コンセプト

目指すのは：

> 「OBS を超えるツール」

ではなく、

> 「弾き語り・雑談配信を最も快適にするツール」

である。
