import serial
import subprocess
import time
import numpy as np
import matplotlib.pyplot as plt

PAYLOAD_SIZE = 3000
PACKET_HEADER_1 = b'\xaa'
PACKET_HEADER_2 = b'\x55'

def print_header():
    print("\n" + "=" * 55)
    print("\tECE2071 Audio Sampling System\t")
    print("="*55)

def print_menu():
    print("\n[ MAIN MENU ]")
    print(" > 'manual' : Record for a fixed duration")
    print(" > 'distance' - Record via ultrasonic sensor")
    print(" > 'exit' : Close the application")

def get_output_preferences():
    print("\n[ Output Format Configuration ]")
    print("Select the output files you wish to generate:")
    while True:
        wav = input(" > Generate .wav audio file? (y/n): ")
        wav = wav.strip().lower() == 'y'

        png = input(" > Generate .png waveform plot? (y/n): ")
        png = png.strip().lower() == 'y'

        csv = input(" > Generate .csv raw data log? (y/n): ")
        csv = csv.strip().lower() == 'y'

        if not(wav or png or csv):
            print("Error: No outputs selected. Please input at least one output.")
            continue
            
        else:
            break
    
    return wav, png, csv

def verify_packet(ser):
    while True:
        char1 = ser.read(1)
        if not char1:
            return None

        if char1 == PACKET_HEADER_1:
            char2 = ser.read(1)
            if char2 == PACKET_HEADER_2:
                break

    data = ser.read(PAYLOAD_SIZE + 1)

    if len(data) != (PAYLOAD_SIZE + 1):
        print("Incomplete packet read")
        return None
    
    payload = data[:-1]
    received_checksum = data[-1]

    calculated_checksum = 0

    for i in payload:
        calculated_checksum ^= i

    if calculated_checksum == received_checksum:
        return payload

    else:
        print("Checksum mismatch!")
        return None


def process_data(file_path, wav, png, csv, sample_rate=44100):
    print("\n[ Processing Data ]")
    
    # 1. WAV Generation (Handled by C Executable)
    if wav:
        print("Compiling/Converting to .wav format...")
        try:
            subprocess.run(["convert.exe", file_path, "recorded_audio.wav"], check=True)
            print("    [Success] Saved 'recorded_audio.wav'")
        except FileNotFoundError:
            print("    [Error] 'convert.exe' not found in directory.")
            
    # 2. Numpy/Matplotlib Math (Only run if user asked for CSV or PNG)
    if png or csv:
        print("Decoding raw bits for graphing...")
        raw_bytes = np.fromfile(file_path, dtype=np.uint8)
        n = len(raw_bytes) // 3
        
        # Unpack the 3-byte chunks into two 12-bit samples
        a = (raw_bytes[0:n*3:3].astype(np.uint16) << 4) | (raw_bytes[1:n*3:3] >> 4)
        b = ((raw_bytes[1:n*3:3].astype(np.uint16) & 0x0F) << 8) | raw_bytes[2:n*3:3]

        samples = np.empty(n * 2, dtype=np.int32)
        samples[0::2] = (a.astype(np.int32) - 2048) * 16
        samples[1::2] = (b.astype(np.int32) - 2048) * 16

        duration = len(samples) / sample_rate
        time_axis = np.linspace(0, duration, len(samples))

        # CSV Generation
        if csv:
            csv_filename = "audio_log.csv"
            np.savetxt(csv_filename, np.column_stack((time_axis, samples)),
                       delimiter=",", header=f"Sample rate (Hz): {sample_rate}\nTime (s), Amplitude")
            print(f"    [Success] Saved '{csv_filename}'")
            
    
        if png:
            plt.figure(figsize=(12, 6))
            plt.plot(time_axis, samples, linewidth=0.5, color='b')
            plt.title(f"Audio Waveform Analysis (Fs = {sample_rate} Hz)", fontsize=15)
            plt.xlabel("Time (seconds)", fontsize=12)
            plt.ylabel("Amplitude (Scaled)", fontsize=12)
            plt.grid(True, alpha=0.3)
            plt.savefig("waveform_plot.png", dpi=300)
            plt.close()
            print("    [Success] Saved 'waveform_plot.png'")

