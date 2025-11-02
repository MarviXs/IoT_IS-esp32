idf_component_register( SRCS "ota.c" "semver_utils.c"
                        PRIV_REQUIRES esp_https_ota app_update esp_http_client json
                        INCLUDE_DIRS "."
                        EMBED_TXTFILES "cacert.pem")