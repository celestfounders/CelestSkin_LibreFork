# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0 (the "License"). If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Deliver private backend runtime files into instdir program/

$(eval $(call gb_Package_Package,private_backend_runtime,$(SRCDIR)))

# Auto-start manager script
$(eval $(call gb_Package_add_file,private_backend_runtime,program/private_backend/autostart_lo_python.py,private_backend/autostart_lo_python.py))

# AI server worker (single-file HTTP server)
$(eval $(call gb_Package_add_file,private_backend_runtime,program/private_backend/source/python_service/ai_server_worker.py,private_backend/source/python_service/ai_server_worker.py))

# vim: set noet sw=4 ts=4:
