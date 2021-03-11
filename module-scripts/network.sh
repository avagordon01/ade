set -euo pipefail

nmcli device status | awk 'BEGIN { ORS=" " } /ethernet/ { if($3 == "connected") print "eth" } /wifi/ { if($3 == "connected") print $2, $4 } END { print "\n" }'
