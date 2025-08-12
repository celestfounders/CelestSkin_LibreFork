# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_Package_Package,private_backend_services,$(SRCDIR)))

# Install Jobs.xcu configuration for auto-startup
$(eval $(call gb_Package_add_file,private_backend_services,share/registry/data/org/openoffice/Office/Jobs.xcu,private_backend/Jobs.xcu))

# Python files are handled by Pyuno_private_backend.mk

# vim: set noet sw=4 ts=4: 