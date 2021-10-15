#!/bin/bash

# Ansi escape codes for colors
RED='\033[0;31m'
GREEN='\033[0;32m'
NO_COLOR='\033[0m'

# Check if apt is installed on machine
if command -v apt &> /dev/null
then
    echo -e "${GREEN}apt command found. Using apt to install dependencies${NO_COLOR}"
    # List apt package name for all dependencies
    declare -a apt_depends=("libb2-dev" "zlib1g-dev" "libbz2-dev" "liblzo2-dev"
                            "liblz4-dev" "libzstd-dev" "liblzma-dev" "lrzip"
                            "libssl-dev" "libxml2-dev" "libexpat-dev" "libpcre3-dev"
                            "libmbedtls-dev" "nettle-dev")

    # Update and upgrade apt packages to most recent version
    apt update
    apt upgrade

    # Install each listed dependency
    for i in "${apt_depends[@]}"
    do
        apt install "$i"
    done

    echo -e "${GREEN}All dependencies successfully installed${NO_COLOR}"
fi