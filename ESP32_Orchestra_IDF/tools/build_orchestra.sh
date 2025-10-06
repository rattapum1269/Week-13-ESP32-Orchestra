#!/bin/bash
# ESP32 Orchestra Build and Flash Script
# สำหรับ build และ flash ESP32 Orchestra ด้วย ESP-IDF

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if ESP-IDF is available
check_idf() {
    if ! command -v idf.py &> /dev/null; then
        print_error "ESP-IDF not found! Please install and source ESP-IDF environment."
        echo "Run: . \$IDF_PATH/export.sh"
        exit 1
    fi
    print_success "ESP-IDF found: $(idf.py --version)"
}

# Build project
build_project() {
    local project_dir=$1
    local project_name=$2
    
    print_status "Building $project_name..."
    cd "$project_dir"
    
    # Set target (only needed once)
    idf.py set-target esp32
    
    # Build
    if idf.py build; then
        print_success "$project_name built successfully"
    else
        print_error "Failed to build $project_name"
        exit 1
    fi
    
    cd - > /dev/null
}

# Flash project
flash_project() {
    local project_dir=$1
    local project_name=$2
    local port=$3
    
    print_status "Flashing $project_name to $port..."
    cd "$project_dir"
    
    if idf.py -p "$port" flash; then
        print_success "$project_name flashed successfully to $port"
    else
        print_error "Failed to flash $project_name to $port"
        print_warning "Make sure the device is connected and the port is correct"
        exit 1
    fi
    
    cd - > /dev/null
}

# Monitor project
monitor_project() {
    local project_dir=$1
    local project_name=$2
    local port=$3
    
    print_status "Starting monitor for $project_name on $port..."
    print_warning "Press Ctrl+] to exit monitor"
    cd "$project_dir"
    idf.py -p "$port" monitor
}

# Update MUSICIAN_ID
update_musician_id() {
    local musician_id=$1
    local file="musician/main/musician_main.c"
    
    if [ ! -f "$file" ]; then
        print_error "Musician source file not found: $file"
        exit 1
    fi
    
    print_status "Updating MUSICIAN_ID to $musician_id..."
    
    # Backup original file
    cp "$file" "$file.backup"
    
    # Update MUSICIAN_ID
    sed -i "s/#define MUSICIAN_ID.*/#define MUSICIAN_ID $musician_id  \/\/ 0=Part A, 1=Part B, 2=Part C, 3=Part D/" "$file"
    
    if grep -q "#define MUSICIAN_ID $musician_id" "$file"; then
        print_success "MUSICIAN_ID updated to $musician_id"
    else
        print_error "Failed to update MUSICIAN_ID"
        exit 1
    fi
}

# Clean project
clean_project() {
    local project_dir=$1
    local project_name=$2
    
    print_status "Cleaning $project_name..."
    cd "$project_dir"
    idf.py fullclean
    print_success "$project_name cleaned"
    cd - > /dev/null
}

