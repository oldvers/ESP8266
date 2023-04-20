#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_SRCDIRS := \
    . \
	east \
	block_queue \
	wifi \
	udp \
	led_strip \
	httpd \
	http_servr \
	http_server/fsdata

COMPONENT_ADD_INCLUDEDIRS := \
    . \
	east/include \
	block_queue/include \
	wifi/include \
	udp/include \
	led_strip/include \
	httpd \
	http_server \
	http_server/fsdata