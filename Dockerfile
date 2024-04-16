from ubuntu:20.04

RUN apt-get update -y; \
    apt-get upgrade -y; \
    DEBIAN_FRONTEND="noninteractive" apt-get install -y tzdata; \
    ln -fs /usr/share/America/Chicago /etc/localtime ; \
    apt-get install -y \
        g++ \
        g++-multilib \
        libglib2.0-dev \
        libfdt-dev \
        libpixman-1-dev \
        make \
        wget \
        python \
        time \
        xz-utils \
        zlib1g-dev; \
    wget https://download.qemu.org/qemu-5.1.0.tar.xz; \
    tar xvf qemu-5.1.0.tar.xz; \
    (cd qemu-5.1.0; ./configure --target-list=i386-softmmu; make install)

RUN qemu-system-i386 --version



