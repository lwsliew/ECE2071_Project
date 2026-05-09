import serial
import subprocess
import time
import numpy as np
import matplotlib.pyplot as plt

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

def process_data(file_path, wav, png, csv, sample_rate=44100):
    print("\n[ Processing Data ]")
    
    # 1. WAV Generation (Handled by C Executable)
    if wav:
        print(" -> Compiling/Converting to .wav format...")
        try:
            subprocess.run(["convert.exe", file_path, "recorded_audio.wav"], check=True)
            print("    [Success] Saved 'recorded_audio.wav'")
        except FileNotFoundError:
            print("    [Error] 'convert.exe' not found in directory.")
            
    # 2. Numpy/Matplotlib Math (Only run if user asked for CSV or PNG)
    if png or csv:
        print(" -> Decoding raw bits for graphing/logging...")
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

            total_expected = int(duration * 66150)
            print(f"\n[*] Recording for {duration} seconds... Please wait.")
            
            ser.reset_input_buffer()
            
            raw_data = bytearray()
            start_time = time.time()
            
            while len(raw_data) < total_expected:
                bytes_waiting = ser.in_waiting
                if bytes_waiting > 0:
                    # Read what's available, but don't over-read past the expected total
                    chunk_size = min(bytes_waiting, total_expected - len(raw_data))
                    raw_data.extend(ser.read(chunk_size))
                
                # Timeout safety net (Duration + 2.0 seconds)
                if time.time() - start_time > duration + 2.0:
                    break
            
            with open("raw_ADC_values.data", "wb") as f:
                f.write(raw_data)
                
            if len(raw_data) == total_expected:
                print("[*] Recording complete. 100% Data integrity verified.")

            else:
                print(f"[!] Warning: Expected {total_expected} bytes but captured {len(raw_data)}.")

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
            print("\n[*] Distance Mode Active. [Press Ctrl+C to return to Main Menu]")
            
            try:
                while True:
                    print("\nWaiting for ultrasonic trigger... (Wave hand to start)")
                    ser.reset_input_buffer()
                    started_receiving = False
                    
                    with open("raw_ADC_values.data", "wb") as file_1:
                        last_receive_time = time.time()
                        
                        while True:
                            bytestoRead = ser.in_waiting
                            
                            if bytestoRead > 0:
                                file_1.write(ser.read(bytestoRead))
                                last_receive_time = time.time()
                                
                                if not started_receiving:
                                    print("[*] Object Detected! Recording audio...") 
                                    started_receiving = True
                            else:
                                # Wait for 1 full second of silence before closing the file
                                if started_receiving and (time.time() - last_receive_time > 0.5):
                                    print("[*] Object removed. Sensor cooldown finished.")
                                    break 
                                    
                    # Once a trigger finishes, process the data immediately
                    process_data("raw_ADC_values.data", wav, png, csv)
                    
            except KeyboardInterrupt:
                print("\n\n[*] Exiting Distance Trigger Mode...")
                # Optional: Send a dummy byte or command to ensure STM32 state clears if needed

        # -----------------------------------------
        # EXIT APPLICATION
        # -----------------------------------------
        elif choice == 'exit':
            print("\n[*] Closing port and exiting application. Goodbye!")
            ser.close()
            break

        else:
            print("\n[!] Invalid selection. Please choose 1, 2, or 3.")

if __name__ == "__main__":
    main()
    