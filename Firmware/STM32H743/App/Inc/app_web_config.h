#ifndef AURA_APP_WEB_CONFIG_H
#define AURA_APP_WEB_CONFIG_H
/* Enable only after providing the deployed API host, device-scoped token and
   its actual root CA PEM. No credentials or invented service URLs ship here. */
#ifndef APP_WEB_ENABLED
#define APP_WEB_ENABLED 0
#endif
#ifndef APP_WEB_HOST
#define APP_WEB_HOST ""
#endif
#ifndef APP_WEB_DEVICE_TOKEN
#define APP_WEB_DEVICE_TOKEN ""
#endif
#ifndef APP_WEB_ROOT_CA_PEM
#define APP_WEB_ROOT_CA_PEM ""
#endif
#define APP_WEB_POLL_SECONDS 2U
#endif
