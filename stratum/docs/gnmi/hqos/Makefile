# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# Build the Yang model python binding files

# set pyang plugin directory
PYBINDPLUGIN=`/usr/bin/env python -c \
    'import pyangbind; import os; print "%s/plugin" % os.path.dirname(pyangbind.__file__)'`

# OpenConfig yang model dir
OCDIR=OpenConfig/public/release/models
OCYANG=$(OCDIR)/qos/openconfig-qos-types.yang \
    $(OCDIR)/qos/openconfig-qos.yang

# Openconfig QoS Stratum yang model
STDIR=../../../public/yang
STYANG=$(OCDIR)/qos/openconfig-qos-types.yang \
    $(OCDIR)/qos/openconfig-qos-interfaces.yang \
    $(OCDIR)/qos/openconfig-qos.yang \
    $(STDIR)/openconfig-qos-stratum.yang

all: st_bindings.py

# Build openconfig qos stratum model
st_bindings.py: $(STYANG)
	pyang --plugindir $(PYBINDPLUGIN) -f pybind -p $(OCDIR):$(STDIR) -o st_bindings.py $(STYANG)

