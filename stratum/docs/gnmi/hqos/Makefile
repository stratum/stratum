# Build the Yang model python binding files

# set pyang plugin directory
PYBINDPLUGIN=`/usr/bin/env python -c \
    'import pyangbind; import os; print "%s/plugin" % os.path.dirname(pyangbind.__file__)'`

# IETF yang model dir
IETFDIR=ietf/yang/standard/ietf
IETFYANG=$(IETFDIR)/RFC/ietf-interfaces.yang

# BBF Yang models
BBFDIR=BroadbandForum/yang/standard
BBFYANG=$(BBFDIR)/networking/bbf-qos-traffic-mngt.yang \
    $(BBFDIR)/networking/bbf-qos-enhanced-scheduling.yang

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

all: bbf_bindings.py oc_bindings.py st_bindings.py

# Build bbf-qos-traffic-management.yang
bbf_bindings.py: $(BBFYANG) $(IETFYANG)
	pyang --plugindir $(PYBINDPLUGIN) -f pybind -p $(BBFDIR):$(IETFDIR) -o bbf_bindings.py $(BBFYANG) $(IETFYANG)

# Build openconfig qos model
oc_bindings.py: $(OCYANG)
	pyang --plugindir $(PYBINDPLUGIN) -f pybind -p $(OCDIR) -o oc_bindings.py $(OCYANG)

# Build openconfig qos stratum model
st_bindings.py: $(STYANG)
	pyang --plugindir $(PYBINDPLUGIN) -f pybind -p $(OCDIR):$(STDIR) -o st_bindings.py $(STYANG)

