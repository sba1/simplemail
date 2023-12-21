#
# Defines some functions to support unity kind of builds, i.e., builds in which individual files
# are placed into a single file.
#
# Needs TARGET_DIR to point to a writable directory.
#

# Defines the number of files per unity
NUM_FILES_PER_UNITY := 5

# This macro will be created when calling and evaluating
# subsequently the unity function
UNITIES :=

# Populates from $1 the UNITIES macro and the dependencie of the unities
define unity

$$(TARGET_DIR)/unities/unity$$(words $$(UNITIES)).c: $1

# Accumulate files belonging to the current unity
acc := $$(if $$(filter $$(words $1 $$(acc)), $(NUM_FILES_PER_UNITY)), $1, $$(acc) $1)

# Add a new unity after each 4 files
UNITIES := $$(if $$(filter $$(words $1 $$(acc)), $(NUM_FILES_PER_UNITY)), $$(UNITIES) $$(TARGET_DIR)/unities/unity$$(words $$(UNITIES)).c, $$(UNITIES))

endef

define unity-finish

ifneq ($$(acc),)
UNITIES := $$(UNITIES) $$(TARGET_DIR)/unities/unity$$(words $$(UNITIES)).c
endif

endef

# Create a rule for building the unity source file from its dependencies
define unity-src
$1:
	mkdir -p $$(dir $$@)
	rm -f $$@
	$$(foreach f, $$^, echo \#include \"$$f\" >>$$@ &&) true
endef


# $1 list of all source files
define make-unities
$(eval $(foreach a, $1, $(call unity, $(a))))
$(eval $(call unity-finish))
endef

# Make the rules for the unity source files
#
# $1 the list of all unities
define make-unity-rules
$(foreach u, $1,$(eval $(call unity-src, $(u))))
endef
