# Justfile (https://just.systems/) for building healthcheck-helpers

version := "1.0"
dist_dir := justfile_directory() / "dist"

rpmbuild_dir := dist_dir / "rpmbuild"
rpmbuild_src_dir := rpmbuild_dir / "SOURCES"
rpm_tgz := "healthchecks-helpers-" + version + ".tar.gz"

debbuild_dir := dist_dir / "debbuild"
debbuild_src_dir := debbuild_dir / "src"

docker_run_args := "--rm -v '" + justfile_directory() + ":/mnt' -w /mnt -u " + `id -u` + ":" + `id -g`
dockerfile_rpm := '''
FROM fedora:44
RUN dnf install -y cmake gcc groff-base just libcurl-devel ninja-build rpmdevtools systemd-devel
'''
dockerfile_deb := '''
FROM debian:trixie-slim
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get -yqqU install cmake debhelper groff-base just libcurl4-openssl-dev libsystemd-dev pkg-config
'''

build:
    @[ -d build ] || '{{ just_executable() }}' configure
    exec cmake --build build

configure type="Release":
    cmake -S . -B build -DCMAKE_BUILD_TYPE='{{ type }}'

install prefix="/usr/local": build
    exec cmake --install build --strip --prefix '{{ prefix }}'

# Check the version numbers are aligned
version-check:
    grep -qF 'project(healthcheck-helpers VERSION {{ version }} LANGUAGES C)' CMakeLists.txt
    grep -qF 'Version:        {{ version }}' healthchecks-helpers.spec
    grep -qF 'healthchecks-helpers ({{ version }})' debian/changelog

tag-check tag:
    test '{{ tag }}' = 'v{{ version }}'

lint:
    ! groff -man -ww -Tutf8 docs/healthchecks-systemd-oneshot.1 2>&1 >/dev/null | grep -q .

build-rpm: version-check lint
    rm -rf '{{ rpmbuild_dir }}'
    mkdir -p '{{ rpmbuild_src_dir }}'
    tar --exclude=build --exclude=dist \
        --transform 's,^\.,healthchecks-helpers-{{ version }},' \
        -czf '{{ rpmbuild_src_dir / rpm_tgz }}' .
    rpmbuild -bb --define '_topdir {{ rpmbuild_dir }}' healthchecks-helpers.spec
    find '{{ rpmbuild_dir / "RPMS" }}' -name 'healthchecks-helpers-{{ version }}*.rpm' -execdir cp -v '{}' '{{ dist_dir }}' \;

build-deb: version-check lint
    rm -rf '{{ debbuild_dir }}'
    mkdir -p '{{ debbuild_src_dir }}'
    cp -av debian CMakeLists.txt LICENSE docs *.c '{{ debbuild_src_dir }}/'
    cd '{{ debbuild_src_dir }}' && dpkg-buildpackage -us -uc -b
    find '{{ debbuild_dir }}' -maxdepth 1 -name 'healthchecks-helpers_{{ version }}*.deb' -execdir cp -v '{}' '{{ dist_dir }}' \;

docker-build-rpm:
    echo '{{ dockerfile_rpm }}' | docker build -t healthchecks-helpers-build-rpm -
    docker run {{ docker_run_args }} healthchecks-helpers-build-rpm just build-rpm

docker-build-deb:
    echo '{{ dockerfile_deb }}' | docker build -t healthchecks-helpers-build-deb -
    docker run {{ docker_run_args }} healthchecks-helpers-build-deb just build-deb

clean:
    -rm -rf build dist
