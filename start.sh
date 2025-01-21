#!/bin/bash -e

. ./print.sh

clean=false
build=false
test=false

clean() {
    if $clean; then
        print "Removing containers"
        docker compose rm -fs
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

cassandra() {
    docker compose up -d migrate
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
    cassandra
    test
}

run_flags "$@"
run
