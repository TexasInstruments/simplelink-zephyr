# This file is intended to be used only in TI's internal Continuous Integration (CI) system and will not function outside.

FROM artifactory.itg.ti.com/docker-public/zephyrprojectrtos/ci:v0.26-branch

# Create user, should be one that has permissions to modify the
# mounted folder (jenkins)
ARG UID
ARG GID
ARG UNAME
RUN groupadd -g ${GID} -o ${UNAME}
RUN useradd -m -u ${UID} -g ${GID} -o -s /bin/bash ${UNAME}

USER root

ENV http_proxy=webproxy.ext.ti.com:80
ENV https_proxy=webproxy.ext.ti.com:80

# Trick for making apt choose fast mirrors
RUN sed -i 's/htt[p|ps]:\/\/archive.ubuntu.com\/ubuntu\//mirror:\/\/mirrors.ubuntu.com\/mirrors.txt/g' /etc/apt/sources.list
# Set up apt with proxy
RUN echo -e "Acquire::http::proxy \"http://webproxy.ext.ti.com:80\"\; \n Acquire::https::proxy \"http://webproxy.ext.ti.com:80\"\;" > /etc/apt/apt.conf

# Apt packages for compliance, guideline checks and docs
# noccinelle: .github/workflows/coding_guidelines.yml
# ninja-build, graphviz, lcov: .github/workflows/doc-build.yml
RUN apt-get update && apt-get install -y --no-install-recommends coccinelle ninja-build graphviz lcov

WORKDIR /project

# From .github/workflows/doc-build.yml
ENV DOXYGEN_VERSION=1.9.6

COPY requirements.txt .

RUN pip3 --proxy wwwgate.ti.com install --no-cache-dir -r requirements.txt

# Install Doxygen (.github/workflows/doc-build.yml)
RUN wget --no-verbose "https://github.com/doxygen/doxygen/releases/download/Release_${DOXYGEN_VERSION//./_}/doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz"
RUN tar xf doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz -C /project

# Workaround to allow certain git operations inside the container, since the
# Docker user is different to the one in Jenkins (ZEPHYR-164)
RUN git config --global --add safe.directory /project; \
    git config --global --add safe.directory /project/zephyr

USER user

# Make sure we can find the zephyr SDK
ENV CMAKE_PREFIX_PATH="/opt/toolchains"
