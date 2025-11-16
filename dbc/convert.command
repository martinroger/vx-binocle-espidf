#!/bin/bash
#Convert DBC files to C source files
# Get the directory where the script is located
SCRIPT_DIR=$(dirname "$0")

# Change the command to use the script's directory and the filename
cantools generate_c_source --use-float "$SCRIPT_DIR/binocan.dbc";
 
exit