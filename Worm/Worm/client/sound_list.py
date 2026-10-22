import subprocess
import sounddevice as sd

if __name__ == "__main__":
    subprocess.run(["arecord", "-l"])
    print()
    print(sd.query_devices())