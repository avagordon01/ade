set -euo pipefail

case ${1-default} in
    default)
        if $(pamixer --get-mute); then
            str="muted "
        else
            str="volume"
        fi
        echo "${str} $(pamixer --get-volume)%"
        ;;
    up)
        pamixer --increase 1
        ;;
    down)
        pamixer --decrease 1
        ;;
    mute)
        pamixer --toggle-mute
        ;;
esac
