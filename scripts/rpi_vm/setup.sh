#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Exit on undefined variables
set -u

# Print commands before executing them (optional, for debugging)
# set -x

# Get script directory
script_dir=$(dirname "$(readlink -f "$0")")

# Check if custom Docker image exists, build if not
if ! docker images --format '{{.Repository}}:{{.Tag}}' | grep -q '^pi-ci-larger:latest$'; then
    echo "Building custom Docker image 'pi-ci-larger'..."
    docker build -t pi-ci-larger "$script_dir/pi-ci-larger"
    echo "Docker image built successfully."
else
    echo "Docker image 'pi-ci-larger' already exists, skipping build."
fi

if docker ps -a --format '{{.Names}}' | grep -q '^rpi-led-matrix$'; then
    read -p "Container 'rpi-led-matrix' already exists. [D]elete, [C]ontinue anyway, or [A]bort? [d/C/a]: " confirm
    case "$confirm" in
        [Dd])
            docker rm -f rpi-led-matrix
            docker run --name rpi-led-matrix -d -p 2222:2222 -p 8888:8080 pi-ci-larger start
            ;;
        [Aa])
            echo "Aborting setup."
            exit 1
            ;;
        *)
            echo "Continuing with existing container."
            ;;
    esac
else
    docker run --name rpi-led-matrix -d -p 2222:2222 -p 8888:8080 pi-ci-larger start
fi

# Wait for the container to be ready
echo -n "Waiting for the Raspberry Pi VM to be ready..."
while ! ssh -o ConnectTimeout=1 -o StrictHostKeyChecking=no -p 2222 root@localhost "exit" >/dev/null 2>&1; do
    sleep 1
    echo -n "."
done >/dev/null 2>&1
echo " done."

# Get script directory
script_dir=$(dirname "$(readlink -f "$0")")
proj_dir="$script_dir/../.."

scp -P 2222 -o StrictHostKeyChecking=no $script_dir/../install_led_matrix.sh root@localhost:/root/install_script.sh

echo "Running install script..."
ssh -t -o StrictHostKeyChecking=no -p 2222 root@localhost "bash /root/install_script.sh"
ssh -o StrictHostKeyChecking=no -p 2222 root@localhost << 'EOF'
    rm /root/install_script.sh
    systemctl stop led-matrix.service
EOF


# Configure & build the project (run in $proj_dir)

echo "Configuring and building the project in $proj_dir..."
(
    cd $proj_dir || exit 1
    cmake --preset cross-compile
    cmake --build --preset cross-compile --target install
)

echo "Project configured and built successfully."

# Copying the built project to the VM

echo "Overwriting led-matrix files with the built project..."
scp -P 2222 -o StrictHostKeyChecking=no -r "$proj_dir/build/cross-compile/install/"* root@localhost:/opt/led-matrix/
echo "Files copied to the Raspberry Pi VM."

echo "Adding user 'pi' and configuring SSH..."
ssh -o StrictHostKeyChecking=no root@localhost -p 2222 << 'EOF'
# Add user pi
useradd -m -s /bin/bash pi
echo "pi:raspberry" | chpasswd
# Add user pi to sudoers
echo "pi ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/pi
chmod 0440 /etc/sudoers.d/pi

systemctl start led-matrix.service

# Restart SSH service to apply changes
systemctl restart ssh
EOF
