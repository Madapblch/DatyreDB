# ==========================================
# STAGE 1: BUILDER (Сборка)
# ==========================================
FROM debian:bookworm-slim AS builder

ARG DEBIAN_FRONTEND=noninteractive

# Устанавливаем ВСЕ зависимости для сборки (включая Boost)
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    libfmt-dev \
    libspdlog-dev \
    nlohmann-json3-dev \
    libcurl4-openssl-dev \
    libboost-system-dev \
    libboost-thread-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Копируем исходный код
COPY . .

# Процесс сборки
# Используем -j$(nproc) для параллельной компиляции на всех ядрах
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local .. && \
    make -j$(nproc) && \
    make install

# ==========================================
# STAGE 2: RUNTIME (Запуск)
# ==========================================
FROM debian:bookworm-slim

ARG DEBIAN_FRONTEND=noninteractive

# Устанавливаем Runtime-библиотеки (те же, что и для сборки, но без компиляторов)
# Boost часто линкуется динамически, поэтому его библиотеки нужны и здесь!
RUN apt-get update && apt-get install -y --no-install-recommends \
    libfmt-dev \
    libspdlog-dev \
    nlohmann-json3-dev \
    libcurl4-openssl-dev \
    libboost-system-dev \
    libboost-thread-dev \
    netcat-openbsd \
    net-tools \
    && rm -rf /var/lib/apt/lists/*

# Создаем пользователя (Security Best Practice)
RUN groupadd -r datyre && useradd -r -g datyre datyre

# Рабочая директория
WORKDIR /var/lib/datyre
RUN chown datyre:datyre /var/lib/datyre

# Копируем готовый бинарник
COPY --from=builder /usr/local/bin/datyredb /usr/local/bin/datyredb

# Переключаемся на пользователя
USER datyre

EXPOSE 7432
VOLUME ["/var/lib/datyre"]

ENTRYPOINT ["datyredb"]
