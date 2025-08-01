# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t; fill-column: 100 -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_ExternalProject_ExternalProject,python3))

$(eval $(call gb_ExternalProject_use_externals,python3,\
	bzip2 \
	expat \
	$(if $(filter WNT LINUX,$(OS)),libffi) \
	openssl \
	zlib \
))

$(eval $(call gb_ExternalProject_register_targets,python3,\
	build \
	$(if $(filter MACOSX,$(OS)),\
		packages \
		fixscripts \
		fixinstallnames \
		executables \
		removeunnecessarystuff \
	) \
))

ifeq ($(OS),WNT)

# TODO: using Debug configuration and related mangling of pyconfig.h

# at least for MSVC 2008 it is necessary to clear MAKEFLAGS because
# nmake is invoked
$(call gb_ExternalProject_get_state_target,python3,build) :
	$(call gb_Trace_StartRange,python3,EXTERNAL)
	$(call gb_ExternalProject_run,build,\
		MAKEFLAGS= MSBuild.exe pcbuild.sln /t:Build $(gb_MSBUILD_CONFIG_AND_PLATFORM) \
			/p:bz2Dir=$(gb_UnpackedTarball_workdir)/bzip2 \
			/p:opensslIncludeDir=$(gb_UnpackedTarball_workdir)/openssl/include \
			/p:opensslOutDir=$(gb_UnpackedTarball_workdir)/openssl \
			/p:zlibDir=$(gb_UnpackedTarball_workdir)/zlib \
			/p:libffiOutDir=$(gb_UnpackedTarball_workdir)/libffi/$(HOST_PLATFORM)/.libs \
			/p:libffiIncludeDir=$(gb_UnpackedTarball_workdir)/libffi/$(HOST_PLATFORM)/include \
			/maxcpucount \
			/p:PlatformToolset=$(VCTOOLSET) /p:VisualStudioVersion=$(VCVER) /ToolsVersion:Current \
			$(if $(filter 10,$(WINDOWS_SDK_VERSION)),/p:WindowsTargetPlatformVersion=$(UCRTVERSION)) \
		$(foreach i,$(python3_EXTENSION_MODULES), \
			&& { test -e ../$i || { printf 'Error: missing %s\n' $i; false; } }) \
	,PCBuild)
	$(call gb_Trace_EndRange,python3,EXTERNAL)

else

# --with-system-expat: this should find the one in the workdir (or system)

# OPENSSL_INCLUDES OPENSSL_LDFLAGS OPENSSL_LIBS cannot be set via commandline!
# use --with-openssl instead, which requires include/ and lib/ subdirs.

# libffi is not all that stable, with 3 different SONAMEs currently, so we
# have to bundle it; --without-system-ffi does not work any more on Linux.
# Unfortunately (as of 3.7) pkg-config is used to locate libffi so we do some
# hacks to find the libffi.pc in workdir by overriding PKG_CONFIG_LIBDIR.
# Also, pkg-config is only used to find the headers, the libdir needs to be
# passed extra.

# create a symlink "LO_lib" because the .so are in a directory with platform
# specific name like build/lib.linux-x86_64-3.3

python3_cflags = $(ZLIB_CFLAGS)
ifneq (,$(ENABLE_VALGRIND))
    python3_cflags += $(VALGRIND_CFLAGS)
endif

# This happens to override the -O3 in the default OPT set in
# workdir/UnpackedTarball/python3/configure.ac while keeping the other content of that OPT intact:
ifeq ($(ENABLE_OPTIMIZED),)
python3_cflags += $(gb_COMPILERNOOPTFLAGS)
endif

