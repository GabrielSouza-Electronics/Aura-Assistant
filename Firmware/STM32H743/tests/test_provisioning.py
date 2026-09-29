"""Compile production provisioning C against a fake ST transport and RTOS."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default='gcc')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='aura_provision_') as directory:
    tmp = Path(directory)
    includes = ['-I' + str(root / path) for path in (
        'tests/provisioning_fakes', 'App/Inc',
        'Middlewares/ST/ST67W6X_Network_Driver/Driver/W61_at')]
    common = [args.compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-g', *includes]
    source = (root / 'Middlewares/ST/ST67W6X_Network_Driver/Driver/W61_at/w61_at_ble.c').read_text()
    functions = []
    for name in ('W61_Ble_CreateService', 'W61_Ble_CreateCharacteristic', 'W61_Ble_SetAdvParam'):
        begin = source.index(f'W61_Status_t {name}(')
        end = source.index('\nW61_Status_t ', begin + 1)
        functions.append(source[begin:end])
    (tmp / 'w61_gatt_under_test.inc').write_text('\n'.join(functions))
    exe = tmp / 'w61_gatt.exe'
    subprocess.run([*common, '-I' + str(tmp), str(root / 'tests/test_w61_gatt.c'),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    core = root / 'Middlewares/ST/ST67W6X_Network_Driver/Core'
    source = (core / 'w6x_ble.c').read_text()
    begin = source.index('volatile W6X_Ble_InitDiagnostics_t w6x_ble_init_diagnostics')
    end = source.index('\n#if (W6X_ASSERT_ENABLE', begin)
    diagnostics = source[begin:end]
    begin = source.index('W6X_Status_t W6X_Ble_Init(')
    end = source.index('\nW6X_Status_t W6X_Ble_SetRecvDataPtr(', begin)
    (tmp / 'w6x_ble_init_under_test.inc').write_text(diagnostics + source[begin:end])
    exe = tmp / 'w6x_ble_init.exe'
    subprocess.run([*common, '-I' + str(tmp), '-I' + str(core),
                    str(root / 'tests/test_w6x_ble_init.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    protocol = root / 'App/Src/provision_protocol.c'
    exe = tmp / 'protocol.exe'
    subprocess.run([*common, str(protocol), str(root / 'tests/test_provision_protocol.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    source = (root / 'Middlewares/ST/ST67W6X_Network_Driver/Driver/W61_at/w61_at_wifi.c').read_text()
    functions = []
    for name in ('W61_WiFi_Connect', 'W61_WiFi_DeleteCredentials'):
        begin = source.index(f'W61_Status_t {name}(')
        end = source.index('\nW61_Status_t ', begin + 1)
        functions.append(source[begin:end])
    (tmp / 'w61_credentials_under_test.inc').write_text('\n'.join(functions))
    exe = tmp / 'w61_credentials.exe'
    subprocess.run([*common, '-I' + str(tmp), str(root / 'tests/test_w61_credentials.c'),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    exe = tmp / 'provisioning.exe'
    subprocess.run([*common, str(protocol), str(root / 'App/Src/app_provisioning.c'),
                    str(root / 'tests/test_provisioning.c'), '-o', str(exe)], check=True)
    for scenario in ('happy', 'security', 'timeout', 'overflow', 'oversize',
                     'scan_timeout', 'send_failure', 'stale_session', 'wifi_failure',
                     'rpa_pairing', 'mtu', 'drop_once'):
        subprocess.run([str(exe), scenario], check=True)
    # Compile the actual vendor functions without the unrelated HTTP/MQTT stack.
    source = (root / 'Middlewares/ST/ST67W6X_Network_Driver/Core/w6x_wifi.c').read_text()
    functions = []
    for name in ('W6X_WiFi_Connect', 'W6X_WiFi_Disconnect'):
        begin = source.index(f'W6X_Status_t {name}(')
        end = source.index('\nW6X_Status_t ', begin + 1)
        functions.append(source[begin:end])
    (tmp / 'w6x_connection_under_test.inc').write_text('\n'.join(functions))
    exe = tmp / 'w6x_connection.exe'
    subprocess.run([*common, '-I' + str(tmp), str(root / 'tests/test_w6x_connection.c'),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
