#!/bin/bash
LOG="$HOME/wsl-doubleclick-test.log"
exec > >(tee -a "$LOG") 2>&1
echo
echo "=== WSL double-click diagnostic @ $(date) ==="
echo "Script path : $0"
echo "CWD         : $(pwd)"
echo "User        : $(whoami)"
echo "Shell       : $SHELL"
echo "PATH        : $PATH"
echo "Interactive : $([[ $- == *i* ]] && echo yes || echo no)"
echo "TTY         : $(tty 2>/dev/null || echo none)"
echo
echo "Arg test: $# args -> $@"
echo
echo "Exit code of 'ls' in CWD:"
ls >/dev/null 2>&1 && echo "  ok" || echo "  FAILED ($?)"
echo
read -n 1 -s -r -p "Press any key to close..."
echo
