#!/bin/bash

set -e

prepare_tarball() {
	export package=opendataplane

	pushd ${ROOT_DIR}
	./bootstrap
	./configure
	make dist

	if [[ -d ${ROOT_DIR}/.git ]]; then
		version=$(cat ${ROOT_DIR}/.scmversion)
	else
		echo "This script isn't expected to be used without"
		echo "a git repository."
		exit 1
	fi

	cp ${package}-${version}.tar.gz ${package}_${version}.orig.tar.gz
	tar xzf ${package}_${version}.orig.tar.gz
}