# Main script
main() {
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local root_dir="$(dirname "$script_dir")"
    
    cd "$root_dir"
    
    print_status "ESP32 Orchestra Build Script"
    print_status "Working directory: $(pwd)"
    
    # Check ESP-IDF
    check_idf
    
    case "$1" in
        "build-all")
            print_status "Building all projects..."
            build_project "conductor" "Conductor"
            build_project "musician" "Musician"
            print_success "All projects built successfully!"
            ;;
            
        "build-conductor")
            build_project "conductor" "Conductor"
            ;;
            
        "build-musician")
            if [ -n "$2" ]; then
                update_musician_id "$2"
            fi
            build_project "musician" "Musician"
            ;;
            
        "flash-conductor")
            if [ -z "$2" ]; then
                print_error "Please specify port: $0 flash-conductor /dev/ttyUSB0"
                exit 1
            fi
            build_project "conductor" "Conductor"
            flash_project "conductor" "Conductor" "$2"
            ;;
            
        "flash-musician")
            if [ -z "$2" ] || [ -z "$3" ]; then
                print_error "Usage: $0 flash-musician <musician_id> <port>"
                print_error "Example: $0 flash-musician 0 /dev/ttyUSB0"
                exit 1
            fi
            update_musician_id "$2"
            build_project "musician" "Musician"
            flash_project "musician" "Musician" "$3"
            ;;
            
        "monitor")
            if [ -z "$2" ] || [ -z "$3" ]; then
                print_error "Usage: $0 monitor <conductor|musician> <port>"
                print_error "Example: $0 monitor conductor /dev/ttyUSB0"
                exit 1
            fi
            monitor_project "$2" "$2" "$3"
            ;;
            
        "clean-all")
            clean_project "conductor" "Conductor"
            clean_project "musician" "Musician"
            print_success "All projects cleaned!"
            ;;
            
        "setup-musicians")
            if [ "$#" -lt 4 ]; then
                print_error "Usage: $0 setup-musicians <port1> <port2> <port3> [port4]"
                print_error "Example: $0 setup-musicians /dev/ttyUSB0 /dev/ttyUSB1 /dev/ttyUSB2"
                exit 1
            fi
            
            print_status "Setting up multiple musicians..."
            
            # Flash musician 0
            update_musician_id 0
            build_project "musician" "Musician"
            flash_project "musician" "Musician" "$2"
            
            # Flash musician 1
            update_musician_id 1
            build_project "musician" "Musician"
            flash_project "musician" "Musician" "$3"
            
            # Flash musician 2
            update_musician_id 2
            build_project "musician" "Musician"
            flash_project "musician" "Musician" "$4"
            
            # Flash musician 3 if port provided
            if [ -n "$5" ]; then
                update_musician_id 3
                build_project "musician" "Musician"
                flash_project "musician" "Musician" "$5"
            fi
            
            print_success "All musicians set up successfully!"
            print_status "Now flash the conductor with: $0 flash-conductor <port>"
            ;;
            
        "full-setup")
            if [ "$#" -lt 5 ]; then
                print_error "Usage: $0 full-setup <conductor_port> <musician0_port> <musician1_port> <musician2_port> [musician3_port]"
                exit 1
            fi
            
            print_status "Full Orchestra Setup..."
            
            # Setup musicians
            "$0" setup-musicians "${@:2}"
            
            # Setup conductor
            build_project "conductor" "Conductor"
            flash_project "conductor" "Conductor" "$2"
            
            print_success "🎵 Full Orchestra setup complete!"
            print_status "All devices are ready. Start the conductor by pressing the BOOT button!"
            ;;
            
        "help"|"")
            echo "ESP32 Orchestra Build Script"
            echo ""
            echo "Usage: $0 <command> [arguments]"
            echo ""
            echo "Commands:"
            echo "  build-all                     - Build conductor and musician"
            echo "  build-conductor              - Build conductor only"
            echo "  build-musician [id]          - Build musician (optionally set ID)"
            echo "  flash-conductor <port>       - Build and flash conductor"
            echo "  flash-musician <id> <port>   - Build and flash musician with specific ID"
            echo "  monitor <project> <port>     - Monitor project logs"
            echo "  clean-all                    - Clean all projects"
            echo "  setup-musicians <ports...>   - Setup multiple musicians with different IDs"
            echo "  full-setup <ports...>        - Complete orchestra setup"
            echo "  help                         - Show this help"
            echo ""
            echo "Examples:"
            echo "  $0 build-all"
            echo "  $0 flash-conductor /dev/ttyUSB0"
            echo "  $0 flash-musician 0 /dev/ttyUSB1"
            echo "  $0 setup-musicians /dev/ttyUSB1 /dev/ttyUSB2 /dev/ttyUSB3"
            echo "  $0 full-setup /dev/ttyUSB0 /dev/ttyUSB1 /dev/ttyUSB2 /dev/ttyUSB3"
            echo "  $0 monitor conductor /dev/ttyUSB0"
            ;;
            
        *)
            print_error "Unknown command: $1"
            print_status "Run '$0 help' for available commands"
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"