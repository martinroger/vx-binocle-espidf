import socket
import struct
import json
import time
import sys

UDP_IP = 'binocle-emulator.local' # or target IP
UDP_PORT = 8888

# 1. Binary Packet Sender
# Packet layout:
# [magic 2B (0xAA, 0x55)] [telltale_mask 2B] [speed_freq_x10 2B] [rpm_freq_x10 2B] [coolant_duty_x100 2B] [fuel_ohm_x10 2B] [checksum 2B]
def build_binary_packet(telltales_mask, speed_freq_hz, rpm_freq_hz, coolant_duty_pct, fuel_target_ohm):
    magic = bytes([0xAA, 0x55])
    speed_x10 = int(round(speed_freq_hz * 10.0)) & 0xFFFF
    rpm_x10 = int(round(rpm_freq_hz * 10.0)) & 0xFFFF
    coolant_x100 = int(round(coolant_duty_pct * 100.0)) & 0xFFFF
    fuel_x10 = int(round(fuel_target_ohm * 10.0)) & 0xFFFF
    mask = int(telltales_mask) & 0xFFFF

    payload_12 = struct.pack('<2sHHHHH', magic, mask, speed_x10, rpm_x10, coolant_x100, fuel_x10)
    checksum = sum(payload_12) & 0xFFFF
    return payload_12 + struct.pack('<H', checksum)

def build_json_packet(telltales_mask, speed_freq_hz, rpm_freq_hz, coolant_duty_pct, fuel_target_ohm):
    payload = {
        'telltales': telltales_mask,
        'speed_freq': round(speed_freq_hz, 2),
        'rpm_freq': round(rpm_freq_hz, 2),
        'coolant_duty': round(coolant_duty_pct, 2),
        'fuel_ohm': round(fuel_target_ohm, 2)
    }
    return json.dumps(payload).encode('utf-8')

def test_send():
    target_host = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith('-') else UDP_IP
    target_port = int(sys.argv[2]) if len(sys.argv) > 2 and not sys.argv[2].startswith('-') else UDP_PORT
    sweep_mode = '--sweep' in sys.argv or '-s' in sys.argv

    try:
        resolved_ip = socket.gethostbyname(target_host)
        print(f"Resolved {target_host} -> {resolved_ip}")
    except Exception as e:
        resolved_ip = target_host
        print(f"Could not resolve {target_host}: {e}, using as-is")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    if sweep_mode:
        print(f"Starting continuous sweep mode against {resolved_ip}:{target_port} (Ctrl+C to stop)...")
        step = 0
        try:
            while True:
                # Oscillate speed 0..120 km/h (0..490 Hz), RPM 800..5000 (26..166 Hz)
                import math
                t = time.time()
                speed_hz = 245.0 + 245.0 * math.sin(t * 1.5)
                rpm_hz = 96.0 + 70.0 * math.sin(t * 2.0)
                coolant = 50.0 + 40.0 * math.sin(t * 0.8)
                fuel_r = 150.0 + 100.0 * math.sin(t * 0.5)
                # Alternating telltales
                mask = 0xD940 if int(t * 2) % 2 == 0 else 0xD94F

                pkt = build_binary_packet(mask, speed_hz, rpm_hz, coolant, fuel_r)
                sock.sendto(pkt, (resolved_ip, target_port))
                step += 1
                if step % 20 == 0:
                    print(f"[{step}] Speed: {speed_hz:.1f}Hz, RPM: {rpm_hz:.1f}Hz, Coolant: {coolant:.1f}%, Fuel: {fuel_r:.1f}Ω")
                time.sleep(0.05) # 20 Hz
        except KeyboardInterrupt:
            print("\nSweep stopped.")
            return

    print(f'Sending test UDP datagrams to {resolved_ip}:{target_port}...')

    # Example 1: Binary Frame
    bin_pkt = build_binary_packet(
        telltales_mask=0xD940, # Ignition + cluster lights
        speed_freq_hz=408.9,   # ~100 km/h
        rpm_freq_hz=100.0,     # ~3000 RPM
        coolant_duty_pct=35.7, # ~90 degC
        fuel_target_ohm=120.0  # Snaps to Step 10 (118.9 Ohm / Half Tank)
    )
    sock.sendto(bin_pkt, (resolved_ip, target_port))
    print(f'Sent binary packet ({len(bin_pkt)} bytes): {bin_pkt.hex()}')

    # Example 2: JSON Frame
    json_pkt = build_json_packet(
        telltales_mask=0xD940,
        speed_freq_hz=408.9,
        rpm_freq_hz=100.0,
        coolant_duty_pct=35.7,
        fuel_target_ohm=120.0
    )
    sock.sendto(json_pkt, (resolved_ip, target_port))
    print(f'Sent JSON packet ({len(json_pkt)} bytes): {json_pkt.decode()}')

if __name__ == '__main__':
    test_send()

