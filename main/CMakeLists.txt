set(main_requires
    app_claw
    app_config
    captive_dns
    cmd_espnow
    espnow_slave_control
    http_server
    wifi_manager
    cmd_wifi
    espnow_link
)

if(CONFIG_APP_CLAW_CAP_IM_WECHAT)
    list(APPEND main_requires cap_im_wechat)
endif()

idf_component_register(
    SRCS
        "main.c"
    REQUIRES
        ${main_requires}
)
