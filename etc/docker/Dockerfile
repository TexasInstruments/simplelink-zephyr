# This file is intended to be used only in TI's internal Continuous Integration (CI) system and will not function outside.
ARG ZEPHYR_SDK_VERSION="0.16.9"
FROM artifactory.itg.ti.com/docker-epd-cmcu-local/ubuntu-developer:jammy-20250224-0
LABEL TI_IMAGE_SOURCE="artifactory.itg.ti.com/docker-epd-cmcu-local/ubuntu-developer:jammy-20250224-0"

USER root

RUN sudo apt-get update
RUN wget https://apt.kitware.com/kitware-archive.sh
RUN sudo bash kitware-archive.sh

RUN DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt-get -y install tzdata

RUN sudo apt-get install -y --no-install-recommends cmake ninja-build gperf \
  ccache dfu-util device-tree-compiler wget usbutils \
  python3-dev python3-pip python3-tk python3-wheel xz-utils file \
  make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1 udev python3.10-venv

#Apt packages for compliance, guideline checks and docs
#coccinelle: .github/workflows/coding_guidelines.yml
#ninja-build, graphviz, lcov: .github/workflows/doc-build.yml
RUN apt-get update && apt-get install -y --no-install-recommends coccinelle graphviz lcov

# Required for some of the sample tests to run
# https://docs.zephyrproject.org/latest/samples/modules/nanopb/README.html
RUN sudo apt-get update && \
  sudo apt-get install -y protobuf-compiler

# Reduce image size
RUN sudo apt-get clean
RUN sudo rm -r /var/lib/apt/lists/*

# Install Python dependencies
COPY zephyr/etc/docker/requirements.txt ci_requirements.txt
COPY zephyr/scripts/ scripts
RUN pip3 --proxy wwwgate.ti.com install --no-cache-dir -r ./scripts/requirements.txt -r ci_requirements.txt

# Install ti-openocd from hardcoded URL to currently used version, to be manually bumped when needed
# Note: Any update to OpenOCD udev rules must be applied to the lab nodes as well.
#   Ref: https://bitbucket.itg.ti.com/projects/DEVOPS/repos/ansible-playbook-jenkins-agent/browse/roles/openocd/defaults/main.yml#5
ENV TI_OPENOCD_INSTALL_URL=http://vmtgpejenkins00.toro.design.ti.com:8080/view/build/job/build.openocd.all/58/artifact/bin/openocd_linux_1.4.1.58.zip
ENV TI_OPENOCD_INSTALL_DIR=/home/developer/ti_openocd
# ENV OPENOCD_BASE=${TI_OPENOCD_INSTALL_DIR}/openocd/bin
# ENV OPENOCD=${OPENOCD_BASE}/bin/openocd
# ENV OPENOCD_DEFAULT_PATH=${OPENOCD_BASE}/share/openocd/scripts
RUN wget ${TI_OPENOCD_INSTALL_URL}
RUN mkdir -p ${TI_OPENOCD_INSTALL_DIR}
RUN unzip $(basename ${TI_OPENOCD_INSTALL_URL}) -d ${TI_OPENOCD_INSTALL_DIR}
RUN chmod +x ${TI_OPENOCD_INSTALL_DIR}/openocd/bin/bin/openocd

# These rules need to be applied at the host, but no harm in applying them here too
RUN cp ${TI_OPENOCD_INSTALL_DIR}/openocd/bin/share/openocd/contrib/60-openocd.rules /etc/udev/rules.d/60-openocd.rules


# Install Zephyr SDK
ARG ZEPHYR_SDK_VERSION
ENV ZEPHYR_SDK_VERSION=${ZEPHYR_SDK_VERSION}
ENV ZEPHYR_SDK_INSTALL_DIR=/home/developer/zephyr-sdk-${ZEPHYR_SDK_VERSION}
RUN wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz --timeout 3600
RUN wget -O - "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/sha256.sum" | shasum --check --ignore-missing
RUN tar xf zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz


# Install ARM toolchain
WORKDIR ${ZEPHYR_SDK_INSTALL_DIR}
RUN wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/toolchain_linux-x86_64_arm-zephyr-eabi.tar.xz
RUN tar xf toolchain_linux-x86_64_arm-zephyr-eabi.tar.xz

ENV DOXYGEN_VERSION=1.9.6
ENV DOXYGEN_INSTALL_DIR=/home/developer/doxygen-${DOXYGEN_VERSION}

# Install Doxygen (.github/workflows/doc-build.yml)
RUN ["/bin/bash", "-c", "wget --no-verbose \"https://github.com/doxygen/doxygen/releases/download/Release_${DOXYGEN_VERSION//./_}/doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz\""]
RUN mkdir -p ${DOXYGEN_INSTALL_DIR}
RUN tar xf doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz -C /home/developer

# Groups needed to access devices
RUN usermod -aG plugdev developer
RUN usermod -aG dialout developer

WORKDIR /project

USER developer

ENV PATH="${DOXYGEN_INSTALL_DIR}/bin:${PATH}"
