# ==========================================
# STAGE 1: BUILDER
# ==========================================
FROM debian:bookworm-slim AS builder

ARG DEBIAN_FRONTEND=noninteractive

# Устанавливаем компиляторы и библиотеки
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    libfmt-dev \
    libspdlog-dev \
    nlohmann-json3-dev \
    libcurl4-openssl-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Копируем всё (кроме того, что в .dockerignore)
COPY . .

# Сборка и установка в /usr/local
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local .. && \
    make -j$(nproc) && \
    make install

# ==========================================
# STAGE 2: RUNTIME
# ==========================================
FROM debian:bookworm-slim

ARG DEBIAN_FRONTEND=noninteractive

# Устанавливаем только библиотеки, нужные для запуска
RUN apt-get update && apt-get install -y --no-install-recommends \
    libfmt-dev \
    libspdlog-dev \
    nlohmann-json3-dev \
    libcurl4-openssl-dev \
    netcat-openbsd \
    net-tools \
    && rm -rf /var/lib/apt/lists/*

# Создаем пользователя для безопасности
RUN groupadd -r datyre && useradd -r -g datyre datyre

# Рабочая директория
WORKDIR /var/lib/datyre
RUN chown datyre:datyre /var/lib/datyre

# Копируем готовый бинарник из первого этапа
COPY --from=builder /usr/local/bin/datyredb /usr/local/bin/datyredb

# Переключаемся на пользователя
USER datyre

EXPOSE 7432
VOLUME ["/var/lib/datyre"]

ENTRYPOINT ["datyredb"]
