#!/usr/bin/env python3
"""
Simple UDP server to receive and display compact audio visualizer data
"""

import socket
import struct
import time
from datetime import datetime

def parse_compact_packet(data):
    """
    Parse the compact binary audio packet format:
    - Magic number (2 bytes): 0xAD, 0x01
    - Version (1 byte): 0x01
    - Number of bands (1 byte): up to 255 bands
    - Timestamp (4 bytes): Unix timestamp in seconds (little-endian u32)
    - Bands data (num_bands bytes): Each band as u8 (0-255)
    """
    if len(data) < 8:
        return None
    
    # Parse header
    magic1, magic2, version, num_bands = struct.unpack('BBBB', data[:4])
    
    # Verify magic number
    if magic1 != 0xAD or magic2 != 0x01:
        return None
    
    # Parse timestamp (little-endian u32)
    timestamp = struct.unpack('<I', data[4:8])[0]
    
    # Check packet length
    if len(data) != 8 + num_bands:
        return None
    
    # Parse band data
    bands = list(struct.unpack(f'{num_bands}B', data[8:8+num_bands]))
    
    # Convert u8 bands back to float (0-255 -> 0.0-1.0)
    bands_float = [b / 255.0 for b in bands]
    
    return {
        'version': version,
        'num_bands': num_bands,
        'timestamp': timestamp,
        'bands': bands_float,
        'bands_raw': bands,
        'packet_size': len(data)
    }

def main():
    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', 8080))
    
    print("Compact UDP Audio Visualizer Server")
    print("==================================")
    print("Listening on 127.0.0.1:8080")
    print("Expecting compact binary audio packets")
    print("Packet format: [Magic:2][Ver:1][Bands:1][Time:4][Data:N]")
    print()
    
    try:
        while True:
            data, addr = sock.recvfrom(1024)  # Much smaller buffer needed now
            
            # Parse the compact packet
            packet = parse_compact_packet(data)
            
            if packet is None:
                print(f"Invalid packet received from {addr} (size: {len(data)} bytes)")
                continue
            
            # Convert timestamp to readable format
            dt = datetime.fromtimestamp(packet['timestamp'])
            
            # Display packet info
            print(f"\n[{dt.strftime('%H:%M:%S')}] Compact Audio Packet")
            print(f"  From: {addr}")
            print(f"  Size: {packet['packet_size']} bytes (vs ~{len(str(packet['bands_float'])) + 50} bytes JSON)")
            print(f"  Bands: {packet['num_bands']}")
            print(f"  Version: {packet['version']}")
            
            # Show a simple bar chart (every 4th band to avoid clutter)
            bands = packet['bands_float']
            for i in range(0, len(bands), max(1, len(bands) // 16)):
                bar_length = int(bands[i] * 50)  # Scale to 50 characters max
                bar = '█' * bar_length + '░' * (50 - bar_length)
                print(f"  Band {i:2d}: {bar} {bands[i]:.3f} (raw: {packet['bands_raw'][i]:3d})")
            
            # Show statistics
            max_amp = max(bands) if bands else 0
            avg_amp = sum(bands) / len(bands) if bands else 0
            print(f"  Stats: Max={max_amp:.3f}, Avg={avg_amp:.3f}")
            
            # Show compression ratio
            json_size_estimate = len(str(bands)) + 50  # Rough estimate
            compression_ratio = json_size_estimate / packet['packet_size']
            print(f"  Compression: ~{compression_ratio:.1f}x smaller than JSON")
                
    except KeyboardInterrupt:
        print("\nServer stopped by user")
    finally:
        sock.close()

if __name__ == "__main__":
    main()
