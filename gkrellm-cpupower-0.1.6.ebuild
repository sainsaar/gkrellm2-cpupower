# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI="2"

inherit gkrellm-plugin

MY_P=${P/gkrellm/gkrellm2}

DESCRIPTION="A Gkrellm2 plugin for displaying and manipulating CPU frequency"
HOMEPAGE="https://github.com/sainsaar/gkrellm2-cpupower/"
SRC_URI="https://github.com/sainsaar/gkrellm2-cpupower/archive/${PV}.tar.gz -> ${MY_P}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE=""

S=${WORKDIR}/${MY_P}

# Collision with /usr/sbin/cpufreqnextgovernor
RDEPEND="sys-power/cpupower
		app-admin/sudo
		!x11-plugins/gkrellm-cpufreq
		"

PLUGIN_SO=cpupower.so

src_install() {
	gkrellm-plugin_src_install
	exeinto /usr/sbin
	doexe cpufreqnextgovernor
}

pkg_postinst() {
	echo
	einfo "Add these lines to /etc/sudoers.d/gkrellm2-cpupower to allow users to change cpu governor and speed:"
	einfo "%trusted ALL = (root) NOPASSWD: /usr/bin/cpupower -c [0-9]* frequency-set -g userspace"
	einfo "%trusted ALL = (root) NOPASSWD: /usr/bin/cpupower -c [0-9]* frequency-set -f [0-9]*"
	einfo "%trusted ALL = (root) NOPASSWD: /usr/sbin/cpufreqnextgovernor [0-9]*"
	echo
}
