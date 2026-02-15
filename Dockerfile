# ============================================================================
# Build Stage
# ============================================================================
FROM ubuntu:22.04 AS builder

# Metadata
LABEL maintainer="madapblch@github.com"
LABEL version="1.0.0"
LABEL description="DatyreDB Build Stage"

# Prevent interactive prompts during build
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    libcurl4-openssl-dev \
    libssl-dev \
    pkg-config \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /build

# Copy source code
COPY CMakeLists.txt ./
COPY src/ ./src/
COPY config/ ./config/

# Build the project
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc) && \
    strip /build/build/bin/DatyreDB

# ============================================================================
# Runtime Stage
# ============================================================================
FROM ubuntu:22.04

# Metadata
LABEL maintainer="madapblch@github.com"
LABEL version="1.0.0"
LABEL description="DatyreDB High-Performance Database with Telegram Bot"

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies only
RUN apt-get update && apt-get install -y --no-install-recommends \
    libcurl4 \
    libssl3 \
    ca-certificates \
    curl \
    procps \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get clean

# Create non-root user for security
RUN groupadd -r datyredb && \
    useradd -r -g datyredb -d /app -s /bin/bash datyredb

# Create necessary directories
RUN mkdir -p /app \
    /var/lib/datyredb \
    /var/log/datyredb \
    /app/backups \
    /app/config \
    && chown -R datyredb:datyredb /app /var/lib/datyredb /var/log/datyredb

# Copy binary from builder
COPY --from=builder --chown=datyredb:datyredb /build/build/bin/DatyreDB /usr/local/bin/DatyreDB

# Copy config files if they exist
COPY --chown=datyredb:datyredb config/*.conf /app/config/ 2>/dev/null || true

# Make binary executable
RUN chmod +x /usr/local/bin/DatyreDB

# Create healthcheck script
RUN echo '#!/bin/bash\n\
if pgrep -x "DatyreDB" > /dev/null; then\n\
    exit 0\n\
else\n\
    exit 1\n\
fi' > /usr/local/bin/healthcheck.sh && \
chmod +x /usr/local/bin/healthcheck.sh

# Switch to non-root user
USER datyredb
WORKDIR /app

# Environment variables (can be overridden)
ENV DATYREDB_DATA_DIR=/var/lib/datyredb \
    DATYREDB_LOG_DIR=/var/log/datyredb \
    DATYREDB_LOG_LEVEL=info \
    DATYREDB_PORT=5432 \
    DATYREDB_WORKERS=4 \
    DATYREDB_BUFFER_POOL_SIZE=1024

# Expose ports
EXPOSE 5432 9090

# Volume mount points
VOLUME ["/var/lib/datyredb", "/var/log/datyredb", "/app/backups"]

# Health check
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD /usr/local/bin/healthcheck.sh || exit 1

# Entry point
ENTRYPOINT ["/usr/local/bin/DatyreDB"]

# Default arguments (can be overridden)
CMD ["--data-dir", "/var/lib/datyredb", \
     "--log-file", "/var/log/datyredb/datyredb.log", \
     "--log-level", "info"]
