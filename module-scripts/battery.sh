BATC=/sys/class/power_supply/BAT0/capacity
BATS=/sys/class/power_supply/BAT0/status

case $(cat $BATS) in
    Full)
        ;;
    Charging)
        ;;
    *)
        ;;
esac

echo "battery $(cat $BATC)%"
