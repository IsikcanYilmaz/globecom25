APPLICATION = cache_perf 

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../..

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# CFLAGS += -DLOG_LEVEL=LOG_NONE  # disable log output
# CFLAGS += -DCONFIG_GNRC_NETIF_IPV6_ADDRS_NUMOF=2
# CFLAGS += -DGNRC_NETIF_IPV6_GROUPS_NUMOF=2 
# CFLAGS += -DCONFIG_GNRC_IPV6_NIB_NUMOF=1
# CFLAGS += -DCONFIG_GNRC_IPV6_NIB_OFFL_NUMOF=1 # be able to configure at least one route

# DEMO FLAGS
# CFLAGS += -DJON_RSSI_LIMITING=-100
# CFLAGS += -DJON_DEFAULT_TX_POWER=-20
# CFLAGS += -DLED0_TO_SHOW_RX
# CFLAGS += -DLED1_TO_SHOW_TX
# CFLAGS += -DLED2_TO_SHOW_GOOD_RX

# CFLAGS += -DNEOPIXEL_SHELL_COMMANDS
# CFLAGS += -DTHROTTLERS_SHELL_COMMANDS

# CFLAGS += -DJON_RELAYS_CODE_AND_CACHE

# Set GNRC_PKTBUF_SIZE via CFLAGS if not being set via Kconfig.
ifndef CONFIG_GNRC_PKTBUF_SIZE
  CFLAGS += -DCONFIG_GNRC_PKTBUF_SIZE=512
endif

# 802154 CONFIG # Found in sys/include/net/ieee802154.h
# CFLAGS += -DCONFIG_IEEE802154_DEFAULT_MAX_FRAME_RETRANS=0 # 0 to disable L2 retransmissions
# ifeq ("seeedstudio-xiao-nrf52840", $(BOARD))
# CFLAGS += -DJON_802154_MAX_RETRANS=4
# endif

# WATCHDOG CONFIG
CFLAGS += -DCONFIG_PERIPH_WDT_WIN_MAX_MS=10000 # 10 seconds watchdog time

CFLAGS += -DDEBUG_ASSERT_VERBOSE

###########

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

USEMODULE += ps
USEMODULE += shell_cmds_default
USEMODULE += ztimer
USEMODULE += auto_init_wdt_thread

EXTERNAL_MODULE_DIRS += $(CURDIR)/../../submodules/
USEMODULE += button
# ifeq ("seeedstudio-xiao-nrf52840", $(BOARD))
# USEMODULE += neopixel
# endif
USEMODULE += onboard_leds
USEMODULE += throttler
USEMODULE += udp_stress
USEMODULE += caching_iperf
# USEMODULE += udp_multihop


# CFLAGS += -I$(EXTERNAL_MODULE_DIRS)/common/
CFLAGS += -I$(EXTERNAL_MODULE_DIRS)/button/
CFLAGS += -I$(EXTERNAL_MODULE_DIRS)/neopixel/
CFLAGS += -I$(EXTERNAL_MODULE_DIRS)/onboard_leds/
CFLAGS += -I$(EXTERNAL_MODULE_DIRS)/throttler/
CFLAGS += -I$(EXTERNAL_MODULE_DIRS)/caching_iperf/

# USEPKG += ccn-lite

include $(RIOTBASE)/Makefile.include
