use std::net::UdpSocket;

// Binary packet format for maximum compactness
// Header: 5 bytes
// - Magic number (2 bytes): 0xAD (Audio Data)
// - Version (1 byte): 0x01
// - Number of bands (1 byte): up to 255 bands
// - Flags (1 byte): bit flags for additional info
//   - bit 0: 1 = interpolated bands enabled + logarithmic scale
//   - bits 1-7: reserved for future use
// Data: variable length
// - Timestamp (4 bytes): Unix timestamp in seconds as u32
// - Bands data (num_bands bytes): Each band as u8 (0-255)
pub struct CompactAudioPacket {
    magic: [u8; 2], // 0xAD, 0x01
    version: u8,    // 0x01
    num_bands: u8,  // Number of frequency bands
    flags: u8,      // Bit flags for additional info
    timestamp: u32, // Unix timestamp in seconds
    bands: Vec<u8>, // Band amplitudes (0-255)
}

impl CompactAudioPacket {
    pub fn new(bands: Vec<f32>, interpolated_log: bool) -> Self {
        let num_bands = bands.len().min(255) as u8;
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_secs() as u32;

        // Set flags based on configuration
        let mut flags = 0u8;
        if interpolated_log {
            flags |= 1; // Set bit 0 when interpolated bands enabled and logarithmic scaling
        }

        // Log packet creation occasionally
        static mut COUNTER: u32 = 0;
        let should_log = unsafe {
            COUNTER += 1;
            COUNTER % 300 == 0 // Log every 300th packet (roughly every 5 seconds at 60fps)
        };
        
        if should_log {
            println!("[DEBUG] CompactAudioPacket.new: Creating packet with {} bands, flags={}, interpolated_log={}", 
                     num_bands, flags, interpolated_log);
            
            // Log a few band values as sample
            if !bands.is_empty() {
                println!("[DEBUG] CompactAudioPacket.new: Band samples - first: {}, middle: {}, last: {}", 
                         bands[0], 
                         bands[bands.len() / 2], 
                         bands[bands.len() - 1]);
            }
        }

        // Convert f32 bands to u8 (0-255) for compactness
        let compact_bands: Vec<u8> = bands
            .iter()
            .take(255)
            .map(|&amp| (amp * 255.0).min(255.0) as u8)
            .collect();

        Self {
            magic: [0xAD, 0x01],
            version: 0x01,
            num_bands,
            flags,
            timestamp,
            bands: compact_bands,
        }
    }

    pub fn to_bytes(&self) -> Vec<u8> {
        let mut packet = Vec::with_capacity(9 + self.bands.len());

        // Header (9 bytes total)
        packet.extend_from_slice(&self.magic); // 2 bytes
        packet.push(self.version); // 1 byte
        packet.push(self.num_bands); // 1 byte
        packet.push(self.flags); // 1 byte (new)
        packet.extend_from_slice(&self.timestamp.to_le_bytes()); // 4 bytes

        // Band data (num_bands bytes)
        packet.extend_from_slice(&self.bands);

        packet
    }

    #[allow(dead_code)]
    pub fn from_bytes(data: &[u8]) -> Option<Self> {
        if data.len() < 9 {
            return None;
        }

        let magic = [data[0], data[1]];
        if magic != [0xAD, 0x01] {
            return None;
        }

        let version = data[2];
        let num_bands = data[3];
        let flags = data[4];
        let timestamp = u32::from_le_bytes([data[5], data[6], data[7], data[8]]);

        if data.len() != 9 + num_bands as usize {
            return None;
        }

        let bands = data[9..].to_vec();

        Some(Self {
            magic,
            version,
            num_bands,
            flags,
            timestamp,
            bands,
        })
    }
}

pub trait NetworkSender {
    fn send_audio_data(&self, bands: Vec<f32>, target_addr: &str, interpolated_log: bool) -> Result<(), std::io::Error>;
}

pub struct UdpSender {
    socket: UdpSocket,
}

impl UdpSender {
    pub fn new() -> Result<Self, std::io::Error> {
        let socket = UdpSocket::bind("0.0.0.0:0")?;
        Ok(Self { socket })
    }

    pub fn try_clone(&self) -> Result<Self, std::io::Error> {
        let socket = self.socket.try_clone()?;
        Ok(Self { socket })
    }
}

impl NetworkSender for UdpSender {
    fn send_audio_data(&self, bands: Vec<f32>, target_addr: &str, interpolated_log: bool) -> Result<(), std::io::Error> {
        // Only log occasionally to avoid flooding the console
        static mut COUNTER: u32 = 0;
        let should_log = unsafe {
            COUNTER += 1;
            COUNTER % 100 == 0 // Log every 100th packet (roughly every 1.6 seconds at 60fps)
        };
        
        if should_log {
            println!("[DEBUG] UdpSender.send_audio_data: Sending {} bands to {}, interpolated_log={}", 
                     bands.len(), target_addr, interpolated_log);
        }
        
        let packet = CompactAudioPacket::new(bands, interpolated_log);
        let data = packet.to_bytes();
        
        if should_log {
            println!("[DEBUG] UdpSender.send_audio_data: Packet size {} bytes", data.len());
        }
        
        match self.socket.send_to(&data, target_addr) {
            Ok(bytes_sent) => {
                if should_log {
                    println!("[DEBUG] UdpSender.send_audio_data: Sent {} bytes successfully", bytes_sent);
                }
                Ok(())
            },
            Err(e) => {
                println!("[ERROR] UdpSender.send_audio_data: Failed to send: {}", e);
                Err(e)
            }
        }
    }
}
