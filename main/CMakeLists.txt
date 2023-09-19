set( include_dirs
     "."
     "east/include"
     "block_queue/include"
     "wifi/include"
     "udp/include"
     "led_strip/include"
     "httpd"
     "http_server/fsdata"
     "http_server" )

set( srcs
     "main.c"
     "east/east_packet.c"
     "east/east_task.c"
     "east/east_uart.c"
     "block_queue/block_queue.c"
     "wifi/wifi_task.c"
     "udp/udp_task.c"
     "led_strip/led_strip.c"
     "httpd/fs.c"
     "httpd/httpd.c"
     "httpd/strcasestr.c"
     "http_server/http_server.c"
     "udp/udp_dns_server.c" )

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    add_definitions("-DLWIP_HTTPD_CGI=1")
    add_definitions("-DLWIP_HTTPD_SSI=1")
    add_definitions("-DLWIP_DEBUG=1")
    add_definitions("-DHTTPD_DEBUG=LWIP_DBG_ON")
endif()

idf_component_register( SRCS "${srcs}"
                        INCLUDE_DIRS "${include_dirs}"
                        REQUIRES
                        PRIV_REQUIRES )
