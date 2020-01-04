set(COMPONENT_ADD_INCLUDEDIRS .)

set(COMPONENT_SRCS  "src/led.c"
                    "src/mqtt.c"
                    "src/lora.c"
                    "src/wifi_manager.c"
                    "src/json.c"
                    "src/http_server.c"
                    "src/heart_beat.c"
                    "src/cJSON.c")

set(COMPONENT_ADD_INCLUDEDIRS "include")

register_component()
