#!/bin/bash

docker run --name rpi-led-matrix --rm -p 2222:2222 -p 8888:8080 ptrsr/pi-ci start

# Wait for the container to be ready
local ready=false
echo -n "Waiting for the Raspberry Pi VM to be ready..."
while [ "$ready" = false ]; do
    sleep 1
    echo -n "."
    if ssh -o StrictHostKeyChecking=no -p 2222 root@localhost "exit" 2>/dev/null; then
        ready=true
    fi
done
echo " done."

# Get script directory
script_dir=$(dirname "$(readlink -f "$0")")
root_dir = "$script_dir/../.."

scp -P 2222 -o StrictHostKeyChecking=no $script_dir/start.sh root@localhost:/root/install_script.sh

echo "Running install script..."
ssh root@localhost -p 2222 "bash /root/install_script.sh"
ssh root@localhost -p 2222 << 'EOF'
    rm /root/install_script.sh
    systemctl stop led-matrix.service
EOF


# Configure & build the project (run in $root_dir)

echo "Configuring and building the project in $root_dir..."
(
    cd $root_dir || exit 1
    cmake --preset cross-compile
    cmake --build --preset cross-compile --target install
)

echo "Project configured and built successfully."

# Copying the built project to the VM

echo "Overwriting led-matrix files with the built project..."
scp -P 2222 -r "$root_dir/build/cross-compile/install/"* root@localhost:/opt/led-matrix/
echo "Files copied to the Raspberry Pi VM."

echo "Adding user 'pi' and configuring SSH..."
ssh root@localhost -p 2222 << 'EOF'
# Add user pi
useradd -m -s /bin/bash pi
echo "pi:raspberry" | chpasswd
# Add user pi to sudoers
echo "pi ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/pi
chmod 0440 /etc/sudoers.d/pi

# Disable root login via SSH
sed -i 's/^PermitRootLogin yes/PermitRootLogin no/' /etc/ssh/sshd_config

systemctl start led-matrix.service

# Restart SSH service to apply changes
systemctl restart ssh
EOF
