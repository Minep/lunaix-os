#!/bin/bash

new_pane() {
  local title=""
  local size=""
  local direction="vertical"
  local target=""

  while [[ $# -gt 0 ]]; do
    case "$1" in
      -t|--title)
        title="$2"
        shift 2
        ;;
      -s|--size)
        size="$2"
        shift 2
        ;;
      -d|--direction)
        direction="$2"
        shift 2
        ;;
      -p|--target-pane|--target)
        target="$2"
        shift 2
        ;;
      --)
        shift 1
        break
        ;;
      *)
        echo "tmux_new_pane_tty: unknown argument: $1" >&2
        return 2
        ;;
    esac
  done

  local split_flag
  case "$direction" in
    h|horizontal|right)
      split_flag="-h"   # side-by-side split
      ;;
    v|vertical|down)
      split_flag="-v"   # top/bottom split
      ;;
    *)
      echo "tmux_new_pane_tty: direction must be horizontal or vertical" >&2
      return 2
      ;;
  esac

  local args=(
    split-window
    -d
    -P
    "$split_flag" 
    -F '#{pane_id} #{pane_tty}'
  )

  if [[ -n "$size" ]]; then
      args+=(-l "$size")
  fi

  if [[ -n "$target" ]]; then
    args+=(-t "$target")
  fi

  if [[ -z "${*}" ]]; then
      args+=(-I)
  fi

  args+=("${*}")

  local out pane_id pane_tty

  if ! out="$(tmux "${args[@]}")"; then
    echo "tmux_new_pane_tty: failed to create tmux pane" >&2
    return 1
  fi
    
  read -r pane_id pane_tty <<< "$out"

  if [[ -n "$title" ]]; then
    tmux select-pane -t "$pane_id" -T "$title" >/dev/null
  fi

  echo "$pane_tty" "$pane_id"
}

tty_by() {
    case "$1" in
        -i|--id)
            tmux list-panes -F "#{pane_tty}" -f "#{==:#{pane_index},${2}}"
        ;;
        -t|--name)
            tmux list-panes -F "#{pane_tty}" -f "#{==:#{pane_title},${2}}"
        ;;
        *)
            echo ""
        ;;
    esac
}

gdb_remote="localhost:0"
serial_telnet="0"
hmp="0"
kernel_bin="$1"

shift 1

while [[ $# -gt 0 ]]; do
    case "$1" in
        -g|--gdb)
            gdb_remote="$2"
            shift 2
            ;;
        -s|--serial)
            serial_telnet="$2"
            shift 2
            ;;
        -q|--hmp)
            hmp="$2"
            shift 2
            ;;
        -p|--target-pane|--target)
            target="$2"
            shift 2
            ;;
        *)
            echo "tmux_new_pane_tty: unknown argument: $1" >&2
            return 2
            ;;
    esac
done

#tmux set-option remain-on-exit on
tmux set pane-border-status top

current_tty=$(tty_by -i 0)

right=($(new_pane -t "ttyS0" -s "80" -d h -- telnet localhost "${serial_telnet}"))
mid=($(new_pane -t "asm" -s "50%" -d h -p ":.0" -- sh))
left_bot=$(new_pane -t "qmp" -s "20" -d v -p ":.0" -- telnet localhost "${hmp}")
var=($(new_pane -t "variables" -s "10" -d v -p ":.0" -- sh))

tmux swap-pane -s 0 -t ${mid[1]}
tmux select-pane -t ${mid[1]} -T "gdb"
tmux select-pane -t ${mid[1]}

asm_tty=$(tty_by -i 0)

gdb-multiarch \
    "${kernel_bin}" \
    -ex "dashboard -layout assembly stack source variables" \
    -ex "dashboard assembly -output ${asm_tty}" \
    -ex "dashboard variables -output ${var[0]}" \
    -ex "target remote localhost:${gdb_remote}" 


tmux kill-window
