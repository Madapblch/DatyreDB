#!/bin/bash

# ============================================================================
# DatyreDB Docker Control Script
# ============================================================================

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
PROJECT_DIR="/root/projects/DatyreDB"
CONTAINER_NAME="datyredb"

# Functions
print_status() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

# Check if Docker is installed
check_docker() {
    if ! command -v docker &> /dev/null; then
        print_error "Docker is not installed"
        exit 1
    fi
    
    if ! command -v docker-compose &> /dev/null; then
        print_error "Docker Compose is not installed"
        exit 1
    fi
    
    print_status "Docker and Docker Compose are installed"
}

# Build image
build() {
    print_status "Building Docker image..."
    cd "$PROJECT_DIR"
    docker-compose build --no-cache
    print_status "Build complete"
}

# Start containers
start() {
    print_status "Starting containers..."
    cd "$PROJECT_DIR"
    docker-compose up -d
    sleep 5
    
    if docker ps | grep -q "$CONTAINER_NAME"; then
        print_status "Container started successfully"
    else
        print_error "Failed to start container"
        exit 1
    fi
}

# Stop containers
stop() {
    print_status "Stopping containers..."
    cd "$PROJECT_DIR"
    docker-compose stop
    print_status "Containers stopped"
}

# Restart containers
restart() {
    stop
    start
}

# Show logs
logs() {
    cd "$PROJECT_DIR"
    docker-compose logs -f --tail=100
}

# Show status
status() {
    echo "============================================"
    echo "          DatyreDB Docker Status"
    echo "============================================"
    echo ""
    
    # Container status
    if docker ps --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}" | grep -q "$CONTAINER_NAME"; then
        print_status "Container is running"
        echo ""
        docker ps --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}" | grep -E "(NAMES|$CONTAINER_NAME)"
    else
        print_error "Container is not running"
    fi
    echo ""
    
    # Health check
    HEALTH=$(docker inspect --format='{{.State.Health.Status}}' "$CONTAINER_NAME" 2>/dev/null || echo "unknown")
    if [ "$HEALTH" = "healthy" ]; then
        print_status "Health check: HEALTHY"
    elif [ "$HEALTH" = "unhealthy" ]; then
        print_error "Health check: UNHEALTHY"
    else
        print_warning "Health check: $HEALTH"
    fi
    echo ""
    
    # Resource usage
    echo "Resource Usage:"
    docker stats --no-stream --format "table {{.Container}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.NetIO}}" "$CONTAINER_NAME" 2>/dev/null || echo "N/A"
    echo ""
    
    # Volume information
    echo "Volumes:"
    docker volume ls | grep datyredb || echo "No volumes found"
    echo ""
    
    # Recent logs
    echo "Recent logs (last 5 lines):"
    docker logs --tail=5 "$CONTAINER_NAME" 2>/dev/null || echo "No logs available"
    echo ""
}

# Execute command in container
exec_cmd() {
    docker exec -it "$CONTAINER_NAME" "$@"
}

# Backup data
backup() {
    BACKUP_DIR="/root/backups/datyredb"
    TIMESTAMP=$(date +%Y%m%d_%H%M%S)
    BACKUP_FILE="${BACKUP_DIR}/backup_${TIMESTAMP}.tar.gz"
    
    mkdir -p "$BACKUP_DIR"
    
    print_status "Creating backup..."
    
    docker run --rm \
        -v datyredb_data:/data:ro \
        -v "$BACKUP_DIR":/backup \
        ubuntu:22.04 \
        tar czf "/backup/backup_${TIMESTAMP}.tar.gz" /data
    
    if [ -f "$BACKUP_FILE" ]; then
        print_status "Backup created: $BACKUP_FILE"
        ls -lh "$BACKUP_FILE"
    else
        print_error "Backup failed"
    fi
}

# Clean up
cleanup() {
    print_warning "This will remove containers, images, and volumes!"
    read -p "Are you sure? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        cd "$PROJECT_DIR"
        docker-compose down -v --rmi all
        print_status "Cleanup complete"
    else
        print_status "Cleanup cancelled"
    fi
}

# Health check
health() {
    echo "Performing health checks..."
    echo ""
    
    # 1. Container running
    if docker ps | grep -q "$CONTAINER_NAME"; then
        print_status "Container is running"
    else
        print_error "Container is not running"
        return 1
    fi
    
    # 2. Health status
    HEALTH=$(docker inspect --format='{{.State.Health.Status}}' "$CONTAINER_NAME" 2>/dev/null)
    if [ "$HEALTH" = "healthy" ]; then
        print_status "Health check passed"
    else
        print_error "Health check failed: $HEALTH"
    fi
    
    # 3. Process check
    if docker exec "$CONTAINER_NAME" pgrep -x DatyreDB > /dev/null; then
        print_status "DatyreDB process is running"
    else
        print_error "DatyreDB process is not running"
    fi
    
    # 4. Port check
    if nc -z localhost 5432 2>/dev/null; then
        print_status "Port 5432 is accessible"
    else
        print_warning "Port 5432 is not accessible"
    fi
    
    # 5. Telegram bot check
    if docker logs --tail=100 "$CONTAINER_NAME" 2>&1 | grep -q "Telegram bot started successfully"; then
        print_status "Telegram bot is connected"
    else
        print_warning "Telegram bot status unknown"
    fi
    
    echo ""
    echo "Health check complete"
}

# Show help
help() {
    echo "DatyreDB Docker Control Script"
    echo ""
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  build     - Build Docker image"
    echo "  start     - Start containers"
    echo "  stop      - Stop containers"
    echo "  restart   - Restart containers"
    echo "  status    - Show container status"
    echo "  logs      - Show logs (follow mode)"
    echo "  health    - Perform health checks"
    echo "  exec      - Execute command in container"
    echo "  backup    - Create data backup"
    echo "  cleanup   - Remove containers and volumes"
    echo "  help      - Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 start"
    echo "  $0 logs"
    echo "  $0 exec bash"
    echo "  $0 backup"
}

# Main
check_docker

case "$1" in
    build)
        build
        ;;
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart)
        restart
        ;;
    status)
        status
        ;;
    logs)
        logs
        ;;
    health)
        health
        ;;
    exec)
        shift
        exec_cmd "$@"
        ;;
    backup)
        backup
        ;;
    cleanup)
        cleanup
        ;;
    help|*)
        help
        ;;
esac
