import socket
import time
import struct
import argparse
import numpy as np # type: ignore
import matplotlib
# Use a more performant backend for real-time visualization
matplotlib.use('Qt5Agg')  # Qt5Agg is generally more performant than TkAgg
matplotlib.rcParams['toolbar'] = 'None'  # Disable toolbar for performance
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import threading

# UDP server configuration
UDP_IP = "127.0.0.1"
UDP_PORT = 5005

# Packet structure based on network.rs
# Header: 5 bytes
# - Magic number (2 bytes): 0xAD (Audio Data)
# - Version (1 byte): 0x01
# - Number of bands (1 byte): up to 255 bands
# - Flags (1 byte): bit flags for additional info
# Data: variable length
# - Timestamp (4 bytes): Unix timestamp in seconds as u32
# - Bands data (num_bands bytes): Each band as u8 (0-255)

class AudioPacket:
    def __init__(self, data):
        try:
            if len(data) < 9:
                raise ValueError(f"Packet too small: {len(data)} bytes, expected at least 9 bytes")

            # Parse header (optimized)
            self.magic = data[0:2]
            if self.magic != bytes([0xAD, 0x01]):
                raise ValueError(f"Invalid magic number: {self.magic.hex()}")

            self.version = data[2]
            if self.version != 0x01:
                raise ValueError(f"Unsupported version: {self.version}")

            self.num_bands = data[3]
            self.flags = data[4]

            # Sanity check for num_bands
            if self.num_bands == 0 or self.num_bands > 255:
                raise ValueError(f"Invalid number of bands: {self.num_bands}")

            # Parse timestamp (4 bytes, little-endian u32)
            self.timestamp = struct.unpack("<I", data[5:9])[0]

            # Check if we have correct data length
            expected_length = 9 + self.num_bands
            if len(data) != expected_length:
                raise ValueError(f"Expected {expected_length} bytes, got {len(data)}")

            # Parse band data (optimized with direct numpy array creation)
            self.bands = np.frombuffer(data[9:], dtype=np.uint8)
            
            # Pre-compute float values for performance
            self.bands_float = self.bands.astype(np.float32) / 255.0

            # Extract flags
            self.interpolated_log = bool(self.flags & 0x01)

        except Exception as e:
            # Convert to hex for easy debugging
            hex_data = ' '.join(f'{b:02x}' for b in data[:min(32, len(data))])
            if len(data) > 32:
                hex_data += '...'
            raise ValueError(f"Failed to parse packet: {e}\nHex data: {hex_data}") from e

    def __str__(self):
        return f"AudioPacket: {self.num_bands} bands, flags={self.flags}, timestamp={self.timestamp}"

# Setup socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))
sock.settimeout(0.001)  # Even shorter timeout for more responsive updates

print(f"Listening for UDP messages on {UDP_IP}:{UDP_PORT}")
print(f"Expecting packets with format matching Rust implementation:")
print(f"  - Magic number: 0xAD01")
print(f"  - Version: 0x01")
print(f"  - Number of bands: variable")
print(f"  - Flags: bit 0 = interpolated bands with logarithmic scale")
print(f"  - Timestamp: 4 bytes (little-endian u32)")
print(f"  - Band data: variable length array of u8 values")

# Variables for FPS calculation
packet_count = 0
start_time = time.time()
last_display_time = start_time

# Buffer for storing latest packet data to reduce contention between network and rendering
packet_buffer = None
packet_lock = threading.Lock()

# Setup visualization with optimized parameters
plt.style.use('dark_background')
fig, ax = plt.subplots(figsize=(12, 6))
plt.get_current_fig_manager().set_window_title('Audio Spectrum Analyzer')

# Create initial empty bars
bars = ax.bar([], [], color='cyan', alpha=0.8)
ax.set_ylim(0, 1)
ax.set_title('Audio Spectrum Analyzer')
ax.set_xlabel('Frequency Band')
ax.set_ylabel('Amplitude')

# Add a text element for FPS display that we can update
fps_text = ax.text(0.02, 0.95, '', transform=ax.transAxes, color='yellow')

# Function to receive packets in a separate thread to avoid blocking the visualization
def receive_packets():
    global packet_count, packet_buffer, packet_lock
    
    while True:
        try:
            # Try to receive data
            packet, addr = sock.recvfrom(265)  # 5 + 4 + 256 = 265 bytes is enough for most cases
            
            try:
                # Parse the packet
                audio_packet = AudioPacket(packet)
                
                # Update the buffer with latest packet
                with packet_lock:
                    packet_buffer = audio_packet
                    packet_count += 1
                    
            except Exception as e:
                # Silent fail for malformed packets
                pass
                
        except socket.timeout:
            # No data received, just continue
            pass
        except Exception as e:
            # Break on socket error
            print(f"Socket error: {e}")
            break

