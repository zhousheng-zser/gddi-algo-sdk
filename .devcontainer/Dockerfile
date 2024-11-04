FROM ubuntu:20.04

RUN sed -i 's/archive.ubuntu.com/mirrors.ustc.edu.cn/g' /etc/apt/sources.list

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && \
    apt install -y git python3 python3-pip pkgconf ninja-build ccache libssl-dev autopoint wget unzip && \
    rm -rf /var/lib/apt/lists/*

# CMake
RUN cd /tmp && \
    wget https://github.com/Kitware/CMake/releases/download/v3.24.0/cmake-3.24.0-linux-x86_64.sh && \
    chmod +x cmake-3.24.0-linux-x86_64.sh && \
    ./cmake-3.24.0-linux-x86_64.sh --prefix=/usr/local --exclude-subdir --skip-license && \
    rm -rf /tmp/*

# aarch64 交叉编译链
RUN apt update && \
    apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu gdb-multiarch && \
    rm -rf /var/lib/apt/lists/*

# BM1684(X) 交叉编译环境
RUN mkdir -p /usr/local/aarch64-bm1684-soc/include && mkdir -p /usr/local/aarch64-bm1684-soc/lib
RUN cd /tmp && \
    wget https://sophon-file.sophon.cn/sophon-prod-s3/drive/24/04/29/18/SDK-23.09_LTS_SP2.zip && \
    unzip SDK-23.09_LTS_SP2.zip && \
    dpkg -x 'SDK-23.09 LTS SP2/sophon-img_20240116_212937/bsp-debs/sophon-soc-libsophon_0.4.9-LTS_arm64.deb' . && \
    dpkg -x 'SDK-23.09 LTS SP2/sophon-img_20240116_212937/bsp-debs/sophon-soc-libsophon-dev_0.4.9-LTS_arm64.deb' . && \
    dpkg -x 'SDK-23.09 LTS SP2/sophon-mw_20240116_152830/sophon-mw-soc-sophon-ffmpeg_0.8.0_arm64.deb' . && \
    dpkg -x 'SDK-23.09 LTS SP2/sophon-mw_20240116_152830/sophon-mw-soc-sophon-ffmpeg-dev_0.8.0_arm64.deb' . && \
    dpkg -x 'SDK-23.09 LTS SP2/sophon-mw_20240116_152830/sophon-mw-soc-sophon-opencv_0.8.0_arm64.deb' . && \
    dpkg -x 'SDK-23.09 LTS SP2/sophon-mw_20240116_152830/sophon-mw-soc-sophon-opencv-dev_0.8.0_arm64.deb' . && \
    cp -r opt/sophon/libsophon-0.4.9/include/* /usr/local/aarch64-bm1684-soc/include && \
    cp -r opt/sophon/libsophon-0.4.9/lib/* /usr/local/aarch64-bm1684-soc/lib && \
    cp -r opt/sophon/sophon-ffmpeg_0.8.0/include/* /usr/local/aarch64-bm1684-soc/include && \
    cp -r opt/sophon/sophon-ffmpeg_0.8.0/lib/* /usr/local/aarch64-bm1684-soc/lib && \
    cp -r opt/sophon/sophon-opencv_0.8.0/include/* /usr/local/aarch64-bm1684-soc/include && \
    cp -r opt/sophon/sophon-opencv_0.8.0/lib/* /usr/local/aarch64-bm1684-soc/lib && \
    sed -i 's/\/usr\/local/\/usr\/local\/aarch64-bm1684-soc/g' /usr/local/aarch64-bm1684-soc/lib/pkgconfig/* && \
    rm -rf /tmp/*

# User && ZSH
RUN apt update && \
    apt install -y zsh && \
    rm -rf /var/lib/apt/lists/*

# Clear
RUN apt clean autoclean && apt autoremove --yes

# Oh My Zsh
RUN git clone https://github.com/ohmyzsh/ohmyzsh.git ~/.oh-my-zsh && \
    git clone https://github.com/zsh-users/zsh-autosuggestions ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-autosuggestions && \
    git clone https://github.com/zsh-users/zsh-syntax-highlighting.git ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-syntax-highlighting && \
    git clone https://github.com/zsh-users/zsh-history-substring-search ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-history-substring-search && \
    cp ~/.oh-my-zsh/templates/zshrc.zsh-template ~/.zshrc && \
    sed -i '/^#\ ZSH_CUSTOM/c ZSH_CUSTOM=~\/.oh-my-zsh\/custom' ~/.zshrc && \
    sed -i '/^plugins/c plugins=(git zsh-autosuggestions zsh-syntax-highlighting zsh-history-substring-search)' ~/.zshrc

WORKDIR /workspace

RUN ["/bin/zsh"]

# docker build -f .devcontainer/Dockerfile -t sdk-devel:1.0.0 .
