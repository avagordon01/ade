set -euo pipefail

brightness=$(xrandr --verbose | awk '/Brightness/ { print $2 * 100 }' | head -1)

case ${1-none} in
    up)
        xrandr --output eDP-1 --brightness $(awk "BEGIN { print ($brightness + 1) / 100 }")
        ;;
    down)
        xrandr --output eDP-1 --brightness $(awk "BEGIN { print ($brightness - 1) / 100 }")
        ;;
    none)
        echo "brightness $brightness%"
        ;;
esac
