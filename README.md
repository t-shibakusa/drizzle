# Streaming Audio Studio

YouTube配信・弾き語り配信向けの
軽量リアルタイム音声処理 & 配信ツール。

## 概要

本プロジェクトは、

- OBS
- Reaper
- 仮想オーディオケーブル

など複数ツールを必要とする現在の配信環境を、
単一アプリへ統合することを目的とする。

特に以下の用途を想定：

- 弾き語り配信
- 雑談配信
- ASMR
- 歌配信
- 低遅延リアルタイム音声処理

---

# コンセプト

「OBSの代替」を目指すのではなく、

> “配信に必要な音声機能を、
> できる限り簡単に扱えること”

を重視する。

万能化ではなく、
配信特化・シンプル化を目的とする。

---

# 想定機能

## 音声機能（最重要）

- マイク入力
- ギター入力
- VST3対応
- EQ
- Compressor
- Noise Gate
- Reverb
- Limiter
- 簡易ミキサー
- モニタリング

---

## 配信機能

- YouTube RTMP配信
- 録画
- BGM再生
- シーン切替

---

## 映像機能

- カメラ入力
- 画面キャプチャ
- 画像表示
- テロップ表示

---

# システムイメージ

```text
Mic / Guitar / BGM
        ↓
    VST Effects
        ↓
      Mixer
        ↓
 Streaming / Recording
```

---

# 技術構成

## 言語

- C++

---

## メインフレームワーク

- JUCE

用途：

- オーディオ処理
- VSTホスト
- ASIO / WASAPI
- UI作成

---

## 将来的に利用検討

### 配信関連

- libobs
- FFmpeg

### OBS連携

- OBS WebSocket

---

# 開発ロードマップ

# Phase 1
## 音声エンジン基礎

### 目標

マイク入力 → VST → スピーカー出力

を成立させる。

### 内容

- JUCE導入
- AudioPluginHost解析
- ASIO対応
- VST3ロード
- 低遅延音声出力

---

# Phase 2
## マイクエフェクター化

### 機能追加

- EQ
- Compressor
- Noise Gate
- Reverb
- 音量メーター
- プリセット保存

### 目的

配信向けリアルタイム音声処理。

---

# Phase 3
## 簡易ミキサー化

### 機能追加

- ギター入力
- BGM入力
- Ducking
- マスター出力
- モニター出力

### イメージ

```text
Mic ─┐
Guitar ─┼→ Mixer → Master
BGM ─┘
```

---

# Phase 4
## OBS連携

### 内容

- 仮想オーディオ出力
- OBS音声入力
- OBS WebSocket制御

### この段階

OBS + 自作音声エンジン構成。

---

# Phase 5
## 配信機能統合

### 機能追加

- RTMP配信
- 録画
- カメラ入力
- 画面キャプチャ
- シーン切替

---

# 開発方針

## 最優先

- 低遅延
- 音切れ防止
- 安定性

---

## 後回しにするもの

- 高度な映像編集
- 多機能DAW化
- 複雑なタイムライン編集

---

# MVP（最初の完成目標）

## 最低限完成させるもの

- マイク入力
- VST3を挿せる
- 音を出せる
- EQ / Comp / Reverb
- 配信可能

---

# 開発環境

## 必要ツール

- Visual Studio 2022
- CMake
- JUCE
- ASIO SDK
- Git

---

# 初期タスク

## Step 1

JUCE導入。

---

## Step 2

AudioPluginHostをビルド。

---

## Step 3

以下を理解：

- AudioDeviceManager
- AudioProcessor
- AudioProcessorGraph
- VSTロード

---

## Step 4

自作VSTホスト作成。

---

# 最終目標

「OBS + Reaper」を置き換えるのではなく、

> “弾き語り・雑談配信を
> 最も快適に行えるツール”

を目指す。

---

# メモ

## 重要

最初から完成品を目指さない。

まずは：

- 音が出る
- VSTが動く
- 遅延しない

ここを目標とする。

リアルタイム音声処理は、
UIより先に安定性が重要。
