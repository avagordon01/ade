set -euo pipefail

#TODO this is really slow when there's an external monitor plugged in
#brightness=$(xrandr --verbose | awk '/Brightness/ { print $2 * 100 }' | head -1)
brightness=25

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
