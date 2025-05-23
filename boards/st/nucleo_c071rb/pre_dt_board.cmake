# Suppress "unique_unit_address_if_enabled" to handle the overlaps in clock trees
list(APPEND EXTRA_DTC_FLAGS "-Wno-unique_unit_address_if_enabled")