$(call gb_ExternalProject_get_state_target,python3,build) :
	$(call gb_Trace_StartRange,python3,EXTERNAL)
	$(call gb_ExternalProject_run,build,\
		$(if $(filter MACOSX,$(OS)), \
			$(if $(filter 10.8 10.9 10.10 10.11,$(MACOSX_DEPLOYMENT_TARGET)), \
				ac_cv_func_getentropy=no \
				ac_cv_func_clock_gettime=no \
			) \
		) \
		$(gb_RUN_CONFIGURE) ./configure \
		$(gb_CONFIGURE_PLATFORMS) \
		$(if $(ENABLE_VALGRIND),--with-valgrind) \
		$(if $(ENABLE_DBGUTIL),--with-pydebug) \
		--prefix=/python-inst \
		--with-system-expat \
		$(if $(filter MACOSX,$(OS)), \
			$(if $(filter INTEL,$(CPUNAME)),--enable-universalsdk=$(MACOSX_SDK_PATH) \
                                --with-universal-archs=intel \
            ) \
			--enable-framework=/@__________________________________________________OOO --with-framework-name=LibreOfficePython, \
			--enable-shared \
		) \
		$(if $(ENABLE_OPENSSL),$(if $(SYSTEM_OPENSSL),,\
			--with-openssl=$(gb_UnpackedTarball_workdir)/openssl \
		) ) \
		$(if $(filter LINUX,$(OS)), \
			PKG_CONFIG_LIBDIR="$(gb_UnpackedTarball_workdir)/libffi/$(HOST_PLATFORM)$${PKG_CONFIG_LIBDIR:+:$$PKG_CONFIG_LIBDIR}" \
		) \
		CC="$(strip $(CC) \
			$(if $(filter -fsanitize=undefined,$(CC)),-fno-sanitize=function) \
			$(if $(SYSTEM_BZIP2),,-I$(gb_UnpackedTarball_workdir)/bzip2) \
			$(if $(SYSTEM_EXPAT),,-I$(gb_UnpackedTarball_workdir)/expat/lib) \
			$(if $(SYSBASE), -I$(SYSBASE)/usr/include) \
			)" \
		$(if $(python3_cflags),CFLAGS='$(python3_cflags)') \
		$(if $(filter -fsanitize=%,$(CC)),LINKCC="$(CXX) -pthread") \
		LDFLAGS="$(strip $(call gb_ExternalProject_get_link_flags,python3) \
			$(if $(filter LINUX,$(OS)),-L$(gb_UnpackedTarball_workdir)/libffi/$(HOST_PLATFORM)/.libs) \
			$(if $(SYSTEM_BZIP2),,-L$(gb_UnpackedTarball_workdir)/bzip2) \
			$(if $(SYSTEM_EXPAT),,-L$(gb_StaticLibrary_WORKDIR)) \
			$(if $(SYSTEM_ZLIB),,-L$(gb_StaticLibrary_WORKDIR)) \
			$(if $(SYSBASE), -L$(SYSBASE)/usr/lib) \
			$(gb_LTOFLAGS) \
			)" \
		&& MAKEFLAGS= $(MAKE) \
			$(if $(filter MACOSX,$(OS)),DESTDIR=$(EXTERNAL_WORKDIR)/python-inst install) \
			$(if $(SYSTEM_ZLIB),,ZLIB_INCDIR=$(WORKDIR)/UnpackedTarball/zlib) \
		&& ln -s build/lib.* LO_lib \
		$(foreach i,$(python3_EXTENSION_MODULES), \
			&& { test -e $i || { printf 'Error: missing %s\n' $i; false; } }) \
	)
	$(call gb_Trace_EndRange,python3,EXTERNAL)

endif

# If you want to run Python's own unit tests, add this to the chain of commands above:
#		&& MAKEFLAGS= $(MAKE) \
#			$(if $(filter MACOSX,$(OS)),DESTDIR=$(EXTERNAL_WORKDIR)/python-inst) \
#			$(if $(SYSTEM_ZLIB),,ZLIB_INCDIR=$(WORKDIR)/UnpackedTarball/zlib) \
#			test \

ifeq ($(OS),MACOSX)

python3_fw_prefix:=$(gb_UnpackedTarball_workdir)/python3/python-inst/@__________________________________________________OOO/LibreOfficePython.framework/Versions/$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)
python3_EXTENSION_MODULE_SUFFIX:=cpython-$(PYTHON_VERSION_MAJOR)$(PYTHON_VERSION_MINOR)$(if $(ENABLE_DBGUTIL),d)-darwin

