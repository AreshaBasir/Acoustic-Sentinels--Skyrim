import serial
import os

# 1. Define the exact file path and print it so you know where it goes
file_name = 'apatoto_noname.csv'
save_path = os.path.abspath(file_name)
print(f"Target Save Location: {save_path}")

try:
    # 2. Add 'timeout=1'. This forces Python to stop waiting after 1 second, 
    # check for Ctrl+C, and then go back to listening.
    ser = serial.Serial('COM8', 921600, timeout=1) 
    print("\n[ SUCCESS ] Port opened. Listening for ESP32 data...")
    print("Recording AI Data... Press Ctrl+C to stop.\n")
    
    lines_written = 0
    
    with open(save_path, 'w') as f:
        while True:
            # Read the raw bytes
            raw_line = ser.readline()
            
            # If we actually caught data (and didn't just time out)
            if raw_line:
                # Decode it. errors='ignore' prevents the script from crashing 
                # if one single byte gets corrupted by electrical noise
                decoded_line = raw_line.decode('utf-8', errors='ignore')
                f.write(decoded_line)
                
                # Visual feedback: Print an update every 5,000 lines
                lines_written += 1
                if lines_written % 5000 == 0:
                    print(f"Successfully recorded {lines_written} samples...")
                    
except KeyboardInterrupt:
    print(f"\n[ STOPPED ] Ctrl+C detected.")
except serial.SerialException as e:
    print(f"\n[ HARDWARE ERROR ] {e}")
finally:
    # Ensure the port is always safely closed, even if you unplug it
    if 'ser' in locals() and ser.is_open:
        ser.close()
    print(f"Data safely written to: {save_path}")