set -euo pipefail

if $(pamixer --get-mute); then
    str="muted "
else
    str="volume"
fi
echo "${str} $(pamixer --get-volume)%"
