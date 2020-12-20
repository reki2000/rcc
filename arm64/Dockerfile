FROM debian:buster-slim

RUN set -eux; \
	apt-get update; \
	apt-get install -y --no-install-recommends \
		ca-certificates \
		gnupg dirmngr \
		wget \
		\
		gcc \
		libc6-dev \
		make \
		\
		libc6-dev-arm64-cross \
		\
		gcc-aarch64-linux-gnu \
		\
		file \
		qemu-user-static \
		qemu-system-arm \
	; \
	rm -rf /var/lib/apt/lists/*

CMD ["/bin/bash"]