# Optimized plot update function that only reads from the buffer
def update_plot(frame):
    global packet_buffer, packet_lock, bars, start_time, last_display_time, fps_text
    
    current_time = time.time()
    elapsed = current_time - last_display_time
    
    # Lock to safely access the packet buffer
    with packet_lock:
        local_packet = packet_buffer
        local_count = packet_count
        packet_buffer = None
    
    # If we have a new packet to visualize
    if local_packet is not None:
        num_bands = local_packet.num_bands
        
        # If the number of bands has changed, efficiently recreate the bars
        if len(bars.patches) != num_bands:
            # Clear and create new bars
            ax.clear()
            x = np.arange(num_bands)
            bars = ax.bar(x, np.zeros(num_bands), color='cyan', alpha=0.8)
            
            # Reset axis properties
            ax.set_ylim(0, 1)
            ax.set_title('Audio Spectrum Analyzer')
            ax.set_xlabel('Frequency Band')
            ax.set_ylabel('Amplitude')
            fps_text = ax.text(0.02, 0.95, '', transform=ax.transAxes, color='yellow')
        
        # Update all bar heights at once using vectorized operations
        heights = np.zeros(len(bars.patches))
        heights[:num_bands] = local_packet.bands_float
        
        # Batch update bar heights - much faster than per-bar updates
        for i, h in enumerate(heights):
            bars.patches[i].set_height(h)
        
        # Update FPS counter and other stats
        if elapsed >= 1.0:
            fps = local_count / (current_time - start_time)
            fps_text.set_text(f'FPS: {fps:.1f}')
            
            # Add flag indicator to title
            flags_text = f"Interpolated & Log: {'Yes' if local_packet.interpolated_log else 'No'}"
            ax.set_title(f'Audio Spectrum Analyzer - {flags_text}')
            
            # Reset timing for next interval
            last_display_time = current_time
    
    # Return the artists that were modified
    return [*bars.patches, fps_text]


# Parse command-line arguments
parser = argparse.ArgumentParser(description='Audio visualizer test server')
parser.add_argument('--mode', choices=['fps_only', 'visualizer'],
                    default='visualizer',
                    help='Operation mode: \n'
                         '  visualizer: Show graphical spectrum visualization (default)\n'
                         '  fps_only: Display packet statistics and FPS only, no visualization')
parser.add_argument('--interval', type=int, default=20, 
                    help='Update interval in milliseconds (default: 20ms = 50fps)')
args = parser.parse_args()

# Print information about the current mode
print(f"\nRunning in {args.mode.upper()} mode")
if args.mode == 'visualizer':
    print(f"Showing graphical spectrum visualization with {args.interval}ms update interval.")
    print("Close window or press Ctrl+C to exit.")
else:
    print("Showing packet statistics only. Press Ctrl+C to exit.")

# Start the animation with optimized parameters
try:
    if args.mode == 'visualizer':
        # Start packet receiving thread
        receiver_thread = threading.Thread(target=receive_packets, daemon=True)
        receiver_thread.start()
        
        # Use a more performant animation approach
        ani = FuncAnimation(
            fig, 
            update_plot, 
            interval=args.interval,  # User-configurable refresh rate
            blit=True,               # Use blitting for maximum performance
            cache_frame_data=False   # Don't cache frames to save memory
        )
        
        # Enable fast rendering
        plt.rcParams['figure.autolayout'] = True
        plt.tight_layout()
        
        # Show the plot and start the main loop
        plt.show()
    else:  # fps_only mode
        print("Running in FPS-only mode. Press Ctrl+C to exit.")

        # Reset counters for this mode
        packet_count = 0
        start_time = time.time()
        last_display_time = start_time

        # Create a packet buffer for the fps_only mode
        latest_packet = None

        while True:
            try:
                # Try to receive data (with a tiny timeout for responsiveness)
                try:
                    packet, addr = sock.recvfrom(265)
                    packet_count += 1
                    
                    # Only parse the packet when we need to display stats
                    current_time = time.time()
                    elapsed = current_time - last_display_time
                    
                    if elapsed >= 1.0:  # Parse only when we're about to display
                        latest_packet = AudioPacket(packet)
                        
                except socket.timeout:
                    pass  # No data, continue silently
                
                # Calculate and display FPS every second
                current_time = time.time()
                elapsed = current_time - last_display_time

                if elapsed >= 1.0:  # Update display every second
                    fps = packet_count / (current_time - start_time)
                    print("\n" + "="*50)
                    print(f"Stats update at {time.strftime('%H:%M:%S')}:")
                    print(f"Received {packet_count} packets. FPS: {fps:.2f}")

                    # Parse and display packet info only if we have a valid packet
                    if latest_packet:
                        print("\nLatest packet details:")
                        print(f"  Magic: {latest_packet.magic.hex()}, Version: {latest_packet.version}")
                        print(f"  Bands: {latest_packet.num_bands}, Flags: {latest_packet.flags:08b}")
                        print(f"  Timestamp: {latest_packet.timestamp}, Data length: {len(packet)}")

                        # Print a few band values as sample
                        if len(latest_packet.bands) > 0:
                            print("\nBand samples:")
                            print(f"  First: {latest_packet.bands[0]} ({latest_packet.bands_float[0]:.2f})")
                            mid_idx = len(latest_packet.bands)//2
                            print(f"  Middle: {latest_packet.bands[mid_idx]} ({latest_packet.bands_float[mid_idx]:.2f})")
                            print(f"  Last: {latest_packet.bands[-1]} ({latest_packet.bands_float[-1]:.2f})")

                            # Add flag info
                            print(f"\nFlags: {'Interpolated & Log' if latest_packet.interpolated_log else 'Raw'}")

                    print("="*50)
                    last_display_time = current_time
                    
                # Small sleep to prevent CPU hogging in the loop
                time.sleep(0.001)
                
            except KeyboardInterrupt:
                break
            except Exception as e:
                print(f"Error: {e}")
                time.sleep(0.1)  # Sleep a bit longer on error
except Exception as e:
    print(f"Error in animation: {e}")
    import traceback
    traceback.print_exc()
finally:
    # Clean up resources
    print("Closing socket...")
    sock.close()