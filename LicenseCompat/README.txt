Drizzle ライセンス互換ランチャー
================================

XLN Audio / iZotope などのプラグインは、ホストアプリの「exe ファイル名」で
ライセンス ID (ComputerId) を判定します。

Reaper で認証済みのプラグインを Drizzle でも使うには reaper.exe として起動します。

  通常の起動:
  - Drizzle.exe を起動すると自動で reaper.exe に切り替わります

  まだ認証に失敗する場合（推奨）:
  1. install_to_reaper_folder.bat を管理者として実行
     → C:\Program Files\REAPER (x64)\DrizzleLicenseHost\reaper.exe にコピー
  2. Drizzle.exe を再起動
  3. VST 設定で再スキャン → プラグインを付け直す
  4. XLN Online Installer を起動し、ログインして製品を修復

  診断ログ:
  %AppData%\Drizzle\license_compat.log

  注意:
  - インストール済みの Reaper 本体 (reaper.exe) とは別ファイルです
  - PATH にこのフォルダを追加しないでください
  - ビルドのたびに reaper.exe は自動更新されます
