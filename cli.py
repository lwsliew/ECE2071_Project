import serial
import subprocess
import time
import numpy as np
import matplotlib.pyplot as plt


def generate_reports(file_path, sample_rate=6400):
    print("Generating wave plot and CSV report...\n")

    raw_data = np.fromfile(file_path, dtype=np.uint16)
    processed_data = (raw_data.astype(np.int32) - 2048) * 16
    duration = len(processed_data) / sample_rate
    time_axis = np.linspace(0, duration, len(processed_data))

    # Generate csv file
    csv_filename = "audio_log.csv"
    data_to_save= np.column_stack((time_axis, processed_data))
    header_text = f"Sample rate (Hz): 6400\nTime (s), Amplitude"
    
    np.savetxt(csv_filename, data_to_save, delimiter=",", header=header_text)

    # Generate png plot
    plt.figure(figsize=(12, 6))
    plt.plot(time_axis, processed_data, linewidth=0.5)

    plt.title(f"Audio Waveform Analysis (Fs = {sample_rate} Hz)", fontsize = 15)
    plt.xlabel("Time (seconds)", fontsize = 12)
    plt.ylabel("Amplitude", fontsize = 12)
    
    plot_filename = "waveform_plot.png"
    plt.savefig(plot_filename, dpi=300)
    plt.close()

    print("All reports generated\n")
        

# Connect to the serial port
ser = serial.Serial(port="COM9", baudrate=230400, bytesize=8, parity="N", stopbits=1, timeout=3)
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
        duration_bytes = str(duration).encode()
        duration_bytes = duration_bytes.ljust(4, b'\x00')
        time.sleep(0.1)
        ser.write(duration_bytes)

    print("Command sent!\n")

    total_data = 50 * (10 ** 3)
    chunk_data = 500
    input_data = 0

    print("Recording audio...")
    started_receiving = False
    file_1 = open("raw_ADC_values.data", "wb")

    if user_input == "manual":
        # For manual, read until silence persists for > 1 second
        silence_count = 0
        while True:
            bytestoRead = ser.in_waiting
            print(bytestoRead)
            if bytestoRead > 0:
                x = ser.read(bytestoRead)
                file_1.write(x)
                started_receiving = True
                silence_count = 0  # reset silence counter on data
            else:
                if started_receiving:
                    silence_count += 1
                    if silence_count > 20:  # ~1s of silence at 50ms polling
                        print("Manual recording complete!")
                        break
                    
            time.sleep(0.05)

    elif user_input == "distance":
        while True:
            bytestoRead = ser.in_waiting
            if bytestoRead > 0:
                x = ser.read(bytestoRead)
                file_1.write(x)

                if not started_receiving:
                    print("Transmission started!")
                    started_receiving = True
            else:
                if started_receiving:
                    print("\nSensor blocked transmission.\n")
                    break  # exit when sensor stops triggering
            time.sleep(0.05)

    file_1.close()
    print("\nRecording complete! Data saved to file.\n")

    print("Converting to .wav file...")
    output_filename = "recorded_audio.wav"

    subprocess.run(["convert.exe", "raw_ADC_values.data", output_filename], check = True)
    generate_reports("raw_ADC_values.data")
    