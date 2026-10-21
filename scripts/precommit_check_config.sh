#!/bin/bash
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
# Pre-commit hook: prevent committing skills/gitcode-pr-review/config.json
# with non-empty gitcode_token or https_proxy.
#
# The config.json file is tracked on the repo with empty fields (template state).
# Users fill in their personal token/proxy locally. This hook blocks accidental commits.
#
# For cleaner git status, also run once after clone:
#   git update-index --skip-worktree skills/gitcode-pr-review/config.json
# -----------------------------------------------------------------------------------------------------------

set -euo pipefail

CONFIG_FILE="skills/gitcode-pr-review/config.json"

extract_fields() {
    git show "$1" 2>/dev/null | python3 -c "
import sys, json
try:
    data = json.load(sys.stdin)
    print(data.get('gitcode_token', '') + '|' + data.get('https_proxy', ''))
except Exception:
    print('|')
" 2>/dev/null || echo '|'
}

check_ref() {
    local ref="$1"
    local label="$2"
    local fields token proxy
    fields=$(extract_fields "$ref")
    token="${fields%%|*}"
    proxy="${fields#*|}"

    if [[ -n "$token" || -n "$proxy" ]]; then
        echo "[pre-commit] ERROR: $CONFIG_FILE $label has non-empty fields:" >&2
        [[ -n "$token" ]] && echo "             gitcode_token is set (must be empty)" >&2
        [[ -n "$proxy" ]]  && echo "             https_proxy is set (must be empty)" >&2
        echo "             Personal config must not be committed to the repository." >&2
        echo "             To fix:" >&2
        echo "               git reset HEAD $CONFIG_FILE" >&2
        echo "               git update-index --skip-worktree $CONFIG_FILE" >&2
        echo "             Then re-stage your other changes and retry." >&2
        exit 1
    fi
}

# Check 1: staged version (before commit) to catch what will be committed
check_ref ":${CONFIG_FILE}" "staged version"

# Check 2: latest committed version (after commit) to catch values that slipped through
check_ref "HEAD:${CONFIG_FILE}" "latest commit (HEAD)"

exit 0