def main():
    # Connect to serial port

    ser = serial.Serial(port="COM9", baudrate=921600, bytesize=8, parity="N", stopbits=1, timeout=3)
    print(f"Connected to {ser.name}")
    commands = ["manual", "distance"]

    print_header()

    while True:
        print_menu()
        choice = input("\nSelect an operating mode (manual, distace, exit): ").strip().lower()

        if choice == 'manual':
            print("\n" + "-"*40)
            print("       MANUAL RECORDING MODE       ")
            print("-"*40)
            
            try:
                duration = int(input("Enter duration to record (in seconds): "))
                if duration <= 0:
                    print("[!] Please enter a duration above 0.")
                    continue
            except ValueError:
                print("Invalid input. Please enter a whole number.")
                continue
                
            wav, png, csv = get_output_preferences()

            # Command the STM32
            ser.write(b"manual  ")
            duration_bytes = str(duration).ljust(4).encode()
            ser.write(duration_bytes)

            print(f"\n[*] Recording for {duration} seconds... Please wait.")
            
            ser.reset_input_buffer()
            
            raw_data = bytearray()
            start_time = time.time()
            
            bytes_per_sec = 66150
            total_expected_bytes = duration * bytes_per_sec
            packets_needed = (total_expected_bytes // PAYLOAD_SIZE) + 1
            packets_received = 0
            
            while packets_received < packets_needed * 0.95:
                clean_payload = verify_packet(ser)
                if clean_payload:
                    raw_data.extend(clean_payload)
                    packets_received += 1
                
                # Timeout safety net
                if time.time() - start_time > duration + 2.0:
                    print("[!] Recording timed out.")
                    break
            
            with open("raw_ADC_values.data", "wb") as f:
                f.write(raw_data)
                
            print(f"[*] Recording complete. Captured {len(raw_data)} verified bytes.")
            process_data("raw_ADC_values.data", wav, png, csv)

        # -----------------------------------------
        # MODE 2: DISTANCE TRIGGER
        # -----------------------------------------
        elif choice == "distance":
            print("\n" + "-"*40)
            print("       DISTANCE TRIGGER MODE       ")
            print("-"*40)
            print("Info: The ultrasonic sensor will reliably trigger at <10cm.")
            print("      Recording will automatically stop 1.5s after the object is removed.\n")
            
            wav, png, csv = get_output_preferences()

            ser.write(b"distance")
            print("\nDistance Mode Active. [Press Ctrl+C to return to Main Menu]")
            done = False
            
            try:
                while not done:
                    print("\nWaiting for ultrasonic trigger... (Wave hand to start)")
                    ser.reset_input_buffer()
                    started_receiving = False
                    raw_data = bytearray()
                    last_receive_time = time.time()
                    
                    while True:
                        # --- UPDATED DISTANCE LOOP ---
                        # If there is enough data in the buffer to start hunting for a packet...
                        if ser.in_waiting > 0:
                            clean_payload = verify_packet(ser)
                            if clean_payload:
                                raw_data.extend(clean_payload)
                                last_receive_time = time.time()
                                
                                if not started_receiving:
                                    print("[*] Object Detected! Recording audio...") 
                                    started_receiving = True
                        else:
                            # Wait for 1 full second of silence before closing the file
                            if started_receiving and (time.time() - last_receive_time > 0.5):
                                print("[*] Object removed. Sensor cooldown finished.")
                                
                                # Write all verified data to file at the end
                                with open("raw_ADC_values.data", "wb") as file_1:
                                    file_1.write(raw_data)

                                done = True
                                break 
                                
                    # Once a trigger finishes, process the data immediately
                    if len(raw_data) > 0:
                        process_data("raw_ADC_values.data", wav, png, csv)
                    
            except KeyboardInterrupt:
                print("\nExiting Distance Trigger Mode...")

        # -----------------------------------------
        # EXIT APPLICATION
        # -----------------------------------------
        elif choice == "exit": 
            print("\nClosing port and exiting application. Goodbye!")
            ser.close()
            break

        else:
            print("\nInvalid selection. Please choose 'manual', 'distance', or 'exit'.")

if __name__ == "__main__":
    main()
    