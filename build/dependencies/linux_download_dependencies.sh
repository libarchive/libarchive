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
# Check if yum is installed on machine
elif command -v yum &> /dev/null
then
    echo -e "${GREEN}yum command found. Using apt to install dependencies${NO_COLOR}"
    # List yum package name for all dependencies
    declare -a yum_depends=("libb2-devel" "zlib-devel" "bzip2-devel" "lzo-devel"
                            "lz4-devel" "libzstd-devel" "lzma-dev" "lrzip"
                            "openssl-dev" "libxml2-devel" "libexpat-devel"
                            "pcre3-devel" "mbedtls-devel" "nettle-devel")

    yum update
    yum upgrade

    # Install each listed dependency
    for i in "${apt_depends[@]}"
    do
        yum install "$i"
    done

    echo -e "${GREEN}All dependencies successfully installed${NO_COLOR}"

else
    echo -e "${RED}No suitable package manager found. Dependencies were not installed.${NO_COLOR}"
fi