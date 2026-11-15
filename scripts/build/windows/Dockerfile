# Native supplies the cross compiler and WinUI SDK. Fyodor's configure step
# also needs Git to check and apply its Native compatibility patch.
FROM native-windows-winui-sdk:24.04
RUN apt-get update && apt-get install -y --no-install-recommends git \
    && rm -rf /var/lib/apt/lists/*
