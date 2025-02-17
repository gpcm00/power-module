import os
import sys
import time

powpath = sys.argv[1]
adcpath = sys.argv[2]

print(f"adc path {adcpath}")
print(f"power path {powpath}")

def read_adc(input_filename):

    buffer_size = 2 

    input_fd = os.open(input_filename, os.O_RDONLY)
    
    content = b""
    while True:
        data = os.read(input_fd, buffer_size)
        if not data:
            break
        content += data
    
    os.close(input_fd)
    return int.from_bytes(content, 'big')

def write_pow(output_filename, content):
    output_fd = os.open(output_filename, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
    
    os.write(output_fd, content)
    
    os.close(output_fd)

if __name__ == "__main__":
    while True:
        val = read_adc(adcpath)
        val = (val>>1) & (0xFF)
        write_pow(powpath, val)
        print(val)
        time.sleep(0.1)
