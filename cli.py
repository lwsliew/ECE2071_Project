import serial
import subprocess
import time
import numpy as np
import matplotlib.pyplot as plt


def generate_reports(file_path, sample_rate=44100):
    print("Generating wave plot and CSV report...\n")

    raw_bytes = np.fromfile(file_path, dtype=np.uint8)
    n = len(raw_bytes) // 3
    a = (raw_bytes[0:n*3:3].astype(np.uint16) << 4) | (raw_bytes[1:n*3:3] >> 4)
    b = ((raw_bytes[1:n*3:3].astype(np.uint16) & 0x0F) << 8) | raw_bytes[2:n*3:3]

    samples = np.empty(n * 2, dtype=np.int32)
    samples[0::2] = (a.astype(np.int32) - 2048) * 16
    samples[1::2] = (b.astype(np.int32) - 2048) * 16

    duration = len(samples) / sample_rate
    time_axis = np.linspace(0, duration, len(samples))

    csv_filename = "audio_log.csv"
    np.savetxt(csv_filename, np.column_stack((time_axis, samples)),
               delimiter=",", header=f"Sample rate (Hz): {sample_rate}\nTime (s), Amplitude")

    plt.figure(figsize=(12, 6))
    plt.plot(time_axis, samples, linewidth=0.5)
    plt.title(f"Audio Waveform Analysis (Fs = {sample_rate} Hz)", fontsize=15)
    plt.xlabel("Time (seconds)", fontsize=12)
    plt.ylabel("Amplitude", fontsize=12)
    plt.savefig("waveform_plot.png", dpi=300)
    plt.close()
    print("All reports generated\n")
        

# Connect to the serial port
ser = serial.Serial(port="COM9", baudrate=921600, bytesize=8, parity="N", stopbits=1, timeout=3)
print(f"Connected to {ser.name}")
commands = ["manual", "distance"]


while True:
    user_input = input("Enter mode: ").strip()
    bytes = user_input.ljust(8)
    ser.write(bytes.encode())

    if user_input not in commands:
        print("Invalid command! Please enter 'distance' or 'manual'")
        continue

    if user_input == "manual":
        duration = int(input("Enter duration in seconds: "))
        duration_bytes = str(duration).ljust(4).encode()
        ser.write(duration_bytes)

    print("Command sent!\n")

    total_data = 50 * (10 ** 3)
    chunk_data = 500
    input_data = 0

    print("Recording audio...")
    ser.reset_input_buffer()
    started_receiving = False
    file_1 = open("raw_ADC_values.data", "wb")

    if user_input == "manual":
        # Calculate exactly how many bytes 6 seconds is (6 * 66150 = 396900)
        total_expected = int(duration * 66150)
        print(f"Expecting exactly {total_expected} bytes. Recording...")
        
        # Give Windows permission to wait the full 6 seconds
        ser.timeout = duration + 2 
        
        # THE NUCLEAR READ: Grab the entire file in one giant bite.
        # Python will literally freeze here until the STM32 finishes. No loops!
        x = ser.read(total_expected)
        
        # Save it
        file_1.write(x)
        
        if len(x) == total_expected:
            print(f"\nPerfect Success! Captured all {len(x)} bytes.")
        else:
            print(f"\nWarning: Only captured {len(x)} bytes before timing out.")
            
        print("Manual recording complete!")
        
                    
    elif user_input == "distance":
            print("Waiting for distance sensor to trigger... wave your hand!") # Let you know it's in the loop
            
            while True:
                bytestoRead = ser.in_waiting
                
                # Print on the same line so it doesn't flood your terminal
                print(f"Polling... Bytes in waiting: {bytestoRead}", end='\r') 
                
                if bytestoRead > 0:
                    x = ser.read(bytestoRead)
                    file_1.write(x)

                    if not started_receiving:
                        print("\nTransmission started!") # \n pushes it to a new line past the polling text
                        started_receiving = True
                else:
                    if started_receiving:
                        print("\nSensor blocked transmission.\n")
                        break  
            

    file_1.close()
    print("\nRecording complete! Data saved to file.\n")

    print("Converting to .wav file...")
    output_filename = "recorded_audio.wav"


    subprocess.run(["convert.exe", "raw_ADC_values.data", output_filename], check = True)
    raw = np.fromfile("raw_ADC_values.data", dtype=np.uint8)
    print("\n--- HEX DUMP (First 12 Bytes) ---")
    try:
        with open("raw_ADC_values.data", "rb") as f:
            bytes_read = f.read(12)
            # This prints the raw bytes as two-digit Hex numbers
            print(" ".join([f"{b:02X}" for b in bytes_read]))
    except Exception as e:
        print("Could not read hex dump:", e)
    n = len(raw) // 3
    a = (raw[0:n*3:3].astype(np.uint16) << 4) | (raw[1:n*3:3] >> 4)
    b = ((raw[1:n*3:3].astype(np.uint16) & 0x0F) << 8) | raw[2:n*3:3]
    samples = np.concatenate([a, b])
    print("min:", samples.min(), "max:", samples.max(), "mean:", samples.mean())
    print("Expected mean ~2048 for centered audio")
    generate_reports("raw_ADC_values.data")
    