set -euo pipefail

brightness=$(xrandr --verbose | awk '/Brightness/ { print $2 * 100 }' | head -1)
echo "brightness $brightness%"
