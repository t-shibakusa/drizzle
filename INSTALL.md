# 開発環境構築手順（Windows 11）

Streaming Audio Studio 開発用セットアップ。

対象：

- Windows 11
- C++
- JUCE
- VST3
- ASIO
- Visual Studio 2022

---

# 1. Visual Studio 2022 インストール

## ダウンロード

:contentReference[oaicite:0]{index=0}

---

## インストール時に必要なワークロード

以下にチェック：

- デスクトップ開発（C++）

---

## 推奨追加コンポーネント

- MSVC 最新版
- Windows 11 SDK
- C++ CMake tools for Windows

---

# 2. Git インストール

## ダウンロード

:contentReference[oaicite:1]{index=1}

---

## 確認

PowerShell：

```powershell
git --version
```

---

# 3. CMake インストール

## ダウンロード

:contentReference[oaicite:2]{index=2}

---

## インストール時

以下にチェック：

```text
Add CMake to system PATH
```

---

## 確認

```powershell
cmake --version
```

---

# 4. JUCE ダウンロード

## 公式

:contentReference[oaicite:3]{index=3}

---

## 推奨配置

```text
C:\dev\JUCE
```

---

## Git Clone

```powershell
cd C:\dev

git clone https://github.com/juce-framework/JUCE.git
```

---

# 5. ASIO SDK ダウンロード

## Steinberg

:contentReference[oaicite:4]{index=4}

---

## 推奨配置

```text
C:\SDKs\ASIO_SDK
```

---

# 6. VST3 SDK ダウンロード

## GitHub

:contentReference[oaicite:5]{index=5}

---

## Clone

```powershell
cd C:\SDKs

git clone https://github.com/steinbergmedia/vst3sdk.git
```

---

# 7. JUCE AudioPluginHost をビルド

最重要。

まずは：

- 音が出る
- VSTが読める

ここを確認。

---

## AudioPluginHost場所

```text
JUCE/examples/Plugins/AudioPluginHost
```

---

## CMake生成

```powershell
cd C:\dev\JUCE\examples\Plugins\AudioPluginHost

cmake -B build
```

---

## Visual Studio Solution生成

```powershell
cmake --build build --config Release
```

---

# 8. AudioPluginHost 起動

生成された exe を起動。

---

## 確認事項

### Audio Device

- WASAPI
または
- ASIO

を選択。

---

## VSTスキャン

### Settings

から：

```text
Plugin Directories
```

へVSTフォルダ追加。

例：

```text
C:\Program Files\Common Files\VST3
```

---

# 9. テスト

## 推奨テストVST

### 無料

- ReaPlugs
- TDR Nova
- Valhalla Supermassive

---

# 10. 動作確認

## 目標

以下を成立させる：

```text
マイク
 ↓
VST
 ↓
スピーカー
```

---

# 推奨ディレクトリ構成

```text
C:\dev
 ├─ JUCE
 ├─ streaming-audio-studio
 └─ tools

C:\SDKs
 ├─ ASIO_SDK
 └─ vst3sdk
```

---

# 開発初期の目標

まずは：

- 音が出る
- VSTが動く
- 遅延が少ない

ここだけを目標にする。

UIは後回し。

---

# 注意点

## Dockerは使わない

リアルタイム音声処理と
ASIO/VSTはDockerと相性が悪い。

開発はホストマシン推奨。

---

# 次のステップ

環境構築後：

1. AudioPluginHost を触る
2. AudioProcessorGraph を読む
3. 自作VSTホストを作る
4. マイクエフェクター化
5. 配信機能追加
