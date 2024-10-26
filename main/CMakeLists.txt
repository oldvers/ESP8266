set( include_dirs
     "."
     "east/include"
     "block_queue/include"
     "wifi/include"
     "udp/include"
     "led_strip/include"
     "http/daemon/include"
     "http/server/include"
     "http/server/fsdata"
     "time/include" )

set( srcs
     "main.c"
     "east/east_packet.c"
     "east/east_task.c"
     "east/east_uart.c"
     "block_queue/block_queue.c"
     "wifi/wifi_task.c"
     "udp/udp_task.c"
     "led_strip/led_strip.c"
     "led_strip/led_task.c"
     "http/daemon/fs.c"
     "http/daemon/httpd.c"
     "http/daemon/strcasestr.c"
     "http/server/http_server.c"
     "udp/udp_dns_server.c"
     "time/time_task.c" )

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    add_definitions("-DLWIP_HTTPD_CGI=1")
    add_definitions("-DLWIP_HTTPD_SSI=1")
    add_definitions("-DHTTPD_DEBUG=LWIP_DBG_ON")
endif()

idf_component_register( SRCS "${srcs}"
                        INCLUDE_DIRS "${include_dirs}"
                        REQUIRES
                        PRIV_REQUIRES )
