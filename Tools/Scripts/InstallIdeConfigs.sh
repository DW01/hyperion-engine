#!/bin/bash

# Copies the editor launch/debug templates from Tools/IDE into .vscode and .zed. Existing files are left alone;
# delete one to get the template back.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
IDE_DIR="$SCRIPT_DIR/../IDE"

case "$(uname -s)" in
    Darwin) PLATFORM="Mac" ;;
    Linux) PLATFORM="Linux" ;;
    *) exit 0 ;;
esac

install_config() {
    if [[ -f "$REPO_DIR/$2" ]]; then
        return
    fi

    mkdir -p "$(dirname "$REPO_DIR/$2")"
    cp "$1" "$REPO_DIR/$2" && echo "Installed $2"
}

install_config "$IDE_DIR/VSCode/launch.$PLATFORM.json" ".vscode/launch.json"
install_config "$IDE_DIR/VSCode/tasks.json" ".vscode/tasks.json"
install_config "$IDE_DIR/Zed/debug.$PLATFORM.json" ".zed/debug.json"
