FROM ubuntu:22.04
RUN apt-get update && apt-get install -y --no-install-recommends \
      liburing-dev gcc libc6-dev \
    && rm -rf /var/lib/apt/lists/*
COPY read_secret.c /tmp/
RUN gcc -O2 -o /usr/local/bin/read_secret /tmp/read_secret.c -luring \
    && rm /tmp/read_secret.c
CMD ["sleep", "infinity"]
