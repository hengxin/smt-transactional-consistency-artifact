# 使用 Ubuntu 22.04 作为基础镜像
FROM ubuntu:22.04

# 设置非交互模式，以免在构建时遇到交互提示
ENV DEBIAN_FRONTEND=noninteractive

# 更新包列表，并安装常用工具
RUN apt-get update && apt-get install -y \
  software-properties-common \
  wget \
  curl \
  git \
  vim \
  python3 \
  python3-pip

# 安装 g++-12 和 meson 依赖
RUN apt-get update && \
  apt-get install -y \
  g++-12 \
  pkg-config \
  cmake \
  meson \
  ninja-build \
  libboost-log-dev \
  libboost-test-dev \
  libboost-graph-dev \
  libz-dev \
  libgmp-dev \
  libfmt-dev && \
  apt-get clean && \
  rm -rf /var/lib/apt/lists/*

# 设置 g++-12 为默认的 g++
RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100 && \
  update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100

RUN pip3 install --no-cache-dir \
  humanize \
  psutil \
  rich \
  pandas \
  pytest-shutil \
  prettytable

# 设置工作目录
WORKDIR /smt-transactional-consistency-artifact

# 将项目文件拷贝到容器中（请将 . 替换为项目文件夹路径）
COPY . /smt-transactional-consistency-artifact

# 默认执行 bash，用户可以进入容器后继续操作
CMD ["/bin/bash"]
