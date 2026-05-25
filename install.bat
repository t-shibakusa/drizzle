@echo off
chcp 65001 > nul
setlocal

echo =====================================
echo Streaming Audio Studio 開発環境セットアップ
echo =====================================

:: 管理者権限チェック
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo 管理者権限で実行してください。
    pause
    exit /b 1
)

:: winget確認
where winget >nul 2>&1
if %errorLevel% neq 0 (
    echo winget が見つかりません。
    echo Microsoft Store から「アプリ インストーラー」を更新してください。
    pause
    exit /b 1
)

echo.
echo [2/5] CMake をインストール中...
winget install --id Kitware.CMake -e --source winget

echo.
echo [3/5] Visual Studio 2022 Community をインストール中...
winget install --id Microsoft.VisualStudio.2022.Community -e --source winget ^
 --override "--add Microsoft.VisualStudio.Workload.NativeDesktop --add Microsoft.VisualStudio.Component.VC.CMake.Project --add Microsoft.VisualStudio.Component.Windows11SDK.22621 --includeRecommended --passive --norestart"

echo.
echo [4/5] ディレクトリ作成...
mkdir C:\dev 2>nul
mkdir C:\SDKs 2>nul

echo.
echo [5/5] JUCE を取得中...
cd /d C:\dev
if not exist C:\dev\JUCE (
    git clone https://github.com/juce-framework/JUCE.git
) else (
    echo JUCE は既に存在します。
)

echo.
echo =====================================
echo セットアップ完了
echo =====================================
echo.
echo 次にやること:
echo 1. PCを再起動
echo 2. C:\dev\JUCE\examples\Plugins\AudioPluginHost をビルド
echo 3. ASIO SDK / VST3 SDK は手動で確認
echo.
pause