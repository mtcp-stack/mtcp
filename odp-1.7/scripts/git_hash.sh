#!/bin/bash

if [ -z ${1} ]; then
	echo "should be called with a path"
	exit
fi
ROOTDIR=${1}

if [ -d ${ROOTDIR}/.git ]; then
	hash=$(git --git-dir=${ROOTDIR}/.git describe | tr -d "\n")
	if [[ $(git --git-dir=${ROOTDIR}/.git diff --shortstat 2> /dev/null \
		| tail -n1) != "" ]]; then
		dirty=.dirty
	fi

	echo -n "${hash}${dirty}">${ROOTDIR}/.scmversion

	sed -i "s|-|.git|" ${ROOTDIR}/.scmversion
	sed -i "s|-|.|g" ${ROOTDIR}/.scmversion
	sed -i "s|^v||g" ${ROOTDIR}/.scmversion
elif [ ! -d ${ROOTDIR}/.git -a ! -f ${ROOTDIR}/.scmversion ]; then
	echo -n "File ROOTDIR/.scmversion not found, "
	echo "and not inside a git repository"
	echo "Bailing out! Not recoverable!"
	exit 1
fi

cat ${ROOTDIR}/.scmversion
