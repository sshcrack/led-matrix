import socket
import time

UDP_IP = "127.0.0.1"
UDP_PORT = 5005

sock = socket.socket(socket.AF_INET, # Internet
                     socket.SOCK_DGRAM) # UDP
sock.bind((UDP_IP, UDP_PORT))

print("Listening for UDP messages on {}:{}".format(UDP_IP, UDP_PORT))

# Variables for FPS calculation
packet_count = 0
start_time = time.time()
last_display_time = start_time

while True:
    data, addr = sock.recvfrom(73) # buffer size is 1024 bytes
    packet_count += 1
    
    # Calculate and display FPS every second
    current_time = time.time()
    elapsed = current_time - last_display_time
    
    if elapsed >= 1.0:  # Update display every second
        fps = packet_count / (current_time - start_time)
        print(f"Received {packet_count} packets. FPS: {fps:.2f}")
        last_display_time = current_time