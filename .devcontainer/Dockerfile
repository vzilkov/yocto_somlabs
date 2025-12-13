FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates \
    ca-certificates build-essential chrpath cpio debianutils diffstat file gawk \
    gcc gcc-multilib git iputils-ping libacl1 liblz4-tool locales \
    python3 python3-git python3-jinja2 python3-pexpect python3-pip python3-subunit \
    socat texinfo unzip wget xz-utils zstd libsdl1.2-dev xterm sed cvs g++ \
    help2man make desktop-file-utils libgl1-mesa-dev libglu1-mesa-dev mercurial \
    autoconf automake groff curl lzop asciidoc u-boot-tools subversion \
    coreutils texi2html docbook-utils sudo openssh-client \
    && rm -rf /var/lib/apt/lists/* \
    && echo "en_US.UTF-8 UTF-8" > /etc/locale.gen \
    && locale-gen en_US.UTF-8

RUN wget https://storage.googleapis.com/git-repo-downloads/repo -O /usr/bin/repo \
    && chmod a+x /usr/bin/repo

ARG USERNAME=newuser
ARG USER_UID=1000
ARG USER_GID=$USER_UID
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers.d/$USERNAME

USER root
RUN chown -R ${USERNAME}:${USERNAME} /home/${USERNAME}
USER ${USERNAME}
WORKDIR /home/${USERNAME}

RUN mkdir -p imx-yocto-bsp

WORKDIR /home/${USERNAME}/imx-yocto-bsp
RUN repo init -u https://github.com/SoMLabs/imx-meta-somlabs -b walnascar -m imx-somlabs-6.12.34-2.1.0.xml || echo "Init completed (repo sync needed)"