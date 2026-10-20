# Root build entry: recurse into every target subproject.
#
#   make            # build all
#   make package    # build all + .deb
#   make clean
#   make install    # push to the device (needs THEOS_DEVICE_IP in the subproject)
#
# Each subproject is a standalone Theos project with its own control file,
# so it can also be built directly: make -C zhihu package

TARGETS := zhihu

.PHONY: all package clean install $(TARGETS)

all package clean install:
	@for t in $(TARGETS); do \
		echo "==> $$t: $@"; \
		$(MAKE) -C $$t $@ || exit 1; \
	done

$(TARGETS):
	@$(MAKE) -C $@ all
