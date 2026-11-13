# Sourced by build.sh and run_phone.sh: where the Android SDK is.
#   ANDROID_HOME or ANDROID_SDK_ROOT if set; under WSL, Windows' %LOCALAPPDATA%\Android\Sdk
#   (Android Studio's default); otherwise ~/Android/Sdk.
android_sdk() {
  local sdk=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}
  if [ -z "$sdk" ] && command -v cmd.exe >/dev/null 2>&1 && command -v wslpath >/dev/null 2>&1; then
    local lad
    lad=$(cmd.exe /c 'echo %LOCALAPPDATA%' 2>/dev/null | tr -d '\r')
    [ -n "$lad" ] && [ -d "$(wslpath "$lad")/Android/Sdk" ] && sdk="$(wslpath "$lad")/Android/Sdk"
  fi
  [ -n "$sdk" ] || sdk="$HOME/Android/Sdk"
  echo "$sdk"
}
