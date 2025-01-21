#!/bin/bash -e

. ./print.sh

clean=false
build=false
test=false

clean() {
    if $clean; then
        print "Removing containers"
        docker compose rm -sf
    fi
}

build() {
    if $build; then
        print "Building containers"
        docker compose build
    fi
}

test() {
    if $test; then
        print "Running tests"
        docker compose run driver busted --lua=luajit
    fi
}

start() {
    print "Starting containers"
    docker compose up --exit-code-from=migrate --attach=migrate
    docker compose up --wait scylla
}

run_flags() {
    while getopts 'cbt' option; do
        case $option in
        c) clean=true ;;
        b) build=true ;;
        t) test=true ;;
        ?) exit 1 ;;
        esac
    done
}

run() {
    clean
    build
    start
    test
}

run_flags "$@"
run
