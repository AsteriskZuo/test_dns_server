#!/bin/bash
# This script tests various network metrics for a list of IP addresses.
# Usage: ./test_ip.sh [test_type] [ip1 ip2 ip3 ...]
# test_type options: ping, trace, geo, speed, all (default)

# Default IP addresses if none provided
DEFAULT_IPS=(
    "23.248.182.81"
    "98.96.227.77"
    "193.122.187.241"
    "132.226.60.241"
    "8.8.8.8"         # Google DNS
    "1.1.1.1"         # Cloudflare DNS
)

# Function to display usage information
usage() {
    echo "Usage: $0 [test_type] [ip1 ip2 ip3 ...]"
    echo "test_type options: ping, trace, geo, speed, all (default)"
    echo "If no IPs are provided, default IPs will be used."
    exit 1
}

# Function to test ping latency
test_ping() {
    local ips=("$@")
    echo "===== PING LATENCY TEST ====="
    for ip in "${ips[@]}"; do
        echo -n "$ip: "
        if ping -c 3 -t 5 "$ip" &>/dev/null; then
            ping -c 3 "$ip" | grep avg | awk -F'/' '{print $5 " ms"}'
        else
            echo "Failed to ping"
        fi
    done
    echo
}

# Function to perform traceroute
test_trace() {
    local ips=("$@")
    echo "===== TRACEROUTE TEST ====="
    for ip in "${ips[@]}"; do
        echo "Traceroute to $ip:"
        traceroute -m 15 "$ip" || echo "Traceroute failed for $ip"
        echo
    done
}

# Function to get geographic location
test_geo() {
    local ips=("$@")
    echo "===== GEOGRAPHIC LOCATION TEST ====="
    for ip in "${ips[@]}"; do
        echo -n "$ip: "
        curl -s --max-time 5 "https://ipinfo.io/$ip" | grep -E 'country|city|region' || echo "Failed to get location data"
    done
    echo
}

# Function to test download speed
test_speed() {
    local ips=("$@")
    echo "===== DOWNLOAD SPEED TEST ====="
    for ip in "${ips[@]}"; do
        echo "Testing $ip..."
        if curl -o /dev/null --connect-timeout 5 -s "http://$ip" &>/dev/null; then
            curl -o /dev/null -s -w "Speed: %{speed_download} bytes/sec\n" "http://$ip" || echo "Speed test failed"
        else
            echo "Failed to connect to $ip"
        fi
    done
    echo
}

# Check for required commands
if ! command -v ping &>/dev/null; then
    echo "Error: ping command not found. Please install it and try again."
    exit 1
fi

if ! command -v traceroute &>/dev/null; then
    echo "Warning: traceroute command not found. Traceroute tests will be skipped."
    HAS_TRACEROUTE=0
else
    HAS_TRACEROUTE=1
fi

if ! command -v curl &>/dev/null; then
    echo "Warning: curl command not found. Geo and speed tests will be skipped."
    HAS_CURL=0
else
    HAS_CURL=1
fi

# Parse arguments
TEST_TYPE="all"
IPS=()

# First argument might be the test type
if [[ $1 == "ping" || $1 == "trace" || $1 == "geo" || $1 == "speed" || $1 == "all" ]]; then
    TEST_TYPE="$1"
    shift
fi

# Remaining arguments are IPs
if [ $# -gt 0 ]; then
    IPS=("$@")
else
    IPS=("${DEFAULT_IPS[@]}")
fi

# Display test configuration
echo "Running $TEST_TYPE tests on ${#IPS[@]} IP addresses:"
printf "  %s\n" "${IPS[@]}"
echo

# Run the selected test(s)
case "$TEST_TYPE" in
    ping)
        test_ping "${IPS[@]}"
        ;;
    trace)
        if [ $HAS_TRACEROUTE -eq 1 ]; then
            test_trace "${IPS[@]}"
        else
            echo "Traceroute command not available."
        fi
        ;;
    geo)
        if [ $HAS_CURL -eq 1 ]; then
            test_geo "${IPS[@]}"
        else
            echo "Curl command not available."
        fi
        ;;
    speed)
        if [ $HAS_CURL -eq 1 ]; then
            test_speed "${IPS[@]}"
        else
            echo "Curl command not available."
        fi
        ;;
    all)
        test_ping "${IPS[@]}"
        if [ $HAS_TRACEROUTE -eq 1 ]; then
            test_trace "${IPS[@]}"
        fi
        if [ $HAS_CURL -eq 1 ]; then
            test_geo "${IPS[@]}"
            test_speed "${IPS[@]}"
        fi
        ;;
    *)
        usage
        ;;
esac

# Print summary
echo "===== TEST SUMMARY ====="
echo "Test completed for ${#IPS[@]} IP addresses."
echo "Test type: $TEST_TYPE"
echo "Date: $(date)"