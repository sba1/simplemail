#
# Some common macros for the version information
#

# The commit identifier that is compiled into SimpleMail
COMMITID := '"$(shell (git rev-list HEAD -n 1 | cut -c1-12) || echo unknown commit)"'

# Some other identifiers used as tags for the archive name
COMMITS := $(shell git rev-list HEAD --count)
VERSION_TAG := $(subst SNAPSHOT,-$(shell date "+%Y%m%d")-$(shell printf %06d $(COMMITS)),$(shell cat VERSION-TAG))
