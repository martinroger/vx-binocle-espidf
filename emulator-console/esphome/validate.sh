#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

VENV_DIR="${SCRIPT_DIR}/.venv"
START_TIME=$(date +%s)

log_step() {
    local timestamp
    timestamp=$(date +"%H:%M:%S")
    echo ""
    echo "=========================================================="
    echo "  [${timestamp}] $1"
    echo "=========================================================="
}

log_info() {
    local timestamp
    timestamp=$(date +"%H:%M:%S")
    echo "  [${timestamp}] [INFO] $1"
}

log_success() {
    local timestamp
    timestamp=$(date +"%H:%M:%S")
    echo "  [${timestamp}] [✓] $1"
}

log_step "Binocle Vehicle Emulator - ESPHome Validation Runner"

# 1. Check Python Virtual Environment
log_info "Verifying Python virtual environment..."
if [ ! -d "${VENV_DIR}" ]; then
    log_info "Creating new virtual environment at ${VENV_DIR}..."
    python3 -m venv "${VENV_DIR}"
    log_success "Virtual environment created."
else
    log_success "Existing virtual environment found."
fi

# Activate virtualenv
source "${VENV_DIR}/bin/activate"

# 2. Check ESPHome Installation
log_info "Checking ESPHome installation in virtualenv..."
if ! command -v esphome &> /dev/null; then
    log_info "Installing ESPHome..."
    pip install --upgrade pip
    pip install esphome
    log_success "ESPHome installed."
else
    ESPHOME_VER=$(esphome version | head -n 1)
    log_success "ESPHome is available: ${ESPHOME_VER}"
fi

# 3. Check Secrets
if [ ! -f "secrets.yaml" ]; then
    log_info "Copying secrets.yaml.example -> secrets.yaml..."
    cp secrets.yaml.example secrets.yaml
    log_success "secrets.yaml ready."
fi

# 4. Validate Configuration Schema & Syntax
log_step "STEP 1/2: Validating YAML Schema & Syntax (esphome config)"
esphome config binocle-emulator.yaml
log_success "Configuration schema and pin substitutions validated successfully!"

# 5. Compile Firmware via ESP-IDF Toolchain
log_step "STEP 2/2: Compiling Firmware via ESP-IDF Toolchain (esphome compile)"
esphome compile binocle-emulator.yaml
log_success "ESP-IDF compilation completed successfully!"

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

log_step "Validation Summary"
echo "  Target:        ESP32-S3 (ESP-IDF 5.5.5)"
echo "  Configuration: ${SCRIPT_DIR}/binocle-emulator.yaml"
echo "  Status:        ALL CHECKS PASSED (100% OK)"
echo "  Total Time:    ${ELAPSED}s"
echo "=========================================================="
