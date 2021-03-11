set -euo pipefail

nmcli --fields TYPE,CONNECTION device | awk 'FNR==2 { print $1, $2 }'
