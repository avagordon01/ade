set -euo pipefail

nmcli device status | awk '/ethernet/ { if($3 == "connected") print "eth" } /wifi/ { if($3 == "connected") print "wifi", $4 }'