# Since python 3.12 setuptools and pip are not available by default
$(call gb_ExternalProject_get_state_target,python3,packages) : $(call gb_ExternalProject_get_state_target,python3,build)
	cp -r $(gb_UnpackedTarball_workdir)/python3/Lib/setuptools $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/site-packages/
	cp -r $(gb_UnpackedTarball_workdir)/python3/Lib/_distutils_hack $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/site-packages/
	cp -r $(gb_UnpackedTarball_workdir)/python3/Lib/pip $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/site-packages/

# rule to allow relocating the whole framework, removing reference to buildinstallation directory
# also scripts are not allowed to be signed as executables (with extended attributes), but need to
# be treated as data/put into Resources folder, see also
# https://developer.apple.com/library/archive/technotes/tn2206/_index.html
$(call gb_ExternalProject_get_state_target,python3,fixscripts) : $(call gb_ExternalProject_get_state_target,python3,build)
	$(call gb_Output_announce,python3 - remove reference to installroot from scripts,build,CUS,5)
	$(COMMAND_ECHO)cd $(python3_fw_prefix)/bin/ && \
	for file in \
		2to3-$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR) \
		idle$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR) \
		pip$(PYTHON_VERSION_MAJOR) \
		pip$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR) \
		pydoc$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR) \
		python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)-config \
	; do { rm "$$file" && $(gb_AWK) '\
		BEGIN {print "#!/usr/bin/env bash\n\
origpath=$$(pwd)\n\
bindir=$$(cd $$(dirname \"$$0\") ; pwd)\n\
cd \"$$origpath\"\n\
\"$$bindir/../Resources/Python.app/Contents/MacOS/LibreOfficePython\" - $$@ <<EOF"} \
		FNR==1{next} \
		      {print} \
		END   {print "EOF"}' > "../Resources/$$file" ; } < "$$file" && \
	chmod +x "../Resources/$$file" && ln -s "../Resources/$$file" ; done
	touch $@

$(call gb_ExternalProject_get_state_target,python3,fixinstallnames) : $(call gb_ExternalProject_get_state_target,python3,build) \
        | $(call gb_ExternalProject_get_state_target,python3,removeunnecessarystuff)
	$(INSTALL_NAME_TOOL) -change \
		$(python3_fw_prefix)/LibreOfficePython \
		@executable_path/../../../../LibreOfficePython \
		$(python3_fw_prefix)/Resources/Python.app/Contents/MacOS/LibreOfficePython
	for file in $(shell $(FIND) $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/lib-dynload -name "*.so") ; do \
	$(INSTALL_NAME_TOOL) -change \
		$(python3_fw_prefix)/LibreOfficePython \
		@loader_path/../../../LibreOfficePython $$file ; done
	touch $@

$(call gb_ExternalProject_get_state_target,python3,executables) : $(call gb_ExternalProject_get_state_target,python3,build)
	cd $(python3_fw_prefix)/bin ; \
	$(INSTALL_NAME_TOOL) -change \
		$(python3_fw_prefix)/LibreOfficePython \
		@executable_path/../LibreOfficePython python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)
	touch $@

# Remove modules (both Python and binary bits) of questionable usefulness that we don't ship on
# other platforms either. See the "packages not shipped" comment in ExternalPackage_python3.mk.

$(call gb_ExternalProject_get_state_target,python3,removeunnecessarystuff) : $(call gb_ExternalProject_get_state_target,python3,build)
	$(call gb_Output_announce,python3 - remove the stuff we don't need to ship,build,CUS,5)
	rm -r $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/dbm
	rm -r $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/sqlite3
	rm -r $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/curses
	rm -r $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/idlelib
	rm -r $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/tkinter
	rm -r $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/turtledemo
	rm -r $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/test
	rm -r $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/venv
	# Then the binary libraries
	rm $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/lib-dynload/_dbm.$(python3_EXTENSION_MODULE_SUFFIX).so
	rm $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/lib-dynload/_sqlite3.$(python3_EXTENSION_MODULE_SUFFIX).so
	rm $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/lib-dynload/_curses.$(python3_EXTENSION_MODULE_SUFFIX).so
	rm $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/lib-dynload/_curses_panel.$(python3_EXTENSION_MODULE_SUFFIX).so
	rm $(python3_fw_prefix)/lib/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/lib-dynload/_test*.$(python3_EXTENSION_MODULE_SUFFIX).so
	touch $@

endif

# vim: set noet sw=4 ts=4:
